/* Real frame/stream/endpoint/owner with behavioral callbacks only. */
#include "ota_framed_endpoint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define LEASE UINT64_C(41)
static ag_ot_endpoint_t endpoint;
static ag_ot_owner_t owner;
static struct {
    uint64_t now;
    unsigned auth,clocks,begins,writes,finishes,selects,polls,cancels;
    bool allowed,reenter,revoke_begin,select_fail;
} fixture;
static bool allowed(void *context,uint64_t lease){
    assert(context==&fixture&&lease==LEASE);++fixture.auth;
    if(fixture.reenter){
        uint8_t out[32];size_t count=99;
        assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_BUSY);
        assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_BUSY);
        assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_BUSY);
        assert(ag_ot_endpoint_take_output(&endpoint,LEASE,out,sizeof(out),&count)==AG_OT_EP_BUSY&&count==99);
    }
    return fixture.allowed;
}
static uint64_t clock_ms(void *context){assert(context==&fixture);++fixture.clocks;return fixture.now;}
static bool begin(void *context,uint64_t lease,const uint8_t *bytes,size_t length,uint32_t *size){
    assert(context==&fixture&&lease==LEASE&&length==1&&bytes[0]==9);++fixture.begins;*size=16;
    if(fixture.revoke_begin)fixture.allowed=false;
    return true;
}
static bool write_image(void *context,uint64_t lease,uint32_t offset,const uint8_t *bytes,size_t length){
    assert(context==&fixture&&lease==LEASE&&bytes!=NULL&&offset==0&&length==16);++fixture.writes;return true;
}
static bool finish(void *context,uint64_t lease){assert(context==&fixture&&lease==LEASE);++fixture.finishes;return true;}
static bool select_boot(void *context,uint64_t lease){assert(context==&fixture&&lease==LEASE);++fixture.selects;return !fixture.select_fail;}
static bool poll_owner(void *context,uint64_t lease){assert(context==&fixture&&lease==LEASE);++fixture.polls;return true;}
static bool cancel(void *context,uint64_t lease){assert(context==&fixture&&lease==LEASE);++fixture.cancels;return true;}
static void boot(void){
    memset(&fixture,0,sizeof(fixture));fixture.allowed=true;memset(&owner,0,sizeof(owner));memset(&endpoint,0,sizeof(endpoint));
    ag_ot_hooks_t hooks={&fixture,allowed,clock_ms,begin,write_image,finish,select_boot,poll_owner,cancel,500,1000};
    assert(ag_ot_owner_init(&owner,&hooks));
    ag_ot_ep_hooks_t ep={&fixture,allowed,clock_ms,100,100};
    assert(ag_ot_endpoint_init(&endpoint,&owner,LEASE,&ep));assert(!ag_ot_endpoint_init(&endpoint,&owner,LEASE,&ep));
}
static size_t frame(ag_ot_opcode_t opcode,uint32_t offset,const uint8_t *body,size_t length,uint32_t id,uint8_t *out){
    uint8_t payload[1036];size_t bytes=0,written=0;ag_ot_request_t r={opcode,7,offset,body,length};
    assert(ag_ot_request_encode(&r,payload,sizeof(payload),&bytes));
    assert(ag_frame_encode(1,id,payload,bytes,out,1052,&written)==AG_FRAME_OK);return written;
}
static void feed_chunks(const uint8_t *bytes,size_t length,size_t chunk){
    for(size_t at=0;at<length;){size_t count=length-at;if(count>chunk)count=chunk;
        assert(ag_ot_endpoint_feed(&endpoint,LEASE,bytes+at,count)==AG_OT_EP_OK);at+=count;}
}
static ag_ot_response_t exchange(ag_ot_opcode_t opcode,uint32_t offset,const uint8_t *body,size_t length){
    uint8_t packet[1052],out[32];uint32_t id=(uint32_t)endpoint.next_request_id;
    size_t bytes=frame(opcode,offset,body,length,id,packet);feed_chunks(packet,bytes,7);
    assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_OK);
    size_t written=0;assert(ag_ot_endpoint_take_output(&endpoint,LEASE,out,sizeof(out),&written)==AG_OT_EP_OK&&written==32);
    ag_frame_view_t view;assert(ag_frame_decode(out,written,&view)==AG_FRAME_OK&&view.kind==2&&view.request_id==id);
    ag_ot_response_t response;assert(ag_ot_response_decode(view.payload,view.payload_length,&response));return response;
}
static void writing(void){
    boot();uint8_t manifest=9;
    assert(exchange(AG_OT_START,1,NULL,0).status==AG_OT_OK);
    assert(exchange(AG_OT_MANIFEST,0,&manifest,1).status==AG_OT_OK);
    assert(exchange(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK&&fixture.begins==1);
}
static void ready(void){writing();uint8_t bytes[16]={0};assert(exchange(AG_OT_IMAGE,0,bytes,sizeof(bytes)).status==AG_OT_OK);assert(exchange(AG_OT_FINISH,0,NULL,0).phase==AG_OT_READY);}
static void test_staging_and_complete_lifecycle(void){
    boot();uint8_t packet[1052];size_t bytes=frame(AG_OT_START,1,NULL,0,1,packet);
    feed_chunks(packet,bytes,1);assert(endpoint.staged&&owner.phase==AG_OT_IDLE&&owner.transfer_id==0);
    ag_ot_endpoint_t copy=endpoint;ag_ot_owner_t owner_copy=owner;unsigned calls=fixture.auth;
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes)==AG_OT_EP_BUSY);
    assert(!memcmp(&copy,&endpoint,sizeof(copy))&&!memcmp(&owner_copy,&owner,sizeof(owner))&&fixture.auth==calls);
    assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_OK&&owner.phase==AG_OT_MANIFEST_PHASE);
    uint8_t output[32];size_t count=9;
    assert(ag_ot_endpoint_take_output(&endpoint,LEASE,output,31,&count)==AG_OT_EP_OUTPUT_SHORT&&count==0&&endpoint.has_output);
    assert(ag_ot_endpoint_take_output(&endpoint,LEASE,output,32,&count)==AG_OT_EP_OK&&count==32);
    assert(ag_ot_endpoint_take_output(&endpoint,LEASE,output,32,&count)==AG_OT_EP_NO_OUTPUT);
    ready();assert(fixture.selects==0);ag_ot_response_t r=exchange(AG_OT_SELECT,0,NULL,0);
    assert(r.phase==AG_OT_SELECTED&&r.selection_may_have_changed&&fixture.selects==1);
    assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_OK&&fixture.cancels==0&&owner.phase==AG_OT_SELECTED);
}
static void test_sole_frame_and_replay(void){
    uint8_t packet[1052];boot();size_t bytes=frame(AG_OT_START,1,NULL,0,1,packet);packet[bytes]='A';
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes+1)==AG_OT_EP_INVALID&&endpoint.closed&&owner.phase==AG_OT_IDLE);
    boot();bytes=frame(AG_OT_START,1,NULL,0,1,packet);memcpy(packet+bytes,packet,bytes);
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes*2)==AG_OT_EP_INVALID&&owner.phase==AG_OT_IDLE);
    boot();bytes=frame(AG_OT_START,1,NULL,0,2,packet);assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes)==AG_OT_EP_INVALID&&owner.phase==AG_OT_IDLE);
    boot();assert(exchange(AG_OT_START,1,NULL,0).status==AG_OT_OK);
    bytes=frame(AG_OT_START,1,NULL,0,1,packet);assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes)==AG_OT_EP_INVALID&&owner.phase==AG_OT_FAULT&&fixture.begins==0);
    boot();uint8_t oversized[1037]={0};size_t oversized_length=0;
    uint8_t oversized_frame[1053];
    assert(ag_frame_encode(1,1,oversized,sizeof(oversized),oversized_frame,sizeof(oversized_frame),&oversized_length)==AG_FRAME_OK);
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,oversized_frame,12)==AG_OT_EP_INVALID&&owner.phase==AG_OT_IDLE);
    boot();bytes=frame(AG_OT_START,1,NULL,0,1,packet);packet[bytes-1]^=1;
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,bytes)==AG_OT_EP_INVALID&&owner.phase==AG_OT_IDLE);
}
static void test_stale_lease_and_reentry(void){
    writing();ag_ot_endpoint_t copy=endpoint;ag_ot_owner_t owner_copy=owner;unsigned auth=fixture.auth,clocks=fixture.clocks;
    uint8_t packet[1052],out[32];size_t written=99,bytes=frame(AG_OT_FINISH,0,NULL,0,4,packet);
    assert(ag_ot_endpoint_feed(&endpoint,LEASE+1,packet,bytes)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_step(&endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_take_output(&endpoint,LEASE+1,out,32,&written)==AG_OT_EP_DENIED&&written==99);
    assert(ag_ot_endpoint_poll(&endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_close(&endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(!memcmp(&copy,&endpoint,sizeof(copy))&&!memcmp(&owner_copy,&owner,sizeof(owner))&&auth==fixture.auth&&clocks==fixture.clocks);
    fixture.reenter=true;assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_OK);
    assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_OK&&fixture.cancels==1);
    assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_CLOSED&&fixture.cancels==1);
}
static void test_frame_and_pending_timeouts(void){
    uint8_t packet[1052];boot();size_t bytes=frame(AG_OT_START,1,NULL,0,1,packet);
    assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,5)==AG_OT_EP_OK);fixture.now=100;
    assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_TIMEOUT&&owner.phase==AG_OT_IDLE);
    boot();bytes=frame(AG_OT_START,1,NULL,0,1,packet);feed_chunks(packet,bytes,32);fixture.now=100;
    assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_TIMEOUT&&owner.phase==AG_OT_IDLE);
    writing();bytes=frame(AG_OT_IMAGE,0,(const uint8_t[16]){0},16,4,packet);feed_chunks(packet,bytes,32);
    assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_OK&&fixture.writes==1);fixture.now=100;
    uint8_t out[32];size_t count=0;assert(ag_ot_endpoint_take_output(&endpoint,LEASE,out,sizeof(out),&count)==AG_OT_EP_TIMEOUT&&count==0&&owner.phase==AG_OT_FAULT&&fixture.cancels==1);
    assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_CLOSED&&fixture.cancels==1);
    boot();fixture.now=10;assert(ag_ot_endpoint_feed(&endpoint,LEASE,packet,1)==AG_OT_EP_OK);fixture.now=9;
    assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_TIMEOUT&&endpoint.closed);
}
static void test_revocation_and_selection_uncertainty(void){
    writing();fixture.allowed=false;assert(ag_ot_endpoint_poll(&endpoint,LEASE)==AG_OT_EP_DENIED&&owner.phase==AG_OT_FAULT&&fixture.cancels==1);
    boot();uint8_t manifest=9;assert(exchange(AG_OT_START,1,NULL,0).status==AG_OT_OK);assert(exchange(AG_OT_MANIFEST,0,&manifest,1).status==AG_OT_OK);
    uint8_t packet[1052];size_t bytes=frame(AG_OT_BEGIN,0,NULL,0,3,packet);feed_chunks(packet,bytes,32);fixture.revoke_begin=true;
    assert(ag_ot_endpoint_step(&endpoint,LEASE)==AG_OT_EP_DENIED&&endpoint.closed&&fixture.begins==1&&fixture.cancels==1);
    ready();fixture.select_fail=true;ag_ot_response_t r=exchange(AG_OT_SELECT,0,NULL,0);
    assert(r.status==AG_OT_BACKEND_FAILURE&&r.selection_may_have_changed&&owner.phase==AG_OT_FAULT);
    assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_OK&&fixture.cancels==0&&fixture.selects==1);
}
static void test_new_endpoint_does_not_reset_owner(void){
    writing();assert(ag_ot_endpoint_close(&endpoint,LEASE)==AG_OT_EP_OK&&owner.phase==AG_OT_FAULT);
    ag_ot_endpoint_t fresh={0};ag_ot_ep_hooks_t hooks={&fixture,allowed,clock_ms,100,100};
    ag_ot_owner_t copy=owner;assert(ag_ot_endpoint_init(&fresh,&owner,LEASE,&hooks));assert(!memcmp(&copy,&owner,sizeof(owner)));
    assert(ag_ot_endpoint_poll(&fresh,LEASE)==AG_OT_EP_BACKEND_FAILURE&&fresh.closed&&!memcmp(&copy,&owner,sizeof(owner)));
}
int main(void){
    test_staging_and_complete_lifecycle();test_sole_frame_and_replay();test_stale_lease_and_reentry();
    test_frame_and_pending_timeouts();test_revocation_and_selection_uncertainty();test_new_endpoint_does_not_reset_owner();
    puts("PASS: 6 framed OTA endpoint groups; behavioral backend only");return 0;
}
