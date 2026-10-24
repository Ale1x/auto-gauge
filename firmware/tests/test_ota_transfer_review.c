/* Independent behavioral review. No crypto, flash, SDK or radio executes. */
#include "ota_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    ag_ot_owner_t owner;
    uint64_t now;
    unsigned begin,write,finish,select,cancel,poll,auth,clock;
    bool authorized,auth_reentry,cancel_reentry,select_failure;
    uint64_t begin_delay,cancel_delay;
} m;
static ag_ot_response_t send(ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t length);
static bool authorized(void *ctx,uint64_t lease){
    assert(ctx==&m&&lease==91);++m.auth;
    if(m.auth_reentry){
        ag_ot_response_t r=ag_ot_owner_poll(&m.owner,lease);assert(r.status==AG_OT_BUSY);
        r=send(AG_OT_CANCEL,0,NULL,0);assert(r.status==AG_OT_BUSY);
    }
    return m.authorized;
}
static uint64_t now(void *ctx){assert(ctx==&m);++m.clock;return m.now;}
static bool begin(void *ctx,uint64_t lease,const uint8_t *bytes,size_t length,uint32_t *size){
    assert(ctx==&m&&lease==91&&length==3&&memcmp(bytes,"abc",3)==0);
    ++m.begin;*size=16;m.now+=m.begin_delay;return true;
}
static bool write_chunk(void *ctx,uint64_t lease,uint32_t offset,const uint8_t *bytes,size_t length){
    assert(ctx==&m&&lease==91&&bytes!=NULL&&offset<16&&length<=16-offset);++m.write;return true;
}
static bool finish(void *ctx,uint64_t lease){assert(ctx==&m&&lease==91);++m.finish;return true;}
static bool select_boot(void *ctx,uint64_t lease){assert(ctx==&m&&lease==91);++m.select;return !m.select_failure;}
static bool poll(void *ctx,uint64_t lease){assert(ctx==&m&&lease==91);++m.poll;return true;}
static bool cancel(void *ctx,uint64_t lease){
    assert(ctx==&m&&lease==91);++m.cancel;
    if(m.cancel_reentry){assert(send(AG_OT_CANCEL,0,NULL,0).status==AG_OT_BUSY);assert(ag_ot_owner_poll(&m.owner,lease).status==AG_OT_BUSY);}
    m.now+=m.cancel_delay;return true;
}
static void boot(void){
    memset(&m,0,sizeof(m));m.authorized=true;
    ag_ot_hooks_t h={&m,authorized,now,begin,write_chunk,finish,select_boot,poll,cancel,100,250};
    assert(ag_ot_owner_init(&m.owner,&h));
}
static ag_ot_response_t send(ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t length){
    ag_ot_request_t r={op,9,offset,body,length};uint8_t p[AG_OTA_TRANSFER_REQUEST_MAX];size_t n=0;
    assert(ag_ot_request_encode(&r,p,sizeof(p),&n));return ag_ot_owner_handle(&m.owner,91,p,n);
}
static void writing(void){
    assert(send(AG_OT_START,3,NULL,0).status==AG_OT_OK);
    assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"abc",3).status==AG_OT_OK);
    assert(send(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK);
}
static void total_deadline(void){
    boot();writing();const uint8_t b=0;
    m.now=90;assert(send(AG_OT_IMAGE,0,&b,1).status==AG_OT_OK);
    m.now=180;assert(send(AG_OT_IMAGE,1,&b,1).status==AG_OT_OK);
    m.now=250;ag_ot_response_t r=send(AG_OT_IMAGE,2,&b,1);
    assert(r.status==AG_OT_EXPIRED&&r.phase==AG_OT_FAULT&&r.image_offset==2&&m.write==2&&m.cancel==1);
    /* begin can expire AFTER crypto/erase already ran: cleanup once. */
    boot();assert(send(AG_OT_START,3,NULL,0).status==AG_OT_OK);
    m.now=90;assert(send(AG_OT_MANIFEST,0,(const uint8_t *)"a",1).status==AG_OT_OK);
    m.now=170;assert(send(AG_OT_MANIFEST,1,(const uint8_t *)"b",1).status==AG_OT_OK);
    m.now=240;assert(send(AG_OT_MANIFEST,2,(const uint8_t *)"c",1).status==AG_OT_OK);
    m.now=249;m.begin_delay=1;r=send(AG_OT_BEGIN,0,NULL,0);
    assert(r.status==AG_OT_EXPIRED&&r.phase==AG_OT_FAULT&&m.begin==1&&m.write==0&&m.cancel==1);
    puts("PASS: independent total deadline survives valid idle refresh and post-begin side effects");
}
static void callbacks_and_selection(void){
    boot();m.auth_reentry=true;writing();m.cancel_reentry=true;m.cancel_delay=100;
    ag_ot_response_t r=send(AG_OT_CANCEL,0,NULL,0);
    assert(r.status==AG_OT_EXPIRED&&r.phase==AG_OT_FAULT&&m.cancel==1&&!m.owner.cleanup_unknown);
    assert(send(AG_OT_CANCEL,0,NULL,0).status==AG_OT_INVALID&&m.cancel==1);
    boot();writing();uint8_t image[16]={0};assert(send(AG_OT_IMAGE,0,image,16).status==AG_OT_OK);
    assert(send(AG_OT_FINISH,0,NULL,0).status==AG_OT_OK);m.select_failure=true;
    r=send(AG_OT_SELECT,0,NULL,0);assert(r.status==AG_OT_BACKEND_FAILURE&&r.phase==AG_OT_FAULT&&r.selection_may_have_changed);
    assert(m.cancel==0&&m.select==1);
    assert(send(AG_OT_SELECT,0,NULL,0).status==AG_OT_INVALID&&m.select==1&&m.cancel==0);
    assert(ag_ot_owner_poll(&m.owner,91).status==AG_OT_INVALID&&m.cancel==0);
    puts("PASS: independent callback reentry/cleanup time and irreversible select latch");
}
static void malformed_and_alien(void){
    boot();writing();ag_ot_owner_t saved=m.owner;unsigned auth=m.auth,clock=m.clock;
    uint32_t rng=UINT32_C(0x31415926);uint8_t bytes[AG_OTA_TRANSFER_REQUEST_MAX+1];
    for(unsigned i=0;i<10000;++i){
        rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);size_t length=rng%sizeof(bytes);
        for(size_t j=0;j<length;++j){rng=rng*UINT32_C(1664525)+UINT32_C(1013904223);bytes[j]=(uint8_t)(rng>>24);}
        if(length>3){bytes[0]=1;bytes[2]=1;} /* explicitly malformed reserved field */
        ag_ot_response_t r=ag_ot_owner_handle(&m.owner,91,bytes,length);
        assert(r.status==AG_OT_INVALID&&memcmp(&saved,&m.owner,sizeof(saved))==0);
    }
    assert(m.auth==auth&&m.clock==clock&&m.cancel==0&&m.write==0);
    ag_ot_request_t req={AG_OT_CANCEL,9,0,NULL,0};size_t n=0;assert(ag_ot_request_encode(&req,bytes,sizeof(bytes),&n));
    assert(ag_ot_owner_handle(&m.owner,92,bytes,n).status==AG_OT_DENIED);
    assert(ag_ot_owner_poll(&m.owner,92).status==AG_OT_DENIED);
    assert(memcmp(&saved,&m.owner,sizeof(saved))==0&&m.auth==auth&&m.clock==clock);
    puts("PASS: independent 10000 malformed requests and stale leases preserve owner without callbacks");
}
int main(void){total_deadline();callbacks_and_selection();malformed_and_alien();return 0;}
