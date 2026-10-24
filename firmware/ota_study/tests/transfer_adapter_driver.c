/* Behavioral SDK, hash and manifest-verifier doubles. Exercises real transfer,
 * adapter and installer orchestration only; no RSA/SHA or physical flash. */
#define main installer_baseline_main
#include "installer_driver.c"
#undef main
#include "transfer_adapter.h"

static ag_ota_transfer_adapter_t adapter;
static bool captured_trust;
static size_t lookup(void *context,const char *key,uint8_t *out,size_t capacity){
    (void)context;(void)key;(void)out;(void)capacity;return 0;
}
ag_manifest_result_t ag_manifest_verify(const ag_manifest_trust_t *trust,const uint8_t *bytes,size_t length,ag_manifest_verified_t *out){
    assert(trust==&adapter.trust&&trust->accepted_release_floor==9);
    assert(bytes!=NULL&&length==1&&bytes[0]==1);captured_trust=true;
    ag_ota_plan_t plan;
    if(!verify_manifest(&mock,bytes,length,&plan))return AG_MANIFEST_SIGNATURE;
    memset(out,0,sizeof(*out));out->plan=plan;out->release_counter=10;memcpy(out->signing_key_id,"test",5);return AG_MANIFEST_OK;
}
static void adapter_boot(void){
    boot();memset(&adapter,0,sizeof(adapter));captured_trust=false;
    ag_manifest_trust_t trust={0};memcpy(trust.hardware_id,"test",5);
    trust.inactive_capacity=0x100000;trust.accepted_release_floor=9;trust.lookup_key=lookup;
    ag_ota_transfer_platform_t platform={&mock,authorized,clock_ms,100,1000};
    assert(ag_ota_transfer_adapter_init(&adapter,&trust,&platform));
    assert(!ag_ota_transfer_adapter_init(&adapter,&trust,&platform));
    assert(mock.begin_calls==0&&mock.verify_calls==0);
}
static ag_ot_response_t request(ag_ot_opcode_t opcode,uint32_t offset,const uint8_t *body,size_t length){
    uint8_t payload[1036];size_t written;
    ag_ot_request_t req={opcode,7,offset,body,length};assert(ag_ot_request_encode(&req,payload,sizeof(payload),&written));
    return ag_ota_transfer_adapter_handle(&adapter,LEASE,payload,written);
}
static void adapter_manifest(void){
    uint8_t envelope=1;
    assert(request(AG_OT_START,1,NULL,0).status==AG_OT_OK);
    assert(request(AG_OT_MANIFEST,0,&envelope,1).status==AG_OT_OK);
    assert(mock.begin_calls==0&&mock.write_calls==0&&mock.verify_calls==0);
}
static void adapter_ready(void){
    adapter_boot();adapter_manifest();assert(request(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK);
    assert(captured_trust&&adapter.manifest_verified&&adapter.verified.release_counter==10);
    assert(adapter.trust.accepted_release_floor==9&&mock.begin_calls==1);
    for(uint32_t at=0;at<IMAGE_BYTES;){size_t count=IMAGE_BYTES-at;if(count>1024)count=1024;
        ag_ot_response_t r=request(AG_OT_IMAGE,at,image+at,count);assert(r.status==AG_OT_OK&&r.image_offset==at+count);at+=(uint32_t)count;}
    assert(request(AG_OT_FINISH,0,NULL,0).phase==AG_OT_READY&&mock.select_calls==0);
    assert(ag_ota_transfer_adapter_poll(&adapter,LEASE).status==AG_OT_OK);
}
int main(void){
    assert(installer_baseline_main()==0);
    adapter_ready();ag_ot_response_t r=request(AG_OT_SELECT,0,NULL,0);
    assert(r.status==AG_OT_OK&&r.phase==AG_OT_SELECTED&&r.selection_may_have_changed&&mock.select_calls==1);
    assert(adapter.trust.accepted_release_floor==9);
    adapter_boot();adapter_manifest();mock.manifest_ok=false;
    r=request(AG_OT_BEGIN,0,NULL,0);assert(r.phase==AG_OT_FAULT&&mock.begin_calls==0&&!adapter.transfer.cleanup_unknown);
    adapter_boot();adapter_manifest();assert(request(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK);
    r=request(AG_OT_CANCEL,0,NULL,0);assert(r.phase==AG_OT_FAULT&&mock.abort_calls==1&&!adapter.transfer.cleanup_unknown);
    adapter_boot();adapter_manifest();assert(request(AG_OT_BEGIN,0,NULL,0).status==AG_OT_OK);
    mock.abort_rc=-1;r=request(AG_OT_CANCEL,0,NULL,0);assert(r.phase==AG_OT_FAULT&&adapter.transfer.cleanup_unknown);
    adapter_ready();mock.select_rc=-1;mock.commit_failed_select=true;r=request(AG_OT_SELECT,0,NULL,0);
    assert(r.status==AG_OT_BACKEND_FAILURE&&r.phase==AG_OT_FAULT&&r.selection_may_have_changed&&mock.boot==&mock.b);
    assert(request(AG_OT_SELECT,0,NULL,0).status!=AG_OT_OK&&mock.select_calls==1);
    puts("PASS: OTA transfer adapter binding + 8 installer baseline groups; behavioral crypto/SDK only");return 0;
}
