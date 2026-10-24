#include "ag_flash_history.h"
#include <string.h>
static const uint8_t bank_marker[4]={'H','C','M','1'},record_marker[4]={'R','L','0','1'};
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void put64(uint8_t *p,uint64_t n){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)p[i]<<(8*i);return n;}
static uint64_t get64(const uint8_t *p){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=(uint64_t)p[i]<<(8*i);return n;}
static uint32_t crc_add(uint32_t n,const uint8_t *p,size_t count){
    for(size_t i=0;i<count;++i){n^=p[i];for(unsigned b=0;b<8;++b)n=(n>>1)^((n&1)?UINT32_C(0xedb88320):0);}return n;
}
static bool bytes_are(const uint8_t *p,size_t n,uint8_t value){for(size_t i=0;i<n;++i)if(p[i]!=value)return false;return true;}
static bool sid_valid(const uint8_t *sid){return sid!=NULL && !bytes_are(sid,16,0);}
static bool same_sid(const uint8_t *a,const uint8_t *b){return memcmp(a,b,16)==0;}
static bool valid_record(const ag_history_record_t *r){
    return r->sequence!=0 && r->pid!=0 && r->ecu_source<=0x1fffffff && r->raw_length<=64
        && r->status>=AG_SAMPLE_VALID && r->status<=AG_SAMPLE_UNAVAILABLE
        && ((r->status==AG_SAMPLE_VALID)==(r->raw_length>0));
}
static void encode_header(const ag_flash_history_info_t *i,uint8_t out[128]){
    memset(out,0,128);memcpy(out,"AGHS",4);out[4]=1;out[6]=128;
    put64(out+8,i->generation);memcpy(out+16,i->current_session,16);
    put32(out+32,i->capacity);put32(out+36,i->count);put64(out+40,i->first_serial);
    put64(out+48,i->next_serial);put64(out+56,i->current_next_sequence);put64(out+64,i->current_last_ms);
    put64(out+72,i->overwritten);out[80]=(uint8_t)i->serial_exhausted;out[81]=(uint8_t)i->current_exhausted;
    out[82]=(uint8_t)i->current_has_records;put64(out+88,i->initial_serial);put64(out+96,i->current_initial_sequence);
    put32(out+124,crc_add(UINT32_MAX,out,124)^UINT32_MAX);
}
static bool decode_header(const uint8_t b[128],uint32_t capacity,ag_flash_history_info_t *i){
    if(memcmp(b,"AGHS",4)!=0 || b[4]!=1 || b[5]!=0 || b[6]!=128 || b[7]!=0
        || get32(b+124)!=(crc_add(UINT32_MAX,b,124)^UINT32_MAX)
        || !bytes_are(b+83,5,0) || !bytes_are(b+104,20,0) || b[80]>1 || b[81]>1 || b[82]>1)return false;
    memset(i,0,sizeof(*i));i->generation=get64(b+8);memcpy(i->current_session,b+16,16);
    i->capacity=get32(b+32);i->count=get32(b+36);i->first_serial=get64(b+40);i->next_serial=get64(b+48);
    i->current_next_sequence=get64(b+56);i->current_last_ms=get64(b+64);i->overwritten=get64(b+72);
    i->serial_exhausted=b[80]!=0;i->current_exhausted=b[81]!=0;i->current_has_records=b[82]!=0;
    i->initial_serial=get64(b+88);i->current_initial_sequence=get64(b+96);
    if(!sid_valid(i->current_session) || i->generation==0 || i->capacity!=capacity || i->count>capacity
        || i->initial_serial==0 || i->first_serial<i->initial_serial || i->next_serial==0
        || i->current_initial_sequence==0 || i->current_next_sequence<i->current_initial_sequence
        || i->overwritten!=i->first_serial-i->initial_serial)return false;
    if(i->count==0)return !i->serial_exhausted && !i->current_has_records && !i->current_exhausted
        && i->first_serial==i->next_serial && i->first_serial==i->initial_serial && i->current_last_ms==0
        && i->current_next_sequence==i->current_initial_sequence;
    if(i->first_serial>UINT64_MAX-(i->count-1))return false;
    uint64_t last=i->first_serial+i->count-1;
    if(i->serial_exhausted ? (last!=UINT64_MAX || i->next_serial!=UINT64_MAX)
        : (last==UINT64_MAX || i->next_serial!=last+1))return false;
    return i->current_has_records || (!i->current_exhausted && i->current_last_ms==0
        && i->current_next_sequence==i->current_initial_sequence);
}
static void encode_record(const uint8_t sid[16],const ag_history_record_t *r,uint64_t serial,uint8_t out[128]){
    memset(out,0,128);memcpy(out,"AGLR",4);memcpy(out+4,sid,16);put64(out+20,r->sequence);
    put64(out+28,r->monotonic_ms);put64(out+36,serial);put32(out+44,r->ecu_source);
    out[48]=r->pid;out[49]=(uint8_t)r->status;out[50]=r->raw_length;memcpy(out+52,r->raw,r->raw_length);
    uint32_t crc=crc_add(UINT32_MAX,out,116);crc=crc_add(crc,out+120,4)^UINT32_MAX;put32(out+116,crc);
    memcpy(out+124,record_marker,4);
}
static bool decode_record(const uint8_t b[128],uint8_t sid[16],ag_history_record_t *r,uint64_t *serial){
    uint32_t crc=crc_add(UINT32_MAX,b,116);crc=crc_add(crc,b+120,4)^UINT32_MAX;
    if(memcmp(b,"AGLR",4)!=0 || memcmp(b+124,record_marker,4)!=0 || get32(b+116)!=crc
        || !sid_valid(b+4) || b[50]>64 || b[51]!=0 || !bytes_are(b+120,4,0)
        || !bytes_are(b+52+b[50],64-b[50],0))return false;
    memset(r,0,sizeof(*r));memcpy(sid,b+4,16);r->sequence=get64(b+20);r->monotonic_ms=get64(b+28);
    *serial=get64(b+36);r->ecu_source=get32(b+44);r->pid=b[48];r->status=(ag_sample_status_t)b[49];
    r->raw_length=b[50];memcpy(r->raw,b+52,r->raw_length);return *serial!=0 && valid_record(r);
}
static bool read_bytes(const ag_flash_history_t *s,uint32_t offset,uint8_t *b,size_t n){return s->io.read(s->io.context,offset,b,n)==n;}
static bool read_record(const ag_flash_history_t *s,unsigned bank,uint32_t index,uint8_t sid[16],ag_history_record_t *r,uint64_t *serial){
    uint8_t b[128];return read_bytes(s,bank*s->bank_bytes+128+index*128,b,128) && decode_record(b,sid,r,serial);
}
typedef enum { BANK_BLANK=0,BANK_VALID,BANK_DIRTY,BANK_IO } bank_state_t;
typedef struct {bank_state_t state;ag_flash_history_info_t info;uint8_t header[128];} bank_info_t;
static bank_state_t blank_region(const ag_flash_history_t *s,uint32_t base,uint32_t n){
    uint8_t b[128];for(uint32_t at=0;at<n;){size_t chunk=n-at<128?n-at:128;
        if(!read_bytes(s,base+at,b,chunk))return BANK_IO;
        if(!bytes_are(b,chunk,0xff))return BANK_DIRTY;
        at+=(uint32_t)chunk;
    }return BANK_BLANK;
}
static void scan_bank(const ag_flash_history_t *s,unsigned bank,bank_info_t *out,bool committed){
    memset(out,0,sizeof(*out));out->state=BANK_IO;uint32_t base=bank*s->bank_bytes;uint8_t marker[4];
    if(!read_bytes(s,base,out->header,128) || !read_bytes(s,base+s->bank_bytes-4,marker,4))return;
    if(memcmp(marker,committed?bank_marker:(const uint8_t *)"\xff\xff\xff\xff",4)!=0
        || !decode_header(out->header,s->capacity,&out->info)){
        out->state=blank_region(s,base,s->bank_bytes);return;
    }
    uint8_t previous_sid[16]={0},sid[16];ag_history_record_t previous={0},r={0};uint64_t serial;
    bool current_run=false;
    for(uint32_t n=0;n<out->info.count;++n){
        uint8_t bytes[128];
        if(!read_bytes(s,base+128+n*128,bytes,128)){out->state=BANK_IO;return;}
        if(!decode_record(bytes,sid,&r,&serial) || serial!=out->info.first_serial+n
            || (n>0 && same_sid(sid,previous_sid) && (previous.sequence==UINT64_MAX
                || r.sequence!=previous.sequence+1 || r.monotonic_ms<previous.monotonic_ms))){out->state=BANK_DIRTY;return;}
        if(same_sid(sid,out->info.current_session)){
            if(!out->info.current_has_records || r.sequence<out->info.current_initial_sequence){out->state=BANK_DIRTY;return;}
            current_run=true;
        }else if(current_run){out->state=BANK_DIRTY;return;}
        memcpy(previous_sid,sid,16);previous=r;
    }
    if(out->info.current_has_records){
        if(out->info.count==0 || !same_sid(previous_sid,out->info.current_session)
            || previous.monotonic_ms!=out->info.current_last_ms
            || (out->info.current_exhausted ? (previous.sequence!=UINT64_MAX || out->info.current_next_sequence!=UINT64_MAX)
                : (previous.sequence==UINT64_MAX || out->info.current_next_sequence!=previous.sequence+1))){out->state=BANK_DIRTY;return;}
    }else if(out->info.count>0 && same_sid(previous_sid,out->info.current_session)){out->state=BANK_DIRTY;return;}
    uint32_t used=128+out->info.count*128;
    out->state=blank_region(s,base+used,s->bank_bytes-4-used);
    if(out->state==BANK_BLANK)out->state=BANK_VALID;
}
static bool same_banks(const ag_flash_history_t *s,const bank_info_t *a,const bank_info_t *b){
    if(memcmp(a->header,b->header,128)!=0)return false;
    uint8_t x[128],y[128];for(uint32_t n=0;n<a->info.count;++n)
        if(!read_bytes(s,128+n*128,x,128) || !read_bytes(s,s->bank_bytes+128+n*128,y,128) || memcmp(x,y,128)!=0)return false;
    return true;
}
ag_flash_history_result_t ag_flash_history_open(ag_flash_history_t *s,const ag_flash_history_io_t *io,uint32_t total,uint32_t capacity){
    if(s==NULL)return AG_FLASH_HISTORY_INVALID;
    ag_flash_history_io_t copy={0};if(io!=NULL)copy=*io;memset(s,0,sizeof(*s));
    if(copy.read==NULL || copy.program==NULL || copy.erase_sector==NULL || total<8192
        || total>AG_FLASH_HISTORY_MAX_BYTES || total%(2*AG_FLASH_HISTORY_SECTOR)!=0 || capacity==0
        || capacity>(total/2-132)/128)return AG_FLASH_HISTORY_INVALID;
    s->io=copy;s->total_bytes=total;s->bank_bytes=total/2;s->capacity=capacity;s->initialized=true;
    bank_info_t banks[2];scan_bank(s,0,&banks[0],true);scan_bank(s,1,&banks[1],true);
    if(banks[0].state!=BANK_VALID && banks[1].state!=BANK_VALID){
        s->blank=banks[0].state==BANK_BLANK && banks[1].state==BANK_BLANK;
        return s->blank?AG_FLASH_HISTORY_EMPTY:(banks[0].state==BANK_IO || banks[1].state==BANK_IO
            ? AG_FLASH_HISTORY_IO_ERROR:AG_FLASH_HISTORY_CORRUPT);
    }
    unsigned chosen=banks[1].state==BANK_VALID && (banks[0].state!=BANK_VALID || banks[1].info.generation>banks[0].info.generation)?1:0;
    if(banks[0].state==BANK_VALID && banks[1].state==BANK_VALID){
        const ag_flash_history_info_t *newer=&banks[chosen].info,*older=&banks[1-chosen].info;
        if((newer->generation==older->generation && !same_banks(s,&banks[0],&banks[1]))
            || newer->next_serial<older->next_serial || newer->first_serial<older->first_serial
            || newer->overwritten<older->overwritten || (older->serial_exhausted && !newer->serial_exhausted)
            || newer->initial_serial!=older->initial_serial)return AG_FLASH_HISTORY_CONFLICT;
        if(same_sid(newer->current_session,older->current_session)
            && (newer->current_next_sequence<older->current_next_sequence
                || newer->current_last_ms<older->current_last_ms
                || newer->current_initial_sequence!=older->current_initial_sequence
                || (older->current_exhausted && !newer->current_exhausted)))return AG_FLASH_HISTORY_CONFLICT;
    }
    s->info=banks[chosen].info;s->active_bank=chosen;s->ready=true;
    s->writable=(banks[0].state==BANK_VALID || banks[0].state==BANK_BLANK)
        && (banks[1].state==BANK_VALID || banks[1].state==BANK_BLANK);
    return s->writable?AG_FLASH_HISTORY_OK:AG_FLASH_HISTORY_READ_ONLY;
}
static ag_flash_history_result_t failed(ag_flash_history_t *s){s->ready=s->writable=s->writer_attached=false;return AG_FLASH_HISTORY_IO_ERROR;}
static bool program(const ag_flash_history_t *s,uint32_t at,const uint8_t *b,size_t n){return s->io.program(s->io.context,at,b,n)==n;}
/* Whole inactive-bank COW: deliberately O(capacity), not a production flash log. */
static ag_flash_history_result_t commit(ag_flash_history_t *s,const ag_flash_history_info_t *info,
                                       const ag_history_record_t *addition,uint32_t skip,bool bootstrap){
    unsigned target=bootstrap?0:1-s->active_bank;uint32_t base=target*s->bank_bytes;
    if(!bootstrap){for(uint32_t at=0;at<s->bank_bytes;at+=AG_FLASH_HISTORY_SECTOR)
        if(!s->io.erase_sector(s->io.context,base+at))return failed(s);}
    if(blank_region(s,base,s->bank_bytes)!=BANK_BLANK)return failed(s);
    uint8_t header[128];encode_header(info,header);if(!program(s,base,header,128))return failed(s);
    uint32_t copied=bootstrap?0:s->info.count-skip;
    for(uint32_t n=0;n<info->count;++n){
        uint8_t bytes[128];
        if(n<copied){
            if(!read_bytes(s,s->active_bank*s->bank_bytes+128+(n+skip)*128,bytes,128))return failed(s);
            uint8_t sid[16];ag_history_record_t record;uint64_t serial;
            if(!decode_record(bytes,sid,&record,&serial) || serial!=info->first_serial+n)return failed(s);
        }else{
            if(addition==NULL)return failed(s);
            encode_record(info->current_session,addition,info->serial_exhausted?UINT64_MAX:info->next_serial-1,bytes);
        }
        if(!program(s,base+128+n*128,bytes,124) || !program(s,base+128+n*128+124,record_marker,4))return failed(s);
    }
    bank_info_t check;scan_bank(s,target,&check,false);
    if(check.state!=BANK_VALID || memcmp(check.header,header,128)!=0)return failed(s);
    if(!program(s,base+s->bank_bytes-4,bank_marker,4))return failed(s);
    scan_bank(s,target,&check,true);
    if(check.state!=BANK_VALID || memcmp(check.header,header,128)!=0)return failed(s);
    s->info=*info;s->active_bank=target;s->ready=s->writable=s->writer_attached=true;s->blank=false;
    return AG_FLASH_HISTORY_OK;
}
ag_flash_history_result_t ag_flash_history_bootstrap(ag_flash_history_t *s,const uint8_t sid[16],uint64_t seq,uint64_t serial){
    if(s==NULL || !s->initialized || !sid_valid(sid) || seq==0 || serial==0)return AG_FLASH_HISTORY_INVALID;
    if(!s->blank || s->ready)return AG_FLASH_HISTORY_CONFLICT;
    if(blank_region(s,0,s->total_bytes)!=BANK_BLANK)return failed(s);
    ag_flash_history_info_t info={0};info.generation=1;info.capacity=s->capacity;
    info.first_serial=info.next_serial=info.initial_serial=serial;
    info.current_next_sequence=info.current_initial_sequence=seq;memcpy(info.current_session,sid,16);
    return commit(s,&info,NULL,0,true);
}
static ag_flash_history_result_t media_check(ag_flash_history_t *s){
    if(s==NULL || !s->initialized || !s->ready)return AG_FLASH_HISTORY_INVALID;
    if(!s->writable)return AG_FLASH_HISTORY_READ_ONLY;
    ag_flash_history_t fresh;
    ag_flash_history_result_t result=ag_flash_history_open(&fresh,&s->io,s->total_bytes,s->capacity);
    if(result!=AG_FLASH_HISTORY_OK){s->ready=false;s->writable=s->writer_attached=false;return result;}
    uint8_t a[128],b[128];encode_header(&s->info,a);encode_header(&fresh.info,b);
    if(memcmp(a,b,128)!=0 || s->active_bank!=fresh.active_bank){s->ready=s->writable=s->writer_attached=false;return AG_FLASH_HISTORY_CONFLICT;}
    return AG_FLASH_HISTORY_OK;
}
ag_flash_history_result_t ag_flash_history_begin_session(ag_flash_history_t *s,const uint8_t sid[16],uint64_t first){
    if(!sid_valid(sid) || first==0)return AG_FLASH_HISTORY_INVALID;
    ag_flash_history_result_t result=media_check(s);if(result!=AG_FLASH_HISTORY_OK)return result;
    if(s->info.generation==UINT64_MAX || s->info.serial_exhausted)return AG_FLASH_HISTORY_EXHAUSTED;
    if(same_sid(sid,s->info.current_session))return AG_FLASH_HISTORY_SESSION_REUSED;
    for(uint32_t n=0;n<s->info.count;++n){uint8_t stored[16];ag_history_record_t r;uint64_t serial;
        if(!read_record(s,s->active_bank,n,stored,&r,&serial))return failed(s);
        if(same_sid(sid,stored))return AG_FLASH_HISTORY_SESSION_REUSED;
    }
    ag_flash_history_info_t info=s->info;++info.generation;memcpy(info.current_session,sid,16);
    info.current_next_sequence=info.current_initial_sequence=first;info.current_last_ms=0;
    info.current_exhausted=info.current_has_records=false;return commit(s,&info,NULL,0,false);
}
ag_flash_history_result_t ag_flash_history_append(ag_flash_history_t *s,uint8_t pid,uint32_t ecu,uint64_t now,
                                                  ag_sample_status_t status,const uint8_t *raw,size_t length){
    if(s==NULL || !s->initialized || !s->ready)return AG_FLASH_HISTORY_INVALID;
    if(!s->writer_attached)return AG_FLASH_HISTORY_NEW_SESSION_REQUIRED;
    if(length>64 || (length>0 && raw==NULL))return AG_FLASH_HISTORY_INVALID;
    ag_history_record_t r={0};r.sequence=s->info.current_next_sequence;r.monotonic_ms=now;
    r.pid=pid;r.ecu_source=ecu;r.status=status;r.raw_length=(uint8_t)length;
    if(!valid_record(&r))return AG_FLASH_HISTORY_INVALID;
    if(length)memcpy(r.raw,raw,length);
    ag_flash_history_result_t checked=media_check(s);if(checked!=AG_FLASH_HISTORY_OK)return checked;
    if(s->info.serial_exhausted || s->info.current_exhausted || s->info.generation==UINT64_MAX)return AG_FLASH_HISTORY_EXHAUSTED;
    if(now<s->info.current_last_ms)return AG_FLASH_HISTORY_CLOCK_REGRESSION;
    ag_flash_history_info_t info=s->info;++info.generation;uint32_t skip=0;
    if(info.count==info.capacity){skip=1;++info.first_serial;++info.overwritten;}else ++info.count;
    if(info.next_serial==UINT64_MAX)info.serial_exhausted=true;else ++info.next_serial;
    if(info.current_next_sequence==UINT64_MAX)info.current_exhausted=true;else ++info.current_next_sequence;
    info.current_has_records=true;info.current_last_ms=now;return commit(s,&info,&r,skip,false);
}
static ag_flash_history_result_t query(ag_flash_history_t *s,const uint8_t sid[16],uint64_t after,
                                      ag_history_snapshot_t *snapshot,uint64_t *latest){
    if(s==NULL || !s->ready || !sid_valid(sid))return AG_FLASH_HISTORY_INVALID;
    uint8_t header[128],expected[128],marker[4];encode_header(&s->info,expected);
    if(!read_bytes(s,s->active_bank*s->bank_bytes,header,128) || memcmp(header,expected,128)!=0
        || !read_bytes(s,(s->active_bank+1)*s->bank_bytes-4,marker,4)
        || memcmp(marker,bank_marker,4)!=0)return failed(s);
    bool found=false,ended=false;uint8_t stored[16];ag_history_record_t r,last={0};uint64_t serial;
    if(snapshot!=NULL){memset(snapshot,0,sizeof(*snapshot));memcpy(snapshot->session_id,sid,16);snapshot->capacity=16;}
    for(uint32_t n=0;n<s->info.count;++n){
        if(!read_record(s,s->active_bank,n,stored,&r,&serial) || serial!=s->info.first_serial+n)return failed(s);
        if(!same_sid(sid,stored)){if(found)ended=true;continue;}
        if(ended || (found && (last.sequence==UINT64_MAX || r.sequence!=last.sequence+1 || r.monotonic_ms<last.monotonic_ms)))return failed(s);
        found=true;last=r;
        if(snapshot!=NULL && r.sequence>after && snapshot->count<16)snapshot->records[snapshot->count++]=r;
    }
    if(!found){
        if(!same_sid(sid,s->info.current_session))return s->writable?AG_FLASH_HISTORY_NOT_RETAINED:AG_FLASH_HISTORY_READ_ONLY;
        if(s->info.current_has_records)return failed(s);
        *latest=s->info.current_initial_sequence-1;
        if(snapshot!=NULL)snapshot->next_sequence=s->info.current_initial_sequence;
    }else{
        *latest=last.sequence;
        if(snapshot!=NULL){
            if(snapshot->count==0)snapshot->records[snapshot->count++]=last;
            const ag_history_record_t *tail=&snapshot->records[snapshot->count-1];
            snapshot->last_ms=tail->monotonic_ms;snapshot->exhausted=tail->sequence==UINT64_MAX;
            snapshot->next_sequence=snapshot->exhausted?UINT64_MAX:tail->sequence+1;
        }
    }
    if(after>*latest){
        if(!s->writable && snapshot!=NULL)memset(snapshot,0,sizeof(*snapshot));
        return s->writable?AG_FLASH_HISTORY_CURSOR_AHEAD:AG_FLASH_HISTORY_READ_ONLY;
    }
    return s->writable?AG_FLASH_HISTORY_OK:AG_FLASH_HISTORY_READ_ONLY;
}
ag_flash_history_result_t ag_flash_history_make_page(ag_flash_history_t *s,const uint8_t sid[16],uint64_t after,size_t limit,
                                                     char *out,size_t capacity,ag_history_page_info_t *info){
    if(out!=NULL && capacity)out[0]=0;
    if(info!=NULL)memset(info,0,sizeof(*info));
    if(out==NULL || info==NULL || limit<1 || limit>16)return AG_FLASH_HISTORY_INVALID;
    ag_history_snapshot_t snapshot;uint64_t latest;
    ag_flash_history_result_t result=query(s,sid,after,&snapshot,&latest);
    if(result!=AG_FLASH_HISTORY_OK && result!=AG_FLASH_HISTORY_READ_ONLY)return result;
    if(snapshot.next_sequence==0)return result;
    ag_history_result_t encoded=ag_history_make_page(&snapshot,sid,after,limit,out,capacity,info);
    if(encoded==AG_HISTORY_OUTPUT_SHORT)return AG_FLASH_HISTORY_OUTPUT_SHORT;
    if(encoded!=AG_HISTORY_OK)return AG_FLASH_HISTORY_INVALID;
    return result;
}
ag_flash_history_result_t ag_flash_history_acknowledge(ag_flash_history_t *s,const uint8_t sid[16],uint64_t through){
    uint64_t latest;return query(s,sid,through,NULL,&latest);
}
