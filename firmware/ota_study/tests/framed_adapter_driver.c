/* Real framed endpoint/owner/adapters/installer, behavioral SDK and crypto only. */
#define main installer_baseline_main
#include "installer_driver.c"
#undef main
#include "framed_adapter.h"
static ag_ota_framed_adapter_t framed;
static uint32_t next_id;
/* Explicit behavioral durable-policy double, never production persistence. */
static bool selection_policy(void *context,const ag_manifest_verified_t *verified,const esp_partition_t *target){
    assert(context==&mock&&verified->release_counter==10&&target==&mock.b);return true;
}
static size_t lookup(void *context,const char *key,uint8_t *out,size_t capacity){(void)context;(void)key;(void)out;(void)capacity;return 0;}
ag_manifest_result_t ag_manifest_verify(const ag_manifest_trust_t *trust,const uint8_t *bytes,size_t length,ag_manifest_verified_t *out){
    assert(trust==&framed.payload.trust&&trust->accepted_release_floor==9&&length==1&&bytes[0]==1);
    ag_ota_plan_t plan;if(!verify_manifest(&mock,bytes,length,&plan))return AG_MANIFEST_SIGNATURE;
    memset(out,0,sizeof(*out));out->plan=plan;out->release_counter=10;return AG_MANIFEST_OK;
}
static void framed_boot(void){
    boot();memset(&framed,0,sizeof(framed));next_id=1;
    ag_manifest_trust_t trust={0};memcpy(trust.hardware_id,"test",5);trust.inactive_capacity=0x100000;trust.accepted_release_floor=9;trust.lookup_key=lookup;
    ag_ota_transfer_platform_t platform={&mock,authorized,clock_ms,100,1000,&mock,selection_policy,false};
    assert(ag_ota_framed_adapter_init(&framed,&trust,&platform,LEASE,50,80));
    assert(!ag_ota_framed_adapter_init(&framed,&trust,&platform,LEASE,50,80));
    assert(mock.begin_calls==0&&mock.verify_calls==0);
}
static size_t packet(ag_ot_opcode_t opcode,uint32_t offset,const uint8_t *body,size_t length,uint8_t *out){
    uint8_t payload[1036];size_t payload_length=0,written=0;
    ag_ot_request_t request={opcode,7,offset,body,length};assert(ag_ot_request_encode(&request,payload,sizeof(payload),&payload_length));
    assert(ag_frame_encode(1,next_id,payload,payload_length,out,1052,&written)==AG_FRAME_OK);return written;
}
static ag_ot_response_t request(ag_ot_opcode_t opcode,uint32_t offset,const uint8_t *body,size_t length){
    uint8_t input[1052],output[32];size_t count=packet(opcode,offset,body,length,input),written=0;
    for(size_t at=0;at<count;){size_t n=count-at;if(n>23)n=23;assert(ag_ota_framed_adapter_feed(&framed,LEASE,input+at,n)==AG_OT_EP_OK);at+=n;}
    assert(ag_ota_framed_adapter_step(&framed,LEASE)==AG_OT_EP_OK);
    assert(ag_ota_framed_adapter_take_output(&framed,LEASE,output,sizeof(output),&written)==AG_OT_EP_OK&&written==32);
    ag_frame_view_t frame;assert(ag_frame_decode(output,written,&frame)==AG_FRAME_OK&&frame.kind==2&&frame.request_id==next_id++);
    ag_ot_response_t response;assert(ag_ot_response_decode(frame.payload,frame.payload_length,&response));return response;
}
static void ready_framed(void){
    framed_boot();uint8_t envelope=1;
    assert(request(AG_OT_START,1,NULL,0).status==AG_OT_OK);
    assert(request(AG_OT_MANIFEST,0,&envelope,1).status==AG_OT_OK);
    assert(mock.verify_calls==0&&mock.begin_calls==0);
    assert(request(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK&&mock.begin_calls==1);
    for(uint32_t at=0;at<IMAGE_BYTES;){size_t n=IMAGE_BYTES-at;if(n>1024)n=1024;assert(request(AG_OT_IMAGE,at,image+at,n).status==AG_OT_OK);at+=(uint32_t)n;}
    assert(request(AG_OT_FINISH,0,NULL,0).phase==AG_OT_READY&&mock.select_calls==0);
}
int main(void){
    assert(installer_baseline_main()==0);
    ready_framed();assert(request(AG_OT_SELECT,0,NULL,0).phase==AG_OT_SELECTED&&mock.select_calls==1);
    assert(framed.payload.trust.accepted_release_floor==9);
    assert(ag_ota_framed_adapter_close(&framed,LEASE)==AG_OT_EP_OK&&mock.abort_calls==0);
    ready_framed();uint8_t bytes[1052];size_t n=packet(AG_OT_SELECT,0,NULL,0,bytes);
    assert(ag_ota_framed_adapter_feed(&framed,LEASE,bytes,n)==AG_OT_EP_OK);
    assert(mock.select_calls==0); /* Stage never selects. */
    assert(ag_ota_framed_adapter_close(&framed,LEASE)==AG_OT_EP_OK&&mock.select_calls==0);
    assert(ag_ota_framed_adapter_step(&framed,LEASE)==AG_OT_EP_CLOSED);
    ready_framed();mock.select_rc=-1;mock.commit_failed_select=true;
    ag_ot_response_t response=request(AG_OT_SELECT,0,NULL,0);
    assert(response.status==AG_OT_BACKEND_FAILURE&&response.selection_may_have_changed&&mock.boot==&mock.b&&mock.select_calls==1);
    (void)ag_ota_framed_adapter_close(&framed,LEASE);assert(mock.select_calls==1&&mock.abort_calls==0);
    ready_framed();n=packet(AG_OT_SELECT,0,NULL,0,bytes);assert(ag_ota_framed_adapter_feed(&framed,LEASE,bytes,n)==AG_OT_EP_OK);
    assert(ag_ota_framed_adapter_step(&framed,LEASE)==AG_OT_EP_OK&&mock.select_calls==1);
    mock.allowed=false;size_t written=99;uint8_t output[32];
    assert(ag_ota_framed_adapter_take_output(&framed,LEASE,output,sizeof(output),&written)!=AG_OT_EP_OK&&written==0);
    assert(framed.payload.transfer.selection_may_have_changed&&mock.select_calls==1);
    puts("PASS: framed SDK binding lifecycle; behavioral SDK/crypto only");return 0;
}
