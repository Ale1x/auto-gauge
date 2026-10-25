/* Production storage+core; explicitly behavioral NVS/SDK + NONCRYPTO hash model. */
#include "boot_storage.h"
#include "psa/crypto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define LENGTH 2080u
static esp_partition_t run={0x10000,0x100000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,false,false,4096,"ota_0"};
static esp_partition_t target={0x110000,0x100000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,false,false,4096,"ota_1"};
static esp_partition_t nvs_partition={0x9000,0x4000,ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,false,false,4096,"nvs"};
static struct {
    bool namespace_exists,blob_exists,fail_set,fail_commit,fail_get,bad_length,hash_abort_error,state_error,mark_error,mark_changes;
    uint8_t blob[96],image[LENGTH];esp_ota_img_states_t state;const esp_partition_t *running,*boot;
    unsigned namespace_writes,sets,commits,marks,partition_reads,bytes_read;char order[32];unsigned order_size;
} f;
static ag_ota_boot_storage_t storage;
static ag_boot_hooks_t hooks;
static ag_boot_health_t health;
static void event(char c){assert(f.order_size<sizeof(f.order));f.order[f.order_size++]=c;}
esp_err_t nvs_open_from_partition(const char *label,const char *space,uint8_t mode,nvs_handle_t *out){
    assert(!strcmp(label,"nvs")&&!strcmp(space,"ag_boot_dev")&&(mode==NVS_READONLY||mode==NVS_READWRITE));
    if(!f.namespace_exists){if(mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;f.namespace_exists=true;++f.namespace_writes;}
    *out=mode+1;return ESP_OK;
}
void nvs_close(nvs_handle_t handle){assert(handle==1||handle==2);}
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *length){
    assert(handle==2&&!strcmp(key,"state_v1")&&*length==96);
    if(f.fail_get)return ESP_FAIL;if(!f.blob_exists)return ESP_ERR_NVS_NOT_FOUND;
    memcpy(out,f.blob,96);*length=f.bad_length?95:96;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *bytes,size_t length){
    assert(handle==2&&!strcmp(key,"state_v1")&&length==96);++f.sets;event('W');memcpy(f.blob,bytes,96);f.blob_exists=true;
    return f.fail_set?ESP_FAIL:ESP_OK; /* Error may follow real commit, as pinned SDK. */
}
esp_err_t nvs_commit(nvs_handle_t handle){assert(handle==2);++f.commits;return f.fail_commit?ESP_FAIL:ESP_OK;}
const esp_partition_t *esp_partition_find_first(uint8_t type,uint8_t subtype,const char *label){
    return type==ESP_PARTITION_TYPE_DATA&&subtype==ESP_PARTITION_SUBTYPE_DATA_NVS&&label&&!strcmp(label,"nvs")?&nvs_partition:NULL;
}
const esp_partition_t *esp_partition_verify(const esp_partition_t *p){return p==&run||p==&target||p==&nvs_partition?p:NULL;}
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t n){
    assert((p==&run||p==&target)&&offset<=LENGTH&&n<=LENGTH-offset&&n<=256);
    ++f.partition_reads;f.bytes_read+=(unsigned)n;memcpy(out,f.image+offset,n);return ESP_OK;
}
const esp_partition_t *esp_ota_get_running_partition(void){return f.running;}
const esp_partition_t *esp_ota_get_boot_partition(void){return f.boot;}
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *p){assert(p==f.running);return p==&run?&target:&run;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *state){assert(p==f.running);*state=f.state;return f.state_error?ESP_FAIL:ESP_OK;}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void){++f.marks;event('M');if(!f.mark_error||f.mark_changes)f.state=ESP_OTA_IMG_VALID;return f.mark_error?ESP_FAIL:ESP_OK;}
psa_status_t psa_crypto_init(void){return PSA_SUCCESS;}
psa_status_t psa_hash_setup(psa_hash_operation_t *op,uint32_t algorithm){assert(algorithm==PSA_ALG_SHA_256);memset(op,0,sizeof(*op));op->active=1;return PSA_SUCCESS;}
psa_status_t psa_hash_update(psa_hash_operation_t *op,const uint8_t *bytes,size_t n){assert(op->active);for(size_t i=0;i<n;++i)op->digest[(op->count++)%32]^=bytes[i];return PSA_SUCCESS;}
psa_status_t psa_hash_finish(psa_hash_operation_t *op,uint8_t *out,size_t capacity,size_t *length){assert(op->active&&capacity==32);memcpy(out,op->digest,32);*length=32;op->active=0;return PSA_SUCCESS;}
psa_status_t psa_hash_abort(psa_hash_operation_t *op){op->active=0;return f.hash_abort_error?1:PSA_SUCCESS;}
static void hash(uint8_t out[32]){memset(out,0,32);for(size_t i=0;i<LENGTH;++i)out[i%32]^=f.image[i];}
static void boot(void){
    memset(&f,0,sizeof(f));memset(&storage,0,sizeof(storage));memset(&health,0,sizeof(health));memset(&hooks,0,sizeof(hooks));
    f.running=f.boot=&run;f.state=ESP_OTA_IMG_VALID;for(unsigned i=0;i<LENGTH;++i)f.image[i]=(uint8_t)(i*29+3);
}
static void bind(void){assert(ag_ota_boot_storage_bind(&storage,"nvs",&hooks));assert(!ag_ota_boot_storage_bind(&storage,"nvs",&hooks));}
static void baseline(void){boot();assert(ag_ota_boot_storage_provision_namespace("nvs"));assert(f.namespace_writes==1&&f.sets==0);bind();assert(ag_boot_health_open(&health,&hooks,15)==AG_BOOT_EMPTY);assert(ag_boot_health_provision(&health,9)==AG_BOOT_OK);}
static void trial(void){
    baseline();uint8_t digest[32];hash(digest);
    ag_manifest_verified_t verified={0};verified.release_counter=10;verified.plan.image_bytes=LENGTH;memcpy(verified.plan.sha256,digest,32);
    assert(ag_ota_boot_prepare_selection(&health,&verified,&target));assert(f.sets==2&&f.bytes_read==LENGTH);
    f.running=f.boot=&target;f.state=ESP_OTA_IMG_PENDING_VERIFY;
}
static void test_namespace_and_floor_order(void){
    boot();assert(!ag_ota_boot_storage_bind(&storage,"nvs",&hooks));assert(!f.namespace_exists&&f.namespace_writes==0&&f.sets==0);
    trial();assert(ag_boot_health_confirm(&health,15)==AG_BOOT_OK);uint64_t floor;assert(ag_boot_health_floor(&health,&floor)&&floor==10);
    assert(f.sets==4&&f.marks==1&&f.order_size==5&&!memcmp(f.order,"WWWMW",5));assert(f.partition_reads>=9&&f.bytes_read%LENGTH==0);
}
static void test_nvs_false_after_commit_and_mark_ambiguity(void){
    for(unsigned which=0;which<3;++which){trial();if(which==0)f.fail_set=true;if(which==1)f.fail_commit=true;if(which==2){f.mark_error=true;f.mark_changes=true;}
        assert(ag_boot_health_confirm(&health,15)==AG_BOOT_UNKNOWN);assert(f.marks==(which==2?1u:0u));
        uint8_t bytes[96];assert(which==2||hooks.read(&storage,bytes)==AG_BOOT_LOAD_IO);
        assert(ag_boot_health_confirm(&health,15)==AG_BOOT_INVALID);
        f.fail_set=f.fail_commit=f.mark_error=false;memset(&storage,0,sizeof(storage));memset(&health,0,sizeof(health));bind();
        assert(ag_boot_health_open(&health,&hooks,15)==AG_BOOT_OK);
        if(which==2)assert(ag_boot_health_reconcile(&health)==AG_BOOT_OK);
        else assert(ag_boot_health_confirm(&health,15)==AG_BOOT_OK);
        uint64_t floor;assert(ag_boot_health_floor(&health,&floor)&&floor==10&&f.marks==1);
    }
}
static void test_hash_and_metadata_rejections(void){
    baseline();uint8_t expected[32];hash(expected);expected[0]^=1;assert(!hooks.target_matches(&storage,target.address,LENGTH,expected));
    hash(expected);f.hash_abort_error=true;assert(!hooks.target_matches(&storage,target.address,LENGTH,expected));assert(f.sets==1&&f.marks==0);
    f.hash_abort_error=false;assert(!hooks.target_matches(&storage,run.address,LENGTH,expected));
    f.bad_length=true;uint8_t out[96];assert(hooks.read(&storage,out)==AG_BOOT_LOAD_IO);
    boot();f.namespace_exists=true;f.state_error=true;bind();assert(ag_boot_health_open(&health,&hooks,15)==AG_BOOT_EMPTY);assert(ag_boot_health_provision(&health,9)==AG_BOOT_POLICY&&f.sets==0);
}
static void test_explicit_factory_observation(void){
    boot();run.subtype=ESP_PARTITION_SUBTYPE_APP_FACTORY;f.state_error=true;
    assert(ag_ota_boot_storage_provision_namespace("nvs"));bind();
    ag_boot_observation_t observed;
    assert(hooks.observe(&storage,0,&observed)&&observed.state==AG_BOOT_IMAGE_FACTORY);
    assert(ag_boot_health_open(&health,&hooks,15)==AG_BOOT_EMPTY);
    assert(ag_boot_health_provision(&health,9)==AG_BOOT_POLICY&&f.sets==0);
    memset(&health,0,sizeof(health));hooks.allow_factory_bootstrap=true;
    assert(ag_boot_health_open(&health,&hooks,15)==AG_BOOT_EMPTY);
    assert(ag_boot_health_provision(&health,9)==AG_BOOT_OK&&f.sets==1);
    uint8_t digest[32];hash(digest);
    assert(ag_boot_health_stage(&health,10,target.address,LENGTH,digest)==AG_BOOT_OK);
    f.running=f.boot=&target;f.state_error=false;f.state=ESP_OTA_IMG_PENDING_VERIFY;
    assert(ag_boot_health_confirm(&health,15)==AG_BOOT_OK&&f.marks==1);
    run.subtype=ESP_PARTITION_SUBTYPE_APP_OTA_0;
}
int main(void){test_namespace_and_floor_order();test_nvs_false_after_commit_and_mark_ambiguity();test_hash_and_metadata_rejections();test_explicit_factory_observation();puts("PASS: 4 boot-storage binding groups; NONCRYPTO hash and behavioral NVS/SDK");return 0;}
