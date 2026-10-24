#include "ag_journal_history.h"
#include <string.h>
enum { BLANK=0, VALID=1, DIRTY=2, IO=3 };
static const uint8_t hm[4]={'J','H','0','1'},em[4]={'J','E','0','1'},fm[4]={'J','I','0','1'};
static void p32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void p64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t g32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)p[i]<<(8*i);return n;}
static uint64_t g64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=(uint64_t)p[i]<<(8*i);return n;}
static uint32_t crc(uint32_t n,const uint8_t *p,size_t count){for(size_t i=0;i<count;++i){n^=p[i];for(unsigned b=0;b<8;++b)n=(n>>1)^((n&1)?UINT32_C(0xedb88320):0);}return n;}
static bool all(const uint8_t *p,size_t n,uint8_t v){for(size_t i=0;i<n;++i)if(p[i]!=v)return false;return true;}
static bool sid_ok(const uint8_t *s){return s!=NULL&&!all(s,16,0);}
static bool eqsid(const uint8_t *a,const uint8_t *b){return memcmp(a,b,16)==0;}
static unsigned index_of(const ag_journal_history_t *s,uint64_t gen){return (unsigned)((gen-1)%s->sectors);}
static int bitmap_count(const uint8_t *p){
    uint32_t value=g32(p);if(value!=g32(p+4))return -1;
    for(unsigned n=0;n<=30;++n){uint32_t expected=~((UINT32_C(1)<<n)-1);if(value==expected)return (int)n;}
    return -1;
}
static uint32_t header_crc(const uint8_t *b){return crc(UINT32_MAX,b,112)^UINT32_MAX;}
static uint32_t footer_crc(const uint8_t *b){uint32_t c=crc(UINT32_MAX,b,88);c=crc(c,b+96,20);return crc(c,b+120,4)^UINT32_MAX;}
static uint8_t flags(const ag_journal_info_t *i){return (uint8_t)((i->has_samples?1:0)|(i->sequence_exhausted?2:0)|(i->serial_exhausted?4:0));}
static void unflags(ag_journal_info_t *i,uint8_t f){i->has_samples=(f&1)!=0;i->sequence_exhausted=(f&2)!=0;i->serial_exhausted=(f&4)!=0;}
static void header_encode(const ag_journal_history_t *s,const ag_journal_info_t *i,uint8_t b[128]){
    memset(b,0,128);memcpy(b,"AGJH",4);b[4]=1;p64(b+8,i->generation);p64(b+16,i->next_serial);
    p64(b+24,i->initial_serial);p64(b+32,i->evicted_samples);p64(b+40,i->retained_first_serial);memcpy(b+48,i->session_id,16);
    p64(b+64,i->next_sequence);p64(b+72,i->last_ms);p64(b+80,i->initial_sequence);b[88]=flags(i);
    p64(b+96,i->generation-1);p32(b+104,index_of(s,i->generation));p32(b+108,s->sectors);
    memset(b+112,0xff,8);p32(b+120,header_crc(b));memcpy(b+124,hm,4);
}
static bool header_decode(const ag_journal_history_t *s,unsigned index,const uint8_t b[128],ag_journal_info_t *i){
    if(memcmp(b,"AGJH",4)!=0||b[4]!=1||!all(b+5,3,0)||!all(b+89,7,0)
        ||b[88]>7||memcmp(b+124,hm,4)!=0||g32(b+120)!=header_crc(b))return false;
    memset(i,0,sizeof(*i));i->generation=g64(b+8);i->next_serial=g64(b+16);i->initial_serial=g64(b+24);
    i->evicted_samples=g64(b+32);i->retained_first_serial=g64(b+40);memcpy(i->session_id,b+48,16);
    i->next_sequence=g64(b+64);i->last_ms=g64(b+72);i->initial_sequence=g64(b+80);unflags(i,b[88]);
    if(!sid_ok(i->session_id)||i->generation==0||i->initial_serial==0||i->next_serial<i->initial_serial
        ||i->initial_sequence==0||i->next_sequence<i->initial_sequence||i->serial_exhausted
        ||g64(b+96)!=i->generation-1||g32(b+104)!=index||g32(b+108)!=s->sectors||index_of(s,i->generation)!=index)return false;
    uint64_t delta=i->next_serial-i->initial_serial;
    if(delta%30!=0||delta/30+1!=i->generation)return false;
    uint64_t preceding=i->generation-1<(uint64_t)(s->sectors-1)?i->generation-1:(uint64_t)(s->sectors-1);
    if(i->retained_first_serial!=i->next_serial-preceding*30||i->evicted_samples>i->retained_first_serial-i->initial_serial)return false;
    return i->has_samples ? (i->sequence_exhausted?i->next_sequence==UINT64_MAX:i->next_sequence>i->initial_sequence)
        : !i->sequence_exhausted&&i->last_ms==0&&i->next_sequence==i->initial_sequence;
}
static bool state_equal(const ag_journal_history_t *s,const ag_journal_info_t *a,const ag_journal_info_t *b){
    uint8_t x[128],y[128];header_encode(s,a,x);header_encode(s,b,y);return memcmp(x,y,128)==0;
}
static bool valid_sample(const ag_history_record_t *r){return r->sequence!=0&&r->pid!=0&&r->ecu_source<=0x1fffffff
    &&r->status>=AG_SAMPLE_VALID&&r->status<=AG_SAMPLE_UNAVAILABLE&&r->raw_length<=64
    &&((r->status==AG_SAMPLE_VALID)==(r->raw_length>0));}
static void entry_encode(const uint8_t sid[16],uint8_t kind,const ag_history_record_t *r,uint64_t serial,uint8_t b[128]){
    memset(b,0,128);memcpy(b,"AGJE",4);b[4]=kind;b[5]=(uint8_t)r->status;b[6]=r->pid;b[7]=r->raw_length;
    memcpy(b+8,sid,16);p64(b+24,serial);p64(b+32,r->sequence);p64(b+40,r->monotonic_ms);p32(b+48,r->ecu_source);
    memcpy(b+52,r->raw,r->raw_length);uint32_t c=crc(UINT32_MAX,b,116);p32(b+116,crc(c,b+120,4)^UINT32_MAX);memcpy(b+124,em,4);
}
static bool entry_decode(const uint8_t b[128],uint8_t sid[16],uint8_t *kind,ag_history_record_t *r,uint64_t *serial){
    uint32_t c=crc(UINT32_MAX,b,116);c=crc(c,b+120,4)^UINT32_MAX;
    if(memcmp(b,"AGJE",4)!=0||(b[4]!=1&&b[4]!=2)||b[7]>64||!all(b+120,4,0)||g32(b+116)!=c
        ||memcmp(b+124,em,4)!=0||!sid_ok(b+8)||!all(b+52+b[7],64-b[7],0))return false;
    memset(r,0,sizeof(*r));memcpy(sid,b+8,16);*kind=b[4];*serial=g64(b+24);r->sequence=g64(b+32);r->monotonic_ms=g64(b+40);
    r->ecu_source=g32(b+48);r->status=(ag_sample_status_t)b[5];r->pid=b[6];r->raw_length=b[7];memcpy(r->raw,b+52,r->raw_length);
    return *serial!=0&&(*kind==1?valid_sample(r):(r->sequence!=0&&r->monotonic_ms==0&&r->ecu_source==0&&r->pid==0&&r->raw_length==0&&r->status==AG_SAMPLE_VALID));
}
static bool replay(ag_journal_info_t *i,const uint8_t sid[16],uint8_t kind,const ag_history_record_t *r,uint64_t serial){
    if(i->serial_exhausted||serial!=i->next_serial)return false;
    if(kind==1){if(!eqsid(sid,i->session_id)||i->sequence_exhausted||r->sequence!=i->next_sequence||r->monotonic_ms<i->last_ms)return false;
        i->last_ms=r->monotonic_ms;i->has_samples=true;if(i->next_sequence==UINT64_MAX)i->sequence_exhausted=true;else ++i->next_sequence;
    }else{if(eqsid(sid,i->session_id))return false;memcpy(i->session_id,sid,16);i->initial_sequence=i->next_sequence=r->sequence;
        i->last_ms=0;i->has_samples=i->sequence_exhausted=false;}
    if(i->next_serial==UINT64_MAX)i->serial_exhausted=true;else ++i->next_serial;return true;
}
static void footer_encode(const ag_journal_history_t *s,const ag_journal_info_t *end,uint8_t victim_samples,uint8_t b[128]){
    memset(b,0,128);memcpy(b,"AGJI",4);b[4]=1;p64(b+8,end->generation);p64(b+16,end->generation+1);p64(b+24,end->next_serial);
    uint64_t victim=end->generation>=s->sectors?end->generation-s->sectors+1:0;
    p64(b+32,victim);p64(b+40,victim?end->retained_first_serial:0);p64(b+48,victim_samples);
    p64(b+56,end->evicted_samples+victim_samples);p64(b+64,end->retained_first_serial+(victim?30:0));memcpy(b+72,end->session_id,16);
    memset(b+88,0xff,8);p32(b+96,(uint32_t)(end->generation%s->sectors));p32(b+100,index_of(s,end->generation));p32(b+104,s->sectors);b[108]=flags(end);
    uint8_t canonical[128];header_encode(s,end,canonical);p32(b+120,g32(canonical+120));
    p32(b+116,footer_crc(b));memcpy(b+124,fm,4);
}
static bool footer_decode(const ag_journal_history_t *s,const ag_journal_info_t *end,const uint8_t b[128],ag_journal_info_t *after){
    if(end->serial_exhausted||g64(b+48)>30||end->evicted_samples>UINT64_MAX-g64(b+48))return false;
    uint64_t victim=end->generation>=s->sectors?end->generation-s->sectors+1:0;
    if((!victim&&g64(b+48)!=0)||(victim&&end->retained_first_serial>UINT64_MAX-30))return false;
    uint8_t expected[128];footer_encode(s,end,(uint8_t)g64(b+48),expected);
    if(g32(b+116)!=footer_crc(b)||memcmp(b,expected,88)!=0||memcmp(b+96,expected+96,32)!=0)return false;
    *after=*end;after->evicted_samples=g64(b+56);after->retained_first_serial=g64(b+64);return true;
}
static bool rd(const ag_journal_history_t *s,uint32_t at,uint8_t *b,size_t n){return s->io.read(s->io.context,at,b,n)==n;}
static uint8_t erased(const ag_journal_history_t *s,uint32_t at,uint32_t length){uint8_t b[128];
    for(uint32_t p=0;p<length;p+=128){size_t n=length-p<128?length-p:128;if(!rd(s,at+p,b,n))return IO;if(!all(b,n,0xff))return DIRTY;}return BLANK;
}
typedef struct {ag_journal_info_t base,end,after;uint8_t count,samples,status;bool footer,witness;} scan_t;
static void scan_sector(const ag_journal_history_t *s,unsigned index,scan_t *out){
    memset(out,0,sizeof(*out));out->status=IO;uint8_t b[128];uint32_t start=index*4096;
    if(!rd(s,start,b,128))return;
    if(!header_decode(s,index,b,&out->base)){out->status=DIRTY;return;}
    int reserved=bitmap_count(b+112);out->end=out->base;bool tail=false,broken=false;
    for(unsigned n=0;n<30;++n){if(!rd(s,start+128+n*128,b,128))return;
        if(all(b,128,0xff)){tail=true;continue;}
        uint8_t sid[16],kind;ag_history_record_t r;uint64_t serial;
        if(tail||broken||!entry_decode(b,sid,&kind,&r,&serial)||!replay(&out->end,sid,kind,&r,serial)){broken=true;continue;}
        ++out->count;if(kind==1)++out->samples;
    }
    bool bitmap_bad=reserved<0||reserved!=(int)out->count;
    if(!rd(s,start+3968,b,128))return;
    if(!all(b,128,0xff)){
        if(broken||out->count!=30||!footer_decode(s,&out->end,b,&out->after))broken=true;
        else {out->footer=true;uint32_t a=g32(b+88),v=g32(b+92);out->witness=a!=UINT32_MAX||v!=UINT32_MAX;
            if(a!=v||(a!=UINT32_MAX&&a!=UINT32_MAX-1))broken=true;}
    }
    out->status=broken||bitmap_bad?DIRTY:VALID;
}
static ag_journal_result_t poison(ag_journal_history_t *s,ag_journal_result_t result){s->writable=s->writer_attached=false;if(result==AG_JOURNAL_IO_ERROR)s->ready=false;return result;}
ag_journal_result_t ag_journal_open(ag_journal_history_t *s,const ag_journal_io_t *io,uint16_t sectors){
    if(s==NULL)return AG_JOURNAL_INVALID;
    ag_journal_io_t copy={0};if(io!=NULL)copy=*io;memset(s,0,sizeof(*s));
    if(copy.read==NULL||copy.program==NULL||copy.erase_sector==NULL||sectors<2||sectors>512)return AG_JOURNAL_INVALID;
    s->io=copy;s->sectors=sectors;s->initialized=true;bool bad=false;uint64_t newest=0;unsigned found=0;
    for(unsigned index=0;index<sectors;++index){uint8_t b[128];ag_journal_info_t base;
        if(!rd(s,index*4096,b,128)){s->states[index]=IO;bad=true;continue;}
        if(all(b,128,0xff)){s->states[index]=erased(s,index*4096,4096);if(s->states[index]!=BLANK)bad=true;continue;}
        if(!header_decode(s,index,b,&base)){s->states[index]=DIRTY;bad=true;continue;}
        s->states[index]=VALID;s->generations[index]=base.generation;++found;
        if(base.generation>newest){newest=base.generation;s->current_index=(uint16_t)index;}
    }
    if(!found){s->blank=!bad;return s->blank?AG_JOURNAL_EMPTY:AG_JOURNAL_CORRUPT;}
    scan_t latest;scan_sector(s,s->current_index,&latest);s->info=latest.footer?latest.after:latest.end;
    if(latest.status!=VALID)bad=true;
    if(latest.footer&&latest.witness)bad=true; /* A reserved future header is absent/invalid: never recreate it. */
    s->pending_rollover=latest.footer;s->first_generation=newest>=sectors?newest-sectors+1:1;
    if(latest.footer&&newest>=sectors)++s->first_generation;
    scan_t previous;memset(&previous,0,sizeof(previous));bool has_previous=false;
    for(uint64_t gen=s->first_generation;gen<=newest;++gen){unsigned index=index_of(s,gen);
        if(s->states[index]!=VALID||s->generations[index]!=gen){bad=true;continue;}
        scan_t scan;scan_sector(s,index,&scan);s->counts[index]=scan.count;s->samples[index]=scan.samples;
        if(scan.status!=VALID||scan.base.initial_serial!=latest.base.initial_serial){bad=true;continue;}
        if(gen<newest&&(!scan.footer||!scan.witness||scan.count!=30))bad=true;
        if(has_previous){ag_journal_info_t expected=previous.after;expected.generation=gen;
            if(!previous.footer||!state_equal(s,&expected,&scan.base))bad=true;}
        previous=scan;has_previous=true;
    }
    for(unsigned index=0;index<sectors;++index){
        if(s->states[index]==BLANK)continue;
        uint64_t gen=s->generations[index];
        if(gen>=s->first_generation&&gen<=newest)continue;
        unsigned target=(unsigned)(newest%sectors);uint64_t victim=newest>=sectors?newest-sectors+1:0;
        if(latest.footer&&victim&&index==target&&s->states[index]==VALID&&gen==victim){
            scan_t scan;scan_sector(s,index,&scan);uint8_t footer[128];
            if(scan.status!=VALID||scan.count!=30||!rd(s,s->current_index*4096+3968,footer,128)
                ||scan.samples!=g64(footer+48)||scan.base.next_serial!=latest.end.retained_first_serial)bad=true;
        }else bad=true;
    }
    /* A missing retained header is not erased unused capacity. The ordered loop
     * above marks it degraded; only the committed intent's victim may be blank. */
    s->ready=true;s->writable=!bad;
    return s->writable?AG_JOURNAL_OK:AG_JOURNAL_READ_ONLY;
}
static bool pg(const ag_journal_history_t *s,uint32_t at,const uint8_t *b,size_t n){return s->io.program(s->io.context,at,b,n)==n;}
static bool write_marked(const ag_journal_history_t *s,uint32_t at,const uint8_t b[128]){
    uint8_t verify[128];return pg(s,at,b,124)&&pg(s,at+124,b+124,4)&&rd(s,at,verify,128)&&memcmp(b,verify,128)==0;
}
ag_journal_result_t ag_journal_bootstrap(ag_journal_history_t *s,const uint8_t sid[16],uint64_t seq,uint64_t serial){
    if(s==NULL||!s->initialized||!sid_ok(sid)||seq==0||serial==0)return AG_JOURNAL_INVALID;
    if(!s->blank||s->ready)return AG_JOURNAL_CORRUPT;
    if(erased(s,0,s->sectors*4096u)!=BLANK)return poison(s,AG_JOURNAL_IO_ERROR);
    ag_journal_info_t info={0};info.generation=1;info.next_serial=info.initial_serial=info.retained_first_serial=serial;
    info.next_sequence=info.initial_sequence=seq;memcpy(info.session_id,sid,16);uint8_t b[128];header_encode(s,&info,b);
    if(!write_marked(s,0,b))return poison(s,AG_JOURNAL_IO_ERROR);
    s->info=info;s->generations[0]=1;s->states[0]=VALID;s->current_index=0;s->first_generation=1;
    s->ready=s->writable=s->writer_attached=true;s->blank=false;return AG_JOURNAL_OK;
}
static ag_journal_result_t check_current(ag_journal_history_t *s){
    if(s==NULL||!s->initialized||!s->ready)return AG_JOURNAL_INVALID;
    if(!s->writable)return AG_JOURNAL_READ_ONLY;
    scan_t scan;scan_sector(s,s->current_index,&scan);
    if(scan.status==IO)return poison(s,AG_JOURNAL_IO_ERROR);
    if(scan.status!=VALID||scan.footer!=s->pending_rollover
        ||!state_equal(s,scan.footer?&scan.after:&scan.end,&s->info)||scan.count!=s->counts[s->current_index])
        return poison(s,AG_JOURNAL_READ_ONLY);
    return AG_JOURNAL_OK;
}
static ag_journal_result_t rollover(ag_journal_history_t *s){
    if(s->counts[s->current_index]<30)return AG_JOURNAL_OK;
    if(s->info.serial_exhausted)return AG_JOURNAL_EXHAUSTED;
    unsigned target=(unsigned)(s->info.generation%s->sectors);uint64_t victim=s->info.generation>=s->sectors?s->info.generation-s->sectors+1:0;
    uint8_t victim_samples=0;uint8_t target_state=s->states[target];
    if(target_state==VALID){scan_t old;scan_sector(s,target,&old);
        if(!victim||old.status!=VALID||old.base.generation!=victim||old.count!=30)return poison(s,AG_JOURNAL_READ_ONLY);
        victim_samples=old.samples;
    }else if(target_state!=BLANK)return poison(s,AG_JOURNAL_READ_ONLY);
    if(!s->pending_rollover){
        if(victim&&target_state==BLANK)return poison(s,AG_JOURNAL_READ_ONLY);
        if(s->info.evicted_samples>UINT64_MAX-victim_samples)return AG_JOURNAL_EXHAUSTED;
        uint8_t footer[128];footer_encode(s,&s->info,victim_samples,footer);ag_journal_info_t after;
        if(!footer_decode(s,&s->info,footer,&after))return poison(s,AG_JOURNAL_READ_ONLY);
        if(!write_marked(s,s->current_index*4096u+3968,footer))return poison(s,AG_JOURNAL_IO_ERROR);
        s->info=after;s->pending_rollover=true;
        if(victim)++s->first_generation; /* Durable LOGICAL eviction, before erase. */
    }
    /* Only the exact intact old victim or fully erased target is eligible.
     * No repair/erase of an unknown or partially programmed sector. */
    if(target_state==VALID){if(!s->io.erase_sector(s->io.context,target*4096u))return poison(s,AG_JOURNAL_IO_ERROR);}
    if(erased(s,target*4096u,4096)!=BLANK)return poison(s,AG_JOURNAL_IO_ERROR);
    uint8_t witness[8],check[8];p32(witness,UINT32_MAX-1);p32(witness+4,UINT32_MAX-1);
    uint32_t witness_at=s->current_index*4096u+3968+88;
    if(!pg(s,witness_at,witness,4)||!pg(s,witness_at+4,witness+4,4)||!rd(s,witness_at,check,8)||memcmp(witness,check,8)!=0)return poison(s,AG_JOURNAL_IO_ERROR);
    ag_journal_info_t next=s->info;++next.generation;uint8_t header[128];header_encode(s,&next,header);
    if(!write_marked(s,target*4096u,header))return poison(s,AG_JOURNAL_IO_ERROR);
    s->current_index=(uint16_t)target;s->info=next;s->generations[target]=next.generation;s->states[target]=VALID;
    s->counts[target]=s->samples[target]=0;s->pending_rollover=false;return AG_JOURNAL_OK;
}
static ag_journal_result_t append_entry(ag_journal_history_t *s,const uint8_t sid[16],uint8_t kind,const ag_history_record_t *r){
    ag_journal_result_t result=check_current(s);if(result!=AG_JOURNAL_OK)return result;
    if(s->info.serial_exhausted)return AG_JOURNAL_EXHAUSTED;
    result=rollover(s);if(result!=AG_JOURNAL_OK)return result;
    ag_journal_info_t next=s->info;if(!replay(&next,sid,kind,r,s->info.next_serial))return AG_JOURNAL_INVALID;
    unsigned reserved=(unsigned)s->counts[s->current_index]+1;
    uint32_t mask=~((UINT32_C(1)<<reserved)-1);uint8_t bits[8],check[8];p32(bits,mask);p32(bits+4,mask);
    uint32_t bitmap_at=s->current_index*4096u+112;
    if(!pg(s,bitmap_at,bits,4)||!pg(s,bitmap_at+4,bits+4,4)||!rd(s,bitmap_at,check,8)||memcmp(bits,check,8)!=0)return poison(s,AG_JOURNAL_IO_ERROR);
    uint8_t b[128];entry_encode(sid,kind,r,s->info.next_serial,b);
    uint32_t offset=s->current_index*4096u+128u+s->counts[s->current_index]*128u;
    if(!write_marked(s,offset,b))return poison(s,AG_JOURNAL_IO_ERROR);
    s->info=next;++s->counts[s->current_index];if(kind==1)++s->samples[s->current_index];return AG_JOURNAL_OK;
}
static ag_journal_result_t retained_sid(ag_journal_history_t *s,const uint8_t sid[16],bool *found){
    *found=false;
    for(uint64_t gen=s->first_generation;gen<=s->info.generation;++gen){unsigned index=index_of(s,gen);
        uint8_t header[128];ag_journal_info_t base;
        if(!rd(s,index*4096u,header,128))return poison(s,AG_JOURNAL_IO_ERROR);
        if(!header_decode(s,index,header,&base)||base.generation!=gen||bitmap_count(header+112)!=s->counts[index])return poison(s,AG_JOURNAL_READ_ONLY);
        if(eqsid(base.session_id,sid))*found=true;
        for(unsigned n=0;n<s->counts[index];++n){uint8_t b[128],stored[16],kind;ag_history_record_t r;uint64_t serial;
            if(!rd(s,index*4096u+128+n*128,b,128))return poison(s,AG_JOURNAL_IO_ERROR);
            if(!entry_decode(b,stored,&kind,&r,&serial)||serial!=base.next_serial+n)return poison(s,AG_JOURNAL_READ_ONLY);
            if(eqsid(stored,sid))*found=true;
        }
    }return AG_JOURNAL_OK;
}
ag_journal_result_t ag_journal_begin_session(ag_journal_history_t *s,const uint8_t sid[16],uint64_t first){
    if(!sid_ok(sid)||first==0)return AG_JOURNAL_INVALID;
    ag_journal_result_t checked=check_current(s);if(checked!=AG_JOURNAL_OK)return checked;
    if(eqsid(sid,s->info.session_id))return AG_JOURNAL_SESSION_REUSED;
    bool found;checked=retained_sid(s,sid,&found);if(checked!=AG_JOURNAL_OK)return checked;if(found)return AG_JOURNAL_SESSION_REUSED;
    ag_history_record_t control={0};control.sequence=first;ag_journal_result_t result=append_entry(s,sid,2,&control);
    if(result==AG_JOURNAL_OK)s->writer_attached=true;
    return result;
}
ag_journal_result_t ag_journal_append(ag_journal_history_t *s,uint8_t pid,uint32_t ecu,uint64_t ms,ag_sample_status_t status,const uint8_t *raw,size_t length){
    if(s==NULL||!s->ready)return AG_JOURNAL_INVALID;
    if(!s->writable)return AG_JOURNAL_READ_ONLY;
    if(!s->writer_attached)return AG_JOURNAL_NEW_SESSION_REQUIRED;
    if(length>64||(length&&raw==NULL))return AG_JOURNAL_INVALID;
    if(s->info.sequence_exhausted||s->info.serial_exhausted)return AG_JOURNAL_EXHAUSTED;
    if(ms<s->info.last_ms)return AG_JOURNAL_CLOCK_REGRESSION;
    ag_history_record_t r={0};r.sequence=s->info.next_sequence;r.monotonic_ms=ms;r.pid=pid;r.ecu_source=ecu;r.status=status;r.raw_length=(uint8_t)length;
    if(!valid_sample(&r))return AG_JOURNAL_INVALID;
    if(length)memcpy(r.raw,raw,length);
    uint8_t sid[16];memcpy(sid,s->info.session_id,16);return append_entry(s,sid,1,&r);
}
static ag_journal_result_t query(ag_journal_history_t *s,const uint8_t wanted[16],uint64_t after,ag_history_snapshot_t *snapshot){
    if(!sid_ok(wanted))return AG_JOURNAL_INVALID;
    ag_journal_result_t checked=check_current(s);if(checked!=AG_JOURNAL_OK)return checked;
    bool found=false,empty_seen=false,ended=false;uint64_t initial=1;ag_history_record_t last={0};
    if(snapshot!=NULL){memset(snapshot,0,sizeof(*snapshot));snapshot->capacity=16;memcpy(snapshot->session_id,wanted,16);}
    for(uint64_t gen=s->first_generation;gen<=s->info.generation;++gen){unsigned index=index_of(s,gen);uint8_t b[128];ag_journal_info_t base;
        if(!rd(s,index*4096u,b,128))return poison(s,AG_JOURNAL_IO_ERROR);
        if(!header_decode(s,index,b,&base)||base.generation!=gen||bitmap_count(b+112)!=s->counts[index])return poison(s,AG_JOURNAL_READ_ONLY);
        if(eqsid(base.session_id,wanted)&&!base.has_samples){empty_seen=true;initial=base.initial_sequence;}
        for(unsigned n=0;n<s->counts[index];++n){uint8_t sid[16],kind;ag_history_record_t r;uint64_t serial;
            if(!rd(s,index*4096u+128+n*128,b,128))return poison(s,AG_JOURNAL_IO_ERROR);
            if(!entry_decode(b,sid,&kind,&r,&serial)||serial!=base.next_serial+n)return poison(s,AG_JOURNAL_READ_ONLY);
            if(!eqsid(sid,wanted)){if(found)ended=true;continue;}
            if(kind==2){if(found||ended)return poison(s,AG_JOURNAL_READ_ONLY);empty_seen=true;initial=r.sequence;continue;}
            if(ended||(found&&(last.sequence==UINT64_MAX||r.sequence!=last.sequence+1||r.monotonic_ms<last.monotonic_ms)))return poison(s,AG_JOURNAL_READ_ONLY);
            found=true;last=r;
            if(snapshot!=NULL&&r.sequence>after&&snapshot->count<16)snapshot->records[snapshot->count++]=r;
        }
    }
    if(!found){
        if(eqsid(wanted,s->info.session_id)&&!s->info.has_samples){empty_seen=true;initial=s->info.initial_sequence;}
        if(!empty_seen)return AG_JOURNAL_NOT_RETAINED;
        if(after>initial-1)return AG_JOURNAL_CURSOR_AHEAD;
        if(snapshot!=NULL)snapshot->next_sequence=initial;
    }else{
        if(after>last.sequence)return AG_JOURNAL_CURSOR_AHEAD;
        if(snapshot!=NULL){if(snapshot->count==0)snapshot->records[snapshot->count++]=last;
            const ag_history_record_t *tail=&snapshot->records[snapshot->count-1];snapshot->last_ms=tail->monotonic_ms;
            snapshot->exhausted=tail->sequence==UINT64_MAX;snapshot->next_sequence=snapshot->exhausted?UINT64_MAX:tail->sequence+1;}
    }
    return AG_JOURNAL_OK;
}
ag_journal_result_t ag_journal_window(ag_journal_history_t *s,const uint8_t sid[16],uint64_t after,ag_history_snapshot_t *out){
    if(out==NULL)return AG_JOURNAL_INVALID;
    ag_history_snapshot_t candidate;ag_journal_result_t result=query(s,sid,after,&candidate);
    if(result==AG_JOURNAL_OK){if(!ag_history_snapshot_valid(&candidate))return poison(s,AG_JOURNAL_READ_ONLY);*out=candidate;}
    return result;
}
ag_journal_result_t ag_journal_make_page(ag_journal_history_t *s,const uint8_t sid[16],uint64_t after,size_t limit,char *out,size_t capacity,ag_history_page_info_t *info){
    if(out!=NULL&&capacity)out[0]=0;
    if(info!=NULL)memset(info,0,sizeof(*info));
    if(out==NULL||info==NULL||limit<1||limit>16)return AG_JOURNAL_INVALID;
    ag_history_snapshot_t snapshot;ag_journal_result_t result=ag_journal_window(s,sid,after,&snapshot);if(result!=AG_JOURNAL_OK)return result;
    ag_history_result_t encoded=ag_history_make_page(&snapshot,sid,after,limit,out,capacity,info);
    return encoded==AG_HISTORY_OK?AG_JOURNAL_OK:(encoded==AG_HISTORY_OUTPUT_SHORT?AG_JOURNAL_OUTPUT_SHORT:AG_JOURNAL_INVALID);
}
ag_journal_result_t ag_journal_acknowledge(ag_journal_history_t *s,const uint8_t sid[16],uint64_t through){return query(s,sid,through,NULL);}
