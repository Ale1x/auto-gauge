#include "ag_boot_health.h"
#include <string.h>
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t get64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=(uint64_t)p[i]<<(8*i);return v;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc(const uint8_t *p,size_t n){uint32_t c=UINT32_MAX;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);}return c^UINT32_MAX;}
static bool shape(const ag_boot_health_t *s){
    if(s->generation==0||s->pending>2)return false;
    if(s->pending==0){uint8_t zero[32]={0};return s->candidate_counter==0&&s->target_address==0&&s->image_bytes==0&&s->health_witness==0&&!memcmp(s->digest,zero,32);}
    if((s->pending==1&&s->health_witness!=0)||(s->pending==2&&((s->health_witness&AG_BOOT_HEALTH_REQUIRED_MIN)!=AG_BOOT_HEALTH_REQUIRED_MIN||(s->health_witness&~AG_BOOT_HEALTH_KNOWN))))return false;
    return s->candidate_counter>s->accepted_floor&&s->target_address>0&&s->target_address%65536u==0&&s->image_bytes>0&&s->image_bytes<=16u*1024u*1024u&&s->image_bytes%16==0;
}
static void encode(const ag_boot_health_t *s,uint8_t bytes[96]){
    memset(bytes,0,96);memcpy(bytes,"AGBH",4);bytes[4]=1;bytes[5]=s->pending;
    put64(bytes+8,s->generation);put64(bytes+16,s->accepted_floor);put64(bytes+24,s->candidate_counter);
    put32(bytes+32,s->target_address);put32(bytes+36,s->image_bytes);memcpy(bytes+40,s->digest,32);put32(bytes+72,s->health_witness);put32(bytes+92,crc(bytes,92));
}
static bool decode(ag_boot_health_t *s,const uint8_t bytes[96]){
    if(memcmp(bytes,"AGBH",4)||bytes[4]!=1||bytes[6]||bytes[7]||get32(bytes+92)!=crc(bytes,92))return false;
    for(unsigned i=76;i<92;++i)if(bytes[i])return false;
    s->pending=bytes[5];s->generation=get64(bytes+8);s->accepted_floor=get64(bytes+16);s->candidate_counter=get64(bytes+24);
    s->target_address=get32(bytes+32);s->image_bytes=get32(bytes+36);memcpy(s->digest,bytes+40,32);s->health_witness=get32(bytes+72);return shape(s);
}
static ag_boot_result_t fail(ag_boot_health_t *s,ag_boot_result_t r){s->fault=true;s->ready=false;return r;}
static ag_boot_result_t enter(ag_boot_health_t *s){if(s==NULL||!s->initialized||s->fault)return AG_BOOT_INVALID;if(s->busy)return AG_BOOT_BUSY;s->busy=true;return AG_BOOT_OK;}
static ag_boot_result_t leave(ag_boot_health_t *s,ag_boot_result_t r){s->busy=false;return r;}
static bool observed(ag_boot_health_t *s,ag_boot_observation_t *o){
    memset(o,0,sizeof(*o));
    if(!s->hooks.observe(s->hooks.context,0,o)||o->running_address==0||o->boot_address==0||(unsigned)o->state>AG_BOOT_IMAGE_FACTORY)return false;
    if(s->pending&&o->running_address==s->target_address){
        ag_boot_observation_t hashed;memset(&hashed,0,sizeof(hashed));
        if(!s->hooks.observe(s->hooks.context,s->image_bytes,&hashed)||hashed.running_address!=o->running_address
            ||hashed.boot_address!=o->boot_address||hashed.state!=o->state)return false;
        *o=hashed;
    }
    return true;
}
static bool stable_source(const ag_boot_health_t *s,const ag_boot_observation_t *o){
    return o->state==AG_BOOT_IMAGE_VALID||(s->hooks.allow_factory_bootstrap&&o->state==AG_BOOT_IMAGE_FACTORY);
}
static ag_boot_result_t persist(ag_boot_health_t *s,ag_boot_health_t *next){
    if(s->generation==UINT64_MAX)return fail(s,AG_BOOT_POLICY);
    next->generation=s->generation+1;
    uint8_t bytes[96],check[96];encode(next,bytes);
    if(!s->hooks.write(s->hooks.context,bytes)||s->hooks.read(s->hooks.context,check)!=AG_BOOT_LOAD_OK||memcmp(bytes,check,96))return fail(s,AG_BOOT_UNKNOWN);
    s->generation=next->generation;s->accepted_floor=next->accepted_floor;s->candidate_counter=next->candidate_counter;
    s->target_address=next->target_address;s->image_bytes=next->image_bytes;s->pending=next->pending;s->health_witness=next->health_witness;memcpy(s->digest,next->digest,32);s->ready=true;return AG_BOOT_OK;
}
static ag_boot_result_t clear(ag_boot_health_t *s,bool accept){
    ag_boot_health_t next=*s;if(accept)next.accepted_floor=s->candidate_counter;
    next.pending=0;next.candidate_counter=0;next.target_address=0;next.image_bytes=0;next.health_witness=0;memset(next.digest,0,32);return persist(s,&next);
}
ag_boot_result_t ag_boot_health_open(ag_boot_health_t *s,const ag_boot_hooks_t *h,uint32_t required){
    if(s==NULL||h==NULL||s->initialized||(required&AG_BOOT_HEALTH_REQUIRED_MIN)!=AG_BOOT_HEALTH_REQUIRED_MIN||(required&~AG_BOOT_HEALTH_KNOWN)
        ||h->read==NULL||h->write==NULL||h->observe==NULL||h->target_matches==NULL||h->mark_valid==NULL)return AG_BOOT_INVALID;
    ag_boot_hooks_t hooks=*h;memset(s,0,sizeof(*s));s->hooks=hooks;s->required_health=required;s->initialized=true;s->busy=true;
    uint8_t bytes[96];ag_boot_load_t load=hooks.read(hooks.context,bytes);ag_boot_result_t result;
    if(load==AG_BOOT_LOAD_EMPTY)result=AG_BOOT_EMPTY;
    else if(load!=AG_BOOT_LOAD_OK)result=fail(s,AG_BOOT_IO);
    else if(!decode(s,bytes))result=fail(s,AG_BOOT_INVALID);
    else{s->ready=true;result=AG_BOOT_OK;}
    return leave(s,result);
}
ag_boot_result_t ag_boot_health_provision(ag_boot_health_t *s,uint64_t baseline){
    ag_boot_result_t result=enter(s);if(result!=AG_BOOT_OK)return result;
    if(s->ready||s->generation!=0)return leave(s,AG_BOOT_POLICY);
    ag_boot_observation_t observation;uint8_t bytes[96];
    if(s->hooks.read(s->hooks.context,bytes)!=AG_BOOT_LOAD_EMPTY||!observed(s,&observation)
        ||!stable_source(s,&observation)||observation.running_address!=observation.boot_address)return leave(s,AG_BOOT_POLICY);
    ag_boot_health_t next=*s;next.accepted_floor=baseline;result=persist(s,&next);return leave(s,result);
}
bool ag_boot_health_floor(const ag_boot_health_t *s,uint64_t *floor){if(s==NULL||floor==NULL||!s->initialized||!s->ready||s->fault||s->busy)return false;*floor=s->accepted_floor;return true;}
ag_boot_result_t ag_boot_health_stage(ag_boot_health_t *s,uint64_t counter,uint32_t target,uint32_t size,const uint8_t digest[32]){
    ag_boot_result_t result=enter(s);if(result!=AG_BOOT_OK)return result;
    if(!s->ready||s->pending||digest==NULL||counter<=s->accepted_floor)return leave(s,AG_BOOT_POLICY);
    ag_boot_health_t next=*s;next.pending=1;next.candidate_counter=counter;next.target_address=target;next.image_bytes=size;memcpy(next.digest,digest,32);
    ag_boot_observation_t observation;
    if(!shape(&next)||!observed(s,&observation)||!stable_source(s,&observation)||observation.boot_address!=observation.running_address
        ||target==observation.running_address||!s->hooks.target_matches(s->hooks.context,target,size,digest))return leave(s,AG_BOOT_POLICY);
    result=persist(s,&next);return leave(s,result);
}
static ag_boot_result_t reconcile(ag_boot_health_t *s){
    if(!s->ready)return AG_BOOT_EMPTY;
    ag_boot_observation_t o;if(!observed(s,&o))return fail(s,AG_BOOT_IO);
    if(!s->pending)return stable_source(s,&o)&&o.boot_address==o.running_address?AG_BOOT_OK:AG_BOOT_POLICY;
    if(o.running_address!=s->target_address){
        if(!stable_source(s,&o))return fail(s,AG_BOOT_POLICY);
        if(o.boot_address==s->target_address)return AG_BOOT_AWAITING_REBOOT;
        if(o.boot_address!=o.running_address)return fail(s,AG_BOOT_POLICY);
        ag_boot_result_t r=clear(s,false);return r==AG_BOOT_OK?AG_BOOT_RECOVERED_FALLBACK:r;
    }
    if(o.boot_address!=o.running_address||memcmp(o.digest,s->digest,32))return fail(s,AG_BOOT_POLICY);
    if(o.state==AG_BOOT_IMAGE_PENDING)return AG_BOOT_NEEDS_HEALTH;
    if(o.state==AG_BOOT_IMAGE_VALID&&s->pending==2&&(s->health_witness&s->required_health)==s->required_health)return clear(s,true);
    return fail(s,AG_BOOT_POLICY);
}
ag_boot_result_t ag_boot_health_reconcile(ag_boot_health_t *s){ag_boot_result_t r=enter(s);if(r!=AG_BOOT_OK)return r;r=reconcile(s);return leave(s,r);}
ag_boot_result_t ag_boot_health_confirm(ag_boot_health_t *s,uint32_t health){
    ag_boot_result_t r=enter(s);if(r!=AG_BOOT_OK)return r;
    if((health&s->required_health)!=s->required_health||(health&~AG_BOOT_HEALTH_KNOWN))return leave(s,AG_BOOT_POLICY);
    r=reconcile(s);if(r!=AG_BOOT_NEEDS_HEALTH)return leave(s,r==AG_BOOT_OK?AG_BOOT_POLICY:r);
    ag_boot_health_t next=*s;next.pending=2;next.health_witness=health;r=persist(s,&next);if(r!=AG_BOOT_OK)return leave(s,r);
    ag_boot_observation_t o;
    if(!observed(s,&o)||o.state!=AG_BOOT_IMAGE_PENDING||o.running_address!=s->target_address||o.boot_address!=o.running_address||memcmp(o.digest,s->digest,32))return leave(s,fail(s,AG_BOOT_POLICY));
    if(!s->hooks.mark_valid(s->hooks.context))return leave(s,fail(s,AG_BOOT_UNKNOWN));
    if(!observed(s,&o)||o.state!=AG_BOOT_IMAGE_VALID||o.running_address!=s->target_address||o.boot_address!=o.running_address||memcmp(o.digest,s->digest,32))return leave(s,fail(s,AG_BOOT_UNKNOWN));
    r=clear(s,true);return leave(s,r);
}
