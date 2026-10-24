#include "transfer_adapter.h"
#include <string.h>

static bool allowed(void *context,uint64_t lease){
    ag_ota_transfer_adapter_t *s=context;
    return s->platform.maintenance_allowed(s->platform.context,lease);
}
static uint64_t clock_ms(void *context){
    ag_ota_transfer_adapter_t *s=context;
    return s->platform.clock_ms(s->platform.context);
}
static bool verify(void *context,const uint8_t *bytes,size_t length,ag_ota_plan_t *plan){
    ag_ota_transfer_adapter_t *s=context;
    ag_manifest_verified_t verified;
    if(ag_manifest_verify(&s->trust,bytes,length,&verified)!=AG_MANIFEST_OK)return false;
    s->verified=verified;s->manifest_verified=true;*plan=verified.plan;return true;
}
static bool begin(void *context,uint64_t lease,const uint8_t *bytes,size_t length,uint32_t *image_bytes){
    ag_ota_transfer_adapter_t *s=context;
    if(ag_ota_installer_begin(&s->installer,lease,bytes,length)!=AG_OTA_OK)return false;
    if(!s->manifest_verified)return false;
    *image_bytes=s->verified.plan.image_bytes;return true;
}
static bool write_chunk(void *context,uint64_t lease,uint32_t offset,const uint8_t *bytes,size_t length){
    return ag_ota_installer_write(&((ag_ota_transfer_adapter_t *)context)->installer,lease,offset,bytes,length)==AG_OTA_OK;
}
static bool finish(void *context,uint64_t lease){return ag_ota_installer_finish(&((ag_ota_transfer_adapter_t *)context)->installer,lease)==AG_OTA_OK;}
static bool select_boot(void *context,uint64_t lease){return ag_ota_installer_select(&((ag_ota_transfer_adapter_t *)context)->installer,lease)==AG_OTA_OK;}
static bool poll(void *context,uint64_t lease){return ag_ota_installer_poll(&((ag_ota_transfer_adapter_t *)context)->installer,lease)==AG_OTA_OK;}
static bool cancel(void *context,uint64_t lease){
    ag_ota_transfer_adapter_t *s=context;
    /* The installer records abort uncertainty; cancelling its fault phase does
     * not turn a failed cleanup into success or permit a second installation. */
    if(s->installer.selection_may_have_changed)return false;
    if(s->installer.phase==AG_OTA_IDLE||s->installer.phase==AG_OTA_FAULT)
        return !s->installer.handle_active&&!s->installer.cleanup_unknown;
    ag_ota_result_t result=ag_ota_installer_cancel(&s->installer,lease);
    /* Installer cancel intentionally returns DENIED after entering FAULT. */
    return result==AG_OTA_DENIED&&s->installer.phase==AG_OTA_FAULT
        &&!s->installer.handle_active&&!s->installer.cleanup_unknown;
}
bool ag_ota_transfer_adapter_init(ag_ota_transfer_adapter_t *s,const ag_manifest_trust_t *trust,const ag_ota_transfer_platform_t *platform){
    if(s==NULL||trust==NULL||platform==NULL||s->initialized||platform->maintenance_allowed==NULL||platform->clock_ms==NULL
        ||trust->lookup_key==NULL||trust->inactive_capacity==0||trust->inactive_capacity>AG_OTA_IMAGE_MAX
        ||memchr(trust->hardware_id,0,sizeof(trust->hardware_id))==NULL)return false;
    ag_manifest_trust_t trust_copy=*trust;ag_ota_transfer_platform_t platform_copy=*platform;
    if(platform_copy.idle_timeout_ms==0||platform_copy.total_timeout_ms<platform_copy.idle_timeout_ms||platform_copy.total_timeout_ms>900000u)return false;
    memset(s,0,sizeof(*s));s->trust=trust_copy;s->platform=platform_copy;
    ag_ota_hooks_t installer={s,verify,allowed,clock_ms,platform_copy.idle_timeout_ms,platform_copy.total_timeout_ms};
    ag_ot_hooks_t transfer={s,allowed,clock_ms,begin,write_chunk,finish,select_boot,poll,cancel,platform_copy.idle_timeout_ms,platform_copy.total_timeout_ms};
    if(!ag_ota_installer_init(&s->installer,&installer)||!ag_ot_owner_init(&s->transfer,&transfer))return false;
    s->initialized=true;return true;
}
ag_ot_response_t ag_ota_transfer_adapter_handle(ag_ota_transfer_adapter_t *s,uint64_t lease,const uint8_t *bytes,size_t length){
    return ag_ot_owner_handle(s!=NULL&&s->initialized?&s->transfer:NULL,lease,bytes,length);
}
ag_ot_response_t ag_ota_transfer_adapter_poll(ag_ota_transfer_adapter_t *s,uint64_t lease){
    return ag_ot_owner_poll(s!=NULL&&s->initialized?&s->transfer:NULL,lease);
}
