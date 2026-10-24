#include "ag_flash_history_sim.h"
#include <string.h>
bool ag_flash_history_sim_init(ag_flash_history_sim_t *s,uint8_t *bytes,uint32_t n){
    if(s==NULL || bytes==NULL || n<2*AG_FLASH_HISTORY_SECTOR || n>AG_FLASH_HISTORY_MAX_BYTES
        || n%(2*AG_FLASH_HISTORY_SECTOR)!=0)return false;
    memset(s,0,sizeof(*s));s->bytes=bytes;s->length=n;memset(bytes,0xff,n);return true;
}
void ag_flash_history_sim_reboot(ag_flash_history_sim_t *s){
    if(s==NULL)return;
    uint8_t *bytes=s->bytes;uint32_t n=s->length;memset(s,0,sizeof(*s));s->bytes=bytes;s->length=n;
}
void ag_flash_history_sim_fail_at(ag_flash_history_sim_t *s,size_t call,size_t cut){
    if(s==NULL)return;
    ag_flash_history_sim_reboot(s);s->fail_call=call;s->cut_after=cut;
}
static size_t begin(ag_flash_history_sim_t *s,uint32_t offset,size_t n,uint8_t kind){
    if(offset>s->length || n>s->length-offset){s->invalid_access=true;return 0;}
    if(s->fault)return 0;
    ++s->operations;
    if(s->operations<=AG_FLASH_HISTORY_TRACE){s->trace_lengths[s->operations-1]=n;s->trace_kinds[s->operations-1]=kind;}
    if(s->fail_call!=0 && s->operations==s->fail_call){s->fault=true;return s->cut_after<n?s->cut_after:n;}
    return n;
}
static size_t read_io(void *v,uint32_t offset,uint8_t *out,size_t n){
    ag_flash_history_sim_t *s=v;if(s==NULL || out==NULL)return 0;++s->reads;
    size_t done=begin(s,offset,n,0);if(done)memcpy(out,s->bytes+offset,done);return done;
}
static size_t program_io(void *v,uint32_t offset,const uint8_t *data,size_t n){
    ag_flash_history_sim_t *s=v;if(s==NULL || data==NULL)return 0;++s->programs;
    size_t done=begin(s,offset,n,1);
    for(size_t i=0;i<done;++i)if((s->bytes[offset+i]&data[i])!=data[i]){s->invalid_access=true;return 0;}
    for(size_t i=0;i<done;++i)s->bytes[offset+i]&=data[i];
    return done;
}
static bool erase_io(void *v,uint32_t offset){
    ag_flash_history_sim_t *s=v;if(s==NULL)return false;++s->erases;
    if(offset%AG_FLASH_HISTORY_SECTOR!=0){s->invalid_access=true;return false;}
    size_t done=begin(s,offset,AG_FLASH_HISTORY_SECTOR,2);if(done)memset(s->bytes+offset,0xff,done);
    return done==AG_FLASH_HISTORY_SECTOR;
}
ag_flash_history_io_t ag_flash_history_sim_io(ag_flash_history_sim_t *s){
    return (ag_flash_history_io_t){s,read_io,program_io,erase_io};
}
