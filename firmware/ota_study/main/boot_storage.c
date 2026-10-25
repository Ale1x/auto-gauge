#include "boot_storage.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "psa/crypto.h"
#include <string.h>
static ag_boot_load_t read_record(void *context,uint8_t out[96]){
    ag_ota_boot_storage_t *s=context;if(!s->initialized||s->fault)return AG_BOOT_LOAD_IO;
    size_t length=96;esp_err_t error=nvs_get_blob(s->handle,"state_v1",out,&length);
    if(error==ESP_ERR_NVS_NOT_FOUND)return AG_BOOT_LOAD_EMPTY;
    return error==ESP_OK&&length==96?AG_BOOT_LOAD_OK:AG_BOOT_LOAD_IO;
}
static bool write_record(void *context,const uint8_t bytes[96]){
    ag_ota_boot_storage_t *s=context;if(!s->initialized||s->fault)return false;
    if(nvs_set_blob(s->handle,"state_v1",bytes,96)!=ESP_OK||nvs_commit(s->handle)!=ESP_OK){s->fault=true;return false;}
    return true;
}
static bool digest(const esp_partition_t *partition,uint32_t length,uint8_t out[32]){
    if(partition==NULL||esp_partition_verify(partition)==NULL||length==0||length>partition->size||length>16u*1024u*1024u)return false;
    psa_hash_operation_t op=PSA_HASH_OPERATION_INIT;uint8_t chunk[256];bool ok=false;size_t size=0;
    if(psa_crypto_init()!=PSA_SUCCESS||psa_hash_setup(&op,PSA_ALG_SHA_256)!=PSA_SUCCESS)goto done;
    for(uint32_t at=0;at<length;){size_t count=length-at;if(count>sizeof(chunk))count=sizeof(chunk);
        if(esp_partition_read(partition,at,chunk,count)!=ESP_OK||psa_hash_update(&op,chunk,count)!=PSA_SUCCESS)goto done;
        at+=(uint32_t)count;
    }
    ok=psa_hash_finish(&op,out,32,&size)==PSA_SUCCESS&&size==32;
 done:
    if(psa_hash_abort(&op)!=PSA_SUCCESS)ok=false;
    return ok;
}
static bool observe(void *context,uint32_t size,ag_boot_observation_t *out){
    ag_ota_boot_storage_t *s=context;if(!s->initialized||s->fault)return false;
    const esp_partition_t *run=esp_ota_get_running_partition(),*boot=esp_ota_get_boot_partition();
    esp_ota_img_states_t state;
    if(run==NULL||boot==NULL||esp_partition_verify(run)==NULL||esp_partition_verify(boot)==NULL)return false;
    bool factory=run->type==ESP_PARTITION_TYPE_APP&&run->subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY;
    if(!factory&&esp_ota_get_state_partition(run,&state)!=ESP_OK)return false;
    memset(out,0,sizeof(*out));out->running_address=run->address;out->boot_address=boot->address;
    out->state=factory?AG_BOOT_IMAGE_FACTORY:state==ESP_OTA_IMG_VALID?AG_BOOT_IMAGE_VALID:state==ESP_OTA_IMG_PENDING_VERIFY?AG_BOOT_IMAGE_PENDING:AG_BOOT_IMAGE_INVALID;
    return size==0||digest(run,size,out->digest);
}
static bool target_matches(void *context,uint32_t address,uint32_t size,const uint8_t expected[32]){
    ag_ota_boot_storage_t *s=context;if(!s->initialized||s->fault)return false;
    const esp_partition_t *run=esp_ota_get_running_partition();if(run==NULL||address==run->address)return false;
    const esp_partition_t *target=esp_ota_get_next_update_partition(run);uint8_t hash[32];
    return target!=NULL&&target->address==address&&target->type==ESP_PARTITION_TYPE_APP
        &&(target->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_0||target->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_1)
        &&digest(target,size,hash)&&memcmp(hash,expected,32)==0;
}
static bool mark_valid(void *context){ag_ota_boot_storage_t *s=context;return s->initialized&&!s->fault&&esp_ota_mark_app_valid_cancel_rollback()==ESP_OK;}
bool ag_ota_boot_storage_bind(ag_ota_boot_storage_t *s,const char *label,ag_boot_hooks_t *hooks){
    if(s==NULL||label==NULL||hooks==NULL||s->initialized)return false;
    size_t n=0;while(n<17&&label[n])++n;if(n==0||n>16)return false;
    const esp_partition_t *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,label);
    if(partition==NULL||esp_partition_verify(partition)==NULL||partition->encrypted||partition->readonly||partition->size<12288||partition->address%4096||partition->size%4096)return false;
    nvs_handle_t existing;
    if(nvs_open_from_partition(label,"ag_boot_dev",NVS_READONLY,&existing)!=ESP_OK)return false;
    nvs_close(existing);
    nvs_handle_t handle;if(nvs_open_from_partition(label,"ag_boot_dev",NVS_READWRITE,&handle)!=ESP_OK)return false;
    memset(s,0,sizeof(*s));s->handle=handle;s->initialized=true;
    *hooks=(ag_boot_hooks_t){s,read_record,write_record,observe,target_matches,mark_valid,false};return true;
}
bool ag_ota_boot_prepare_selection(void *context,const ag_manifest_verified_t *verified,const esp_partition_t *target){
    if(context==NULL||verified==NULL||target==NULL||esp_partition_verify(target)==NULL)return false;
    ag_boot_health_t *boot=context;uint64_t floor;
    return ag_boot_health_floor(boot,&floor)&&verified->release_counter>floor
        &&ag_boot_health_stage(boot,verified->release_counter,target->address,verified->plan.image_bytes,verified->plan.sha256)==AG_BOOT_OK;
}

bool ag_ota_boot_storage_provision_namespace(const char *label){
    if(label==NULL)return false;
    size_t n=0;while(n<17&&label[n])++n;if(n==0||n>16)return false;
    const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,label);
    if(p==NULL||esp_partition_verify(p)==NULL||p->encrypted||p->readonly||p->size<12288||p->address%4096||p->size%4096)return false;
    nvs_handle_t handle;if(nvs_open_from_partition(label,"ag_boot_dev",NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t result=nvs_commit(handle);nvs_close(handle);return result==ESP_OK;
}
