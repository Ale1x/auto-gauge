/* Independent behavioral framing review: no crypto, SDK, flash or radio. */
#include "ota_framed_endpoint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    ag_ot_owner_t owner;
    ag_ot_endpoint_t endpoint;
    uint64_t now;
    unsigned auth,clock,begin,write,finish,select,cancel,poll;
    bool allowed,reenter;
} m;
#define LEASE UINT64_C(81)
static bool authorized(void *context,uint64_t lease){
    assert(context==&m&&lease==LEASE);++m.auth;
    if(m.reenter&&m.endpoint.busy){
        assert(ag_ot_endpoint_poll(&m.endpoint,LEASE)==AG_OT_EP_BUSY);
        assert(ag_ot_endpoint_close(&m.endpoint,LEASE)==AG_OT_EP_BUSY);
    }
    return m.allowed;
}
static uint64_t clock_ms(void *context){assert(context==&m);++m.clock;return m.now;}
static bool begin(void *context,uint64_t lease,const uint8_t *bytes,size_t length,uint32_t *size){
    assert(context==&m&&lease==LEASE&&bytes!=NULL&&length==1&&bytes[0]==1);++m.begin;*size=16;return true;
}
static bool write_chunk(void *context,uint64_t lease,uint32_t offset,const uint8_t *bytes,size_t length){
    assert(context==&m&&lease==LEASE&&offset==0&&bytes!=NULL&&length==16);++m.write;return true;
}
static bool finish(void *context,uint64_t lease){assert(context==&m&&lease==LEASE);++m.finish;return true;}
static bool select_image(void *context,uint64_t lease){assert(context==&m&&lease==LEASE);++m.select;return true;}
static bool cancel(void *context,uint64_t lease){assert(context==&m&&lease==LEASE);++m.cancel;return true;}
static bool poll_owner(void *context,uint64_t lease){assert(context==&m&&lease==LEASE);++m.poll;return true;}
static void boot(void){
    memset(&m,0,sizeof(m));m.allowed=true;
    ag_ot_hooks_t hooks={&m,authorized,clock_ms,begin,write_chunk,finish,select_image,poll_owner,cancel,1000,10000};
    assert(ag_ot_owner_init(&m.owner,&hooks));
    ag_ot_ep_hooks_t endpoint_hooks={&m,authorized,clock_ms,50,100};
    assert(ag_ot_endpoint_init(&m.endpoint,&m.owner,LEASE,&endpoint_hooks));
}
static size_t frame(uint32_t id,ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t length,uint8_t *packet){
    uint8_t payload[AG_OTA_TRANSFER_REQUEST_MAX];size_t payload_length=0,packet_length=0;
    ag_ot_request_t request={op,7,offset,body,length};assert(ag_ot_request_encode(&request,payload,sizeof(payload),&payload_length));
    assert(ag_frame_encode(1,id,payload,payload_length,packet,AG_FRAME_MAX_SIZE,&packet_length)==AG_FRAME_OK);return packet_length;
}
static ag_ot_response_t exchange(uint32_t id,ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t length){
    uint8_t packet[AG_FRAME_MAX_SIZE];size_t n=frame(id,op,offset,body,length,packet);
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n)==AG_OT_EP_OK);
    assert(ag_ot_endpoint_step(&m.endpoint,LEASE)==AG_OT_EP_OK);
    size_t written=0;assert(ag_ot_endpoint_take_output(&m.endpoint,LEASE,packet,sizeof(packet),&written)==AG_OT_EP_OK);
    ag_frame_view_t response;assert(ag_frame_decode(packet,written,&response)==AG_FRAME_OK&&response.kind==2&&response.request_id==id);
    ag_ot_response_t decoded;assert(ag_ot_response_decode(response.payload,response.payload_length,&decoded));return decoded;
}
static void writing(void){
    const uint8_t manifest=1;
    assert(exchange(1,AG_OT_START,1,NULL,0).status==AG_OT_OK);
    assert(exchange(2,AG_OT_MANIFEST,0,&manifest,1).status==AG_OT_OK);
    assert(exchange(3,AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK&&m.begin==1);
}
static void sole_frame_before_dispatch(void){
    uint8_t packet[AG_FRAME_MAX_SIZE*2];boot();size_t n=frame(1,AG_OT_START,1,NULL,0,packet);
    packet[n]='A';assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n+1)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.phase==AG_OT_IDLE&&m.begin==0&&m.cancel==0);
    boot();n=frame(1,AG_OT_START,1,NULL,0,packet);size_t second=frame(2,AG_OT_BEGIN,0,NULL,0,packet+n);
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n+second)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.phase==AG_OT_IDLE&&m.owner.transfer_id==0&&m.cancel==0);
    boot();const uint8_t manifest=1;assert(exchange(1,AG_OT_START,1,NULL,0).status==AG_OT_OK);
    assert(exchange(2,AG_OT_MANIFEST,0,&manifest,1).status==AG_OT_OK);
    n=frame(3,AG_OT_BEGIN,0,NULL,0,packet);packet[n]='A';
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n+1)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.phase==AG_OT_FAULT&&m.begin==0&&m.cancel==0);
    boot();n=frame(1,AG_OT_START,1,NULL,0,packet);assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,10)==AG_OT_EP_OK);
    m.now=50;assert(ag_ot_endpoint_poll(&m.endpoint,LEASE)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.phase==AG_OT_IDLE);
    puts("PASS: reviewer sole-frame staging rejects trailing/concatenated requests and partial timeout before callbacks");
}
static void alien_and_reentry(void){
    boot();m.reenter=true;writing();
    ag_ot_endpoint_t saved=m.endpoint;ag_ot_owner_t saved_owner=m.owner;unsigned auth=m.auth,clock=m.clock;
    uint8_t bytes[AG_FRAME_MAX_SIZE];size_t written=123,n=frame(4,AG_OT_CANCEL,0,NULL,0,bytes);
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE+1,bytes,n)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_step(&m.endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_take_output(&m.endpoint,LEASE+1,bytes,sizeof(bytes),&written)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_poll(&m.endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(ag_ot_endpoint_close(&m.endpoint,LEASE+1)==AG_OT_EP_DENIED);
    assert(memcmp(&saved,&m.endpoint,sizeof(saved))==0&&memcmp(&saved_owner,&m.owner,sizeof(saved_owner))==0&&m.auth==auth&&m.clock==clock);
    assert(ag_ot_endpoint_close(&m.endpoint,LEASE)!=AG_OT_EP_BUSY&&m.cancel==1&&m.owner.phase==AG_OT_FAULT);
    assert(ag_ot_endpoint_close(&m.endpoint,LEASE)!=AG_OT_EP_BUSY&&m.cancel==1);
    ag_ot_endpoint_t replacement={0};ag_ot_ep_hooks_t h={&m,authorized,clock_ms,50,100};saved_owner=m.owner;
    bool initialized=ag_ot_endpoint_init(&replacement,&m.owner,LEASE,&h);
    assert(memcmp(&saved_owner,&m.owner,sizeof(saved_owner))==0);
    if(initialized){n=frame(1,AG_OT_START,1,NULL,0,bytes);assert(ag_ot_endpoint_feed(&replacement,LEASE,bytes,n)==AG_OT_EP_OK);
        assert(ag_ot_endpoint_step(&replacement,LEASE)==AG_OT_EP_OK);assert(m.owner.phase==AG_OT_FAULT&&m.begin==1);}
    puts("PASS: reviewer alien leases preserve state/callbacks; reentry blocked; close single cleanup; reconnect never resets owner");
}
static void pending_and_revoked_output(void){
    uint8_t packet[AG_FRAME_MAX_SIZE];size_t written=0;boot();size_t n=frame(1,AG_OT_START,1,NULL,0,packet);
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n)==AG_OT_EP_OK);
    m.now=99;assert(ag_ot_endpoint_poll(&m.endpoint,LEASE)==AG_OT_EP_OK);
    m.now=100;assert(ag_ot_endpoint_step(&m.endpoint,LEASE)!=AG_OT_EP_OK&&m.endpoint.closed&&m.owner.phase==AG_OT_IDLE);
    boot();writing();uint8_t image[16]={0};n=frame(4,AG_OT_IMAGE,0,image,sizeof(image),packet);
    assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n)==AG_OT_EP_OK);
    assert(ag_ot_endpoint_step(&m.endpoint,LEASE)==AG_OT_EP_OK&&m.write==1);
    m.allowed=false;assert(ag_ot_endpoint_take_output(&m.endpoint,LEASE,packet,sizeof(packet),&written)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.phase==AG_OT_FAULT&&m.cancel==1&&written==0);
    assert(ag_ot_endpoint_close(&m.endpoint,LEASE)!=AG_OT_EP_BUSY&&m.cancel==1);
    boot();writing();assert(exchange(4,AG_OT_IMAGE,0,image,sizeof(image)).status==AG_OT_OK);
    assert(exchange(5,AG_OT_FINISH,0,NULL,0).status==AG_OT_OK);
    n=frame(6,AG_OT_SELECT,0,NULL,0,packet);assert(ag_ot_endpoint_feed(&m.endpoint,LEASE,packet,n)==AG_OT_EP_OK);
    assert(ag_ot_endpoint_step(&m.endpoint,LEASE)==AG_OT_EP_OK&&m.select==1&&m.owner.selection_may_have_changed);
    m.allowed=false;assert(ag_ot_endpoint_take_output(&m.endpoint,LEASE,packet,sizeof(packet),&written)!=AG_OT_EP_OK);
    assert(m.endpoint.closed&&m.owner.selection_may_have_changed&&m.cancel==0&&m.select==1);
    puts("PASS: reviewer pending deadline not refreshed; revoked post-write ACK withheld with cleanup; post-select uncertainty retained");
}
int main(void){sole_frame_before_dispatch();alien_and_reentry();pending_and_revoked_output();return 0;}
