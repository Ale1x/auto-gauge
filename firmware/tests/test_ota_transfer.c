/* Behavioral callbacks only. These tests execute no crypto/flash/boot API. */
#include "ota_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    ag_ot_owner_t owner;
    uint64_t now;
    uint32_t image_bytes,written;
    unsigned begin_calls,write_calls,finish_calls,select_calls,poll_calls,cancel_calls;
    unsigned injection;
    bool allowed,fail_begin,fail_write,fail_finish,fail_select,fail_poll,fail_cancel,reenter,revoke;
    uint64_t delay;
} mock_t;
static mock_t m;
static ag_ot_response_t request(ag_ot_opcode_t op,uint32_t id,uint32_t offset,const uint8_t *body,size_t n,uint64_t lease);
static bool allowed(void *c,uint64_t lease){mock_t *x=c;assert(lease==41);return x->allowed;}
static uint64_t clock_ms(void *c){return ((mock_t *)c)->now;}
static void inject(mock_t *x,unsigned point){
    if(x->injection!=point)return;
    if(x->reenter){ag_ot_response_t r=request(AG_OT_CANCEL,7,0,NULL,0,41);assert(r.status==AG_OT_BUSY);}
    if(x->revoke)x->allowed=false;
    x->now+=x->delay;
}
static bool begin(void *c,uint64_t lease,const uint8_t *p,size_t n,uint32_t *size){
    mock_t *x=c;assert(lease==41&&n==3&&memcmp(p,"abc",3)==0);++x->begin_calls;inject(x,1);*size=x->image_bytes;return !x->fail_begin;
}
static bool write_image(void *c,uint64_t lease,uint32_t offset,const uint8_t *p,size_t n){
    mock_t *x=c;assert(lease==41&&p!=NULL&&offset==x->written);++x->write_calls;x->written+=(uint32_t)n;inject(x,2);return !x->fail_write;
}
static bool finish(void *c,uint64_t lease){mock_t *x=c;assert(lease==41&&x->written==x->image_bytes);++x->finish_calls;inject(x,3);return !x->fail_finish;}
static bool select_image(void *c,uint64_t lease){mock_t *x=c;assert(lease==41);++x->select_calls;inject(x,4);return !x->fail_select;}
static bool poll(void *c,uint64_t lease){mock_t *x=c;assert(lease==41);++x->poll_calls;inject(x,5);return !x->fail_poll;}
static bool cancel(void *c,uint64_t lease){mock_t *x=c;assert(lease==41);++x->cancel_calls;inject(x,6);return !x->fail_cancel;}
static ag_ot_hooks_t hooks(void){ag_ot_hooks_t h={&m,allowed,clock_ms,begin,write_image,finish,select_image,poll,cancel,100,1000};return h;}
static void reset(void){memset(&m,0,sizeof(m));m.allowed=true;m.image_bytes=16;m.now=10;ag_ot_hooks_t h=hooks();assert(ag_ot_owner_init(&m.owner,&h));}
static ag_ot_response_t request(ag_ot_opcode_t op,uint32_t id,uint32_t offset,const uint8_t *body,size_t n,uint64_t lease){
    uint8_t packet[AG_OTA_TRANSFER_REQUEST_MAX];size_t size=0;ag_ot_request_t r={op,id,offset,body,n};
    assert(ag_ot_request_encode(&r,packet,sizeof(packet),&size));return ag_ot_owner_handle(&m.owner,lease,packet,size);
}
static ag_ot_response_t send(ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t n){return request(op,7,offset,body,n,41);}
static void start(void){ag_ot_response_t r=send(AG_OT_START,3,NULL,0);assert(r.status==AG_OT_OK&&r.phase==AG_OT_MANIFEST_PHASE&&r.transfer_id==7);}
static void start_begin(void){start();assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"abc",3).status==AG_OT_OK);assert(send(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK);}
static void ready(void){start_begin();uint8_t bytes[16]={0};assert(send(AG_OT_IMAGE,0,bytes,16).status==AG_OT_OK);assert(send(AG_OT_FINISH,0,NULL,0).phase==AG_OT_READY);}
static void codecs(void){
    reset();uint8_t p[1037];memset(p,0,sizeof(p));p[0]=1;p[1]=1;p[4]=7;p[8]=1;
    ag_ot_request_t r,sentinel;memset(&sentinel,0xa5,sizeof(sentinel));r=sentinel;
    assert(ag_ot_request_decode(p,12,&r)&&r.offset==1&&r.opcode==AG_OT_START);
    for(unsigned i=0;i<12;++i)assert(!ag_ot_request_decode(p,i,&r));
    p[2]=1;r=sentinel;assert(!ag_ot_request_decode(p,12,&r)&&memcmp(&r,&sentinel,sizeof(r))==0);p[2]=0;
    p[1]=2;p[8]=0;assert(!ag_ot_request_decode(p,12,&r));assert(ag_ot_request_decode(p,13,&r));assert(!ag_ot_request_decode(p,1037,&r));
    ag_ot_response_t response={AG_OT_OK,AG_OT_SELECTED,true,7,3,16},decoded;uint8_t out[16];
    assert(ag_ot_response_encode(&response,out,16)&&ag_ot_response_decode(out,16,&decoded));assert(decoded.selection_may_have_changed&&decoded.image_offset==16);
    out[3]=2;memset(&decoded,0xa5,sizeof(decoded));response=decoded;assert(!ag_ot_response_decode(out,16,&decoded)&&memcmp(&decoded,&response,sizeof(decoded))==0);
    puts("PASS: codec strict framing and transactional failures");
}
static void lifecycle(void){
    reset();ag_ot_hooks_t h=hooks();assert(!ag_ot_owner_init(&m.owner,&h));start();
    assert(send(AG_OT_BEGIN,0,NULL,0).status==AG_OT_INVALID&&m.begin_calls==0);
    assert(send(AG_OT_MANIFEST,1,(const uint8_t *)"a",1).status==AG_OT_INVALID);
    assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"a",1).manifest_offset==1);
    assert(send(AG_OT_MANIFEST,1,(const uint8_t *)"bc",2).manifest_offset==3);
    assert(send(AG_OT_BEGIN,0,NULL,0).phase==AG_OT_WRITING&&m.begin_calls==1);
    uint8_t bytes[16]={0};assert(send(AG_OT_IMAGE,1,bytes,16).status==AG_OT_INVALID&&m.write_calls==0);
    assert(send(AG_OT_FINISH,0,NULL,0).status==AG_OT_INVALID&&m.finish_calls==0);
    assert(send(AG_OT_IMAGE,0,bytes,8).image_offset==8);assert(send(AG_OT_IMAGE,0,bytes,8).status==AG_OT_INVALID&&m.write_calls==1);
    assert(send(AG_OT_IMAGE,8,bytes,8).image_offset==16);assert(send(AG_OT_FINISH,0,NULL,0).phase==AG_OT_READY&&m.select_calls==0);
    assert(send(AG_OT_SELECT,0,NULL,0).phase==AG_OT_SELECTED&&m.select_calls==1&&m.owner.selection_may_have_changed);
    assert(send(AG_OT_SELECT,0,NULL,0).status==AG_OT_INVALID&&m.select_calls==1);
    assert(send(AG_OT_START,3,NULL,0).status==AG_OT_INVALID);assert(send(AG_OT_CANCEL,0,NULL,0).status==AG_OT_INVALID&&m.cancel_calls==0);
    assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_OK&&m.poll_calls==0);
    puts("PASS: stop-and-wait progress, explicit select, no retries/restart");
}
static void lease_and_rejections(void){
    reset();m.allowed=false;assert(send(AG_OT_START,3,NULL,0).status==AG_OT_DENIED&&m.owner.phase==AG_OT_IDLE&&m.owner.lease==0);
    m.allowed=true;start_begin();ag_ot_owner_t before=m.owner;
    assert(request(AG_OT_CANCEL,7,0,NULL,0,42).status==AG_OT_DENIED&&memcmp(&m.owner,&before,sizeof(before))==0);
    assert(request(AG_OT_CANCEL,8,0,NULL,0,41).status==AG_OT_INVALID&&m.cancel_calls==0);
    uint8_t bad[12]={0};assert(ag_ot_owner_handle(&m.owner,41,bad,sizeof(bad)).status==AG_OT_INVALID&&m.cancel_calls==0);
    assert(ag_ot_owner_poll(&m.owner,42).status==AG_OT_DENIED&&m.poll_calls==0);
    assert(send(AG_OT_CANCEL,0,NULL,0).status==AG_OT_OK&&m.owner.phase==AG_OT_FAULT&&m.cancel_calls==1);
    assert(send(AG_OT_START,3,NULL,0).status==AG_OT_INVALID);assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_INVALID&&m.cancel_calls==1);
    puts("PASS: pre-admission reusable and stale lease isolated");
}
static void deadlines(void){
    reset();start();m.now=109;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_OK);assert(m.owner.activity_ms==10);
    m.now=110;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_EXPIRED&&m.owner.phase==AG_OT_FAULT&&m.cancel_calls==0);
    reset();start_begin();m.now=9;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_EXPIRED&&m.cancel_calls==1);
    reset();start_begin();m.allowed=false;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_DENIED&&m.cancel_calls==1&&m.poll_calls==0);
    reset();start_begin();m.now=109;assert(send(AG_OT_IMAGE,1,(const uint8_t *)"x",1).status==AG_OT_INVALID);m.now=110;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_EXPIRED);
    reset();m.owner.hooks.total_timeout_ms=500;start_begin();for(unsigned n=1;n<=5;++n){m.now=10+99*n;assert(send(AG_OT_IMAGE,n-1,(const uint8_t *)"x",1).status==AG_OT_OK);}
    m.now=510;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_EXPIRED&&m.cancel_calls==1);
    reset();start_begin();m.now=UINT64_MAX;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_EXPIRED);
    reset();start();assert(send(AG_OT_CANCEL,0,NULL,0).status==AG_OT_OK&&m.cancel_calls==0&&m.owner.phase==AG_OT_FAULT);
    reset();ag_ot_hooks_t h=hooks();h.idle_timeout_ms=1001;h.total_timeout_ms=1000;ag_ot_owner_t other={0};assert(!ag_ot_owner_init(&other,&h));
    puts("PASS: deadline equality, invalid traffic and polling do not extend idle");
}
static void callback_faults(void){
    reset();start();assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"abc",3).status==AG_OT_OK);m.fail_begin=true;m.fail_cancel=true;
    assert(send(AG_OT_BEGIN,0,NULL,0).status==AG_OT_BACKEND_FAILURE&&m.cancel_calls==1&&m.owner.cleanup_unknown);
    assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_INVALID&&m.cancel_calls==1);
    reset();start();assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"abc",3).status==AG_OT_OK);m.image_bytes=17;
    assert(send(AG_OT_BEGIN,0,NULL,0).status==AG_OT_BACKEND_FAILURE&&m.cancel_calls==1);
    reset();start_begin();m.fail_write=true;uint8_t bytes[16]={0};assert(send(AG_OT_IMAGE,0,bytes,16).status==AG_OT_BACKEND_FAILURE&&m.owner.image_offset==0&&m.written==16&&m.cancel_calls==1);
    reset();start_begin();assert(send(AG_OT_IMAGE,0,bytes,16).status==AG_OT_OK);m.fail_finish=true;assert(send(AG_OT_FINISH,0,NULL,0).status==AG_OT_BACKEND_FAILURE&&m.cancel_calls==1);
    reset();start_begin();m.fail_poll=true;assert(ag_ot_owner_poll(&m.owner,41).status==AG_OT_BACKEND_FAILURE&&m.poll_calls==1&&m.cancel_calls==1);
    puts("PASS: backend errors terminal including ambiguous write and cleanup");
}
static void post_callback_guards(void){
    for(unsigned point=1;point<=5;++point){
        reset();if(point==1){start();assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"abc",3).status==AG_OT_OK);}else if(point==4)ready();else start_begin();
        if(point==3){uint8_t bytes[16]={0};assert(send(AG_OT_IMAGE,0,bytes,16).status==AG_OT_OK);}
        m.injection=point;m.revoke=true;m.reenter=true;ag_ot_response_t r;
        if(point==1)r=send(AG_OT_BEGIN,0,NULL,0);
        else if(point==2){uint8_t bytes[16]={0};r=send(AG_OT_IMAGE,0,bytes,16);}
        else if(point==3)r=send(AG_OT_FINISH,0,NULL,0);
        else if(point==4)r=send(AG_OT_SELECT,0,NULL,0);
        else r=ag_ot_owner_poll(&m.owner,41);
        assert(r.status==AG_OT_DENIED&&r.phase==AG_OT_FAULT&&m.cancel_calls==(point==4?0u:1u));
        assert(r.selection_may_have_changed==(point==4));
    }
    reset();start_begin();m.injection=2;m.delay=100;uint8_t bytes[16]={0};assert(send(AG_OT_IMAGE,0,bytes,16).status==AG_OT_EXPIRED&&m.cancel_calls==1);
    reset();ready();m.fail_select=true;ag_ot_response_t r=send(AG_OT_SELECT,0,NULL,0);assert(r.status==AG_OT_BACKEND_FAILURE&&r.phase==AG_OT_FAULT&&r.selection_may_have_changed&&m.cancel_calls==0);
    assert(send(AG_OT_SELECT,0,NULL,0).status==AG_OT_INVALID&&m.select_calls==1);
    puts("PASS: post-callback revocation, reentry, expiration and selection ambiguity");
}
int main(void){codecs();lifecycle();lease_and_rejections();deadlines();callback_faults();post_callback_guards();return 0;}
