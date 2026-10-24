#include "ota_installer.h"
#include "sdkconfig.h"
#include "psa/crypto.h"
#include <string.h>

#if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "OTA study requires SDK rollback enabled"
#endif
#if CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
#error "Hardware anti-rollback provisioning/advancement is outside this study"
#endif

static ag_ota_result_t fail(ag_ota_installer_t *s,ag_ota_result_t result) {
    s->phase=AG_OTA_FAULT;
    if(s->handle_active) {
        s->handle_active=false;
        if(esp_ota_abort(s->handle)!=ESP_OK)s->cleanup_unknown=true;
    }
    return result;
}
static ag_ota_result_t guard(ag_ota_installer_t *s,uint64_t lease) {
    if(lease!=s->lease)return AG_OTA_DENIED; /* Stale caller cannot fault current. */
    uint64_t now=s->hooks.clock_ms(s->hooks.context);
    if(now<s->last_ms || now-s->started_ms>=s->hooks.total_timeout_ms
        || now-s->activity_ms>=s->hooks.idle_timeout_ms)return fail(s,AG_OTA_EXPIRED);
    s->last_ms=now;
    if(!s->hooks.maintenance_allowed(s->hooks.context,lease))return fail(s,AG_OTA_DENIED);
    return AG_OTA_OK;
}
static bool same(const esp_partition_t *a,const esp_partition_t *b) {
    return a!=NULL && b!=NULL && a->address==b->address && a->size==b->size
        && a->type==b->type && a->subtype==b->subtype;
}
static bool extent(const esp_partition_t *p,bool app) {
    return p!=NULL && esp_partition_verify(p)!=NULL && p->size>0
        && p->address%(app?65536u:4096u)==0 && p->size%4096u==0
        && (uint64_t)p->address+p->size<=UINT32_MAX;
}
static bool overlap(const esp_partition_t *a,const esp_partition_t *b) {
    return (uint64_t)a->address<(uint64_t)b->address+b->size
        && (uint64_t)b->address<(uint64_t)a->address+a->size;
}
static bool partitions(ag_ota_installer_t *s,bool initial) {
    const esp_partition_t *run=esp_ota_get_running_partition(),*boot=esp_ota_get_boot_partition();
    const esp_partition_t *a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,NULL);
    const esp_partition_t *b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,NULL);
    const esp_partition_t *data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_OTA,NULL);
    if(!extent(run,true)||!same(run,boot)||!extent(a,true)||!extent(b,true)||!extent(data,false)
        ||data->size!=8192 || overlap(a,b)||overlap(a,data)||overlap(b,data))return false;
    if(run->type!=ESP_PARTITION_TYPE_APP || (!same(run,a)&&!same(run,b)))return false;
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(run,&state)!=ESP_OK || state!=ESP_OTA_IMG_VALID)return false;
    const esp_partition_t *target=esp_ota_get_next_update_partition(run);
    if(target==NULL || (same(run,a)?!same(target,b):!same(target,a))
        ||s->plan.image_bytes>target->size)return false;
    if(initial){s->running=run;s->target=target;}
    else if(!same(run,s->running)||!same(target,s->target))return false;
    return true;
}
static bool plan_valid(const ag_ota_plan_t *p) {
    if(p->image_bytes==0||p->image_bytes>AG_OTA_IMAGE_MAX||p->image_bytes%16!=0)return false;
    size_t n=0;while(n<sizeof(p->firmware_version)&&p->firmware_version[n]) {
        unsigned char c=(unsigned char)p->firmware_version[n++];if(c<33||c>126)return false;
    }
    if(n==0||n==sizeof(p->firmware_version))return false;
    for(size_t i=n;i<sizeof(p->firmware_version);++i)if(p->firmware_version[i]!=0)return false;
    return true;
}
bool ag_ota_installer_init(ag_ota_installer_t *s,const ag_ota_hooks_t *hooks) {
    if(s==NULL||hooks==NULL||s->initialized)return false;
    ag_ota_hooks_t copy=*hooks;
    if(copy.verify_manifest==NULL||copy.maintenance_allowed==NULL||copy.clock_ms==NULL
        ||copy.idle_timeout_ms==0||copy.total_timeout_ms<copy.idle_timeout_ms
        ||copy.total_timeout_ms>900000u)return false;
    memset(s,0,sizeof(*s));s->hooks=copy;s->initialized=true;return true;
}
static bool enter(ag_ota_installer_t *s) {
    if(s==NULL||!s->initialized||s->busy)return false;
    s->busy=true;return true;
}
static ag_ota_result_t leave(ag_ota_installer_t *s,ag_ota_result_t result) {s->busy=false;return result;}
ag_ota_result_t ag_ota_installer_begin(ag_ota_installer_t *s,uint64_t lease,const uint8_t *manifest,size_t length) {
    if(!enter(s))return AG_OTA_BUSY;
    if(s->phase!=AG_OTA_IDLE||lease==0||manifest==NULL||length==0||length>4096)
        return leave(s,AG_OTA_INVALID);
    /* A caller denied before admission owns no installation and must not
     * consume the idle boot slot or bind its lease/clock to this object. */
    if(!s->hooks.maintenance_allowed(s->hooks.context,lease))return leave(s,AG_OTA_DENIED);
    s->lease=lease;s->started_ms=s->last_ms=s->activity_ms=s->hooks.clock_ms(s->hooks.context);
    ag_ota_result_t result=guard(s,lease);
    if(result!=AG_OTA_OK)return leave(s,result);
    if(!s->hooks.verify_manifest(s->hooks.context,manifest,length,&s->plan)||!plan_valid(&s->plan))
        return leave(s,fail(s,AG_OTA_DENIED));
    if((result=guard(s,lease))!=AG_OTA_OK)return leave(s,result);
    if(!partitions(s,true))return leave(s,fail(s,AG_OTA_PARTITIONS));
    if(psa_crypto_init()!=PSA_SUCCESS)return leave(s,fail(s,AG_OTA_HASH));
    if((result=guard(s,lease))!=AG_OTA_OK)return leave(s,result);
    s->handle=0;
    esp_err_t begun=esp_ota_begin(s->target,s->plan.image_bytes,&s->handle);
    /* Pinned SDK publishes a nonzero handle BEFORE erase and can return erase
     * failure without freeing that entry. One installation/boot and exclusive
     * OTA ownership exclude allocator wrap/other owners' live handles. */
    s->handle_active=s->handle!=0;
    if(begun!=ESP_OK)
        return leave(s,fail(s,AG_OTA_IO));
    if(!s->handle_active)return leave(s,fail(s,AG_OTA_IO));
    s->phase=AG_OTA_WRITING;
    result=guard(s,lease);return leave(s,result);
}
ag_ota_result_t ag_ota_installer_write(ag_ota_installer_t *s,uint64_t lease,uint32_t offset,const uint8_t *bytes,size_t length) {
    if(!enter(s))return AG_OTA_BUSY;
    if(s->phase!=AG_OTA_WRITING)return leave(s,AG_OTA_INVALID);
    ag_ota_result_t result=guard(s,lease);if(result!=AG_OTA_OK)return leave(s,result);
    if(bytes==NULL||length==0||length>AG_OTA_CHUNK||offset!=s->written
        ||length>s->plan.image_bytes-s->written)return leave(s,fail(s,AG_OTA_INVALID));
    if(!partitions(s,false))return leave(s,fail(s,AG_OTA_PARTITIONS));
    if((result=guard(s,lease))!=AG_OTA_OK)return leave(s,result);
    if(esp_ota_write(s->handle,bytes,length)!=ESP_OK)return leave(s,fail(s,AG_OTA_IO));
    s->written+=(uint32_t)length;
    result=guard(s,lease);
    if(result==AG_OTA_OK)s->activity_ms=s->last_ms;
    return leave(s,result);
}
static ag_ota_result_t verify_written(ag_ota_installer_t *s,uint64_t lease) {
    uint8_t buffer[AG_OTA_CHUNK],digest[32];size_t written=0;
    psa_hash_operation_t hash=PSA_HASH_OPERATION_INIT;
    if(psa_hash_setup(&hash,PSA_ALG_SHA_256)!=PSA_SUCCESS) {
        if(psa_hash_abort(&hash)!=PSA_SUCCESS)s->cleanup_unknown=true;
        return fail(s,AG_OTA_HASH);
    }
    ag_ota_result_t result=AG_OTA_OK;
    for(uint32_t at=0;at<s->plan.image_bytes;) {
        result=guard(s,lease);if(result!=AG_OTA_OK)break;
        size_t length=s->plan.image_bytes-at;if(length>sizeof(buffer))length=sizeof(buffer);
        if(esp_partition_read(s->target,at,buffer,length)!=ESP_OK){result=fail(s,AG_OTA_IO);break;}
        result=guard(s,lease);if(result!=AG_OTA_OK)break;
        if(psa_hash_update(&hash,buffer,length)!=PSA_SUCCESS){result=fail(s,AG_OTA_HASH);break;}
        at+=(uint32_t)length;
    }
    if(result==AG_OTA_OK && (psa_hash_finish(&hash,digest,sizeof(digest),&written)!=PSA_SUCCESS
        ||written!=32||memcmp(digest,s->plan.sha256,32)!=0))result=fail(s,AG_OTA_HASH);
    if(psa_hash_abort(&hash)!=PSA_SUCCESS) {
        s->cleanup_unknown=true;
        if(result==AG_OTA_OK)result=fail(s,AG_OTA_HASH);
    }
    memset(buffer,0,sizeof(buffer));memset(digest,0,sizeof(digest));
    if(result!=AG_OTA_OK)return result;
    esp_app_desc_t description;
    if(esp_ota_get_partition_description(s->target,&description)!=ESP_OK
        ||memcmp(description.version,s->plan.firmware_version,32)!=0
        ||description.secure_version<s->plan.minimum_secure_version)return fail(s,AG_OTA_IMAGE);
    return guard(s,lease);
}
ag_ota_result_t ag_ota_installer_finish(ag_ota_installer_t *s,uint64_t lease) {
    if(!enter(s))return AG_OTA_BUSY;
    if(s->phase!=AG_OTA_WRITING)return leave(s,AG_OTA_INVALID);
    ag_ota_result_t result=guard(s,lease);if(result!=AG_OTA_OK)return leave(s,result);
    if(s->written!=s->plan.image_bytes)return leave(s,fail(s,AG_OTA_IMAGE));
    if(!partitions(s,false))return leave(s,fail(s,AG_OTA_PARTITIONS));
    if((result=guard(s,lease))!=AG_OTA_OK)return leave(s,result);
    s->handle_active=false; /* SDK end consumes the handle on EVERY result. */
    if(esp_ota_end(s->handle)!=ESP_OK)return leave(s,fail(s,AG_OTA_IMAGE));
    result=guard(s,lease);
    if(result==AG_OTA_OK)result=verify_written(s,lease);
    if(result==AG_OTA_OK)s->phase=AG_OTA_READY;
    return leave(s,result);
}
ag_ota_result_t ag_ota_installer_select(ag_ota_installer_t *s,uint64_t lease) {
    if(!enter(s))return AG_OTA_BUSY;
    if(s->phase!=AG_OTA_READY)return leave(s,AG_OTA_INVALID);
    ag_ota_result_t result=guard(s,lease);if(result!=AG_OTA_OK)return leave(s,result);
    if(!partitions(s,false))return leave(s,fail(s,AG_OTA_PARTITIONS));
    result=verify_written(s,lease);if(result!=AG_OTA_OK)return leave(s,result);
    if(!partitions(s,false))return leave(s,fail(s,AG_OTA_PARTITIONS));
    result=guard(s,lease);if(result!=AG_OTA_OK)return leave(s,result);
    s->selection_may_have_changed=true;
    if(esp_ota_set_boot_partition(s->target)!=ESP_OK)return leave(s,fail(s,AG_OTA_IO));
    result=guard(s,lease);if(result==AG_OTA_OK)s->phase=AG_OTA_SELECTED;
    return leave(s,result);
}
ag_ota_result_t ag_ota_installer_poll(ag_ota_installer_t *s,uint64_t lease) {
    if(!enter(s))return AG_OTA_BUSY;
    ag_ota_result_t result=(s->phase==AG_OTA_WRITING||s->phase==AG_OTA_READY)?guard(s,lease):AG_OTA_INVALID;
    return leave(s,result);
}
ag_ota_result_t ag_ota_installer_cancel(ag_ota_installer_t *s,uint64_t lease) {
    if(!enter(s))return AG_OTA_BUSY;
    if(lease!=s->lease)return leave(s,AG_OTA_DENIED);
    if(s->phase!=AG_OTA_WRITING&&s->phase!=AG_OTA_READY)return leave(s,AG_OTA_INVALID);
    return leave(s,fail(s,AG_OTA_DENIED));
}
