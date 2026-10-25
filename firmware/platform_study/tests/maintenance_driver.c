/* Production reset adapter + core with behavioral NVS. No physical device. */
#include "maintenance_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static esp_partition_t partition={0x9000,0x4000,ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_DATA_NVS,false,false,4096,"nvs"};
static struct {
    bool space,exists,fail_get,fail_set,fail_commit,bad_readback,fail_erase;
    uint8_t blob[32]; size_t length; unsigned creates,sets,commits,credentials,cfg,logs,fail_log_at;
} f;
static ag_partition_store_t partitions;
static ag_maintenance_store_t storage;
static ag_reset_snapshot_t snapshot;
static ag_reset_io_t io;
static ag_factory_reset_t reset;
static ag_connection_t connection;
static const ag_local_reset_event_t local={AG_RESET_ORIGIN_LOCAL_PHYSICAL,true,true,true};

esp_err_t nvs_open_from_partition(const char *label,const char *space,uint8_t mode,nvs_handle_t *out) {
    assert(!strcmp(label,"nvs") && !strcmp(space,"ag_reset_dev"));
    if(!f.space) { if(mode==NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND; f.space=true; ++f.creates; }
    *out=mode+1; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { assert(h==1 || h==2); }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *length) {
    assert((h==1 || h==2) && !strcmp(key,"state_v1"));
    if(f.fail_get) return ESP_FAIL;
    if(!f.exists) return ESP_ERR_NVS_NOT_FOUND;
    if(*length<f.length) return ESP_FAIL;
    memcpy(out,f.blob,f.length); *length=f.length;
    if(f.bad_readback && h==2) ((uint8_t*)out)[0]^=1;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t length) {
    assert(h==2 && !strcmp(key,"state_v1") && length==32);
    ++f.sets; memcpy(f.blob,data,length); f.length=length; f.exists=true;
    return f.fail_set?ESP_FAIL:ESP_OK; /* Side effect can precede failure. */
}
esp_err_t nvs_commit(nvs_handle_t h) { assert(h==2); ++f.commits; return f.fail_commit?ESP_FAIL:ESP_OK; }
const esp_partition_t *esp_partition_find_first(uint8_t type,uint8_t subtype,const char *label) {
    return type==ESP_PARTITION_TYPE_DATA && subtype==ESP_PARTITION_SUBTYPE_DATA_NVS
        && !strcmp(label,"nvs")?&partition:NULL;
}
const esp_partition_t *esp_partition_verify(const esp_partition_t *p) { return p==&partition?p:NULL; }
static bool erase_credentials(void *context) { assert(context==&f); ++f.credentials; return !f.fail_erase; }
static bool erase_config(void *context,uint32_t offset,size_t length) {
    assert(context==&f && offset<8192 && offset%4096==0 && length==4096);
    ++f.cfg; return !f.fail_erase;
}
static bool erase_history(void *context,uint32_t offset) {
    assert(context==&f && offset<12288 && offset%4096==0); ++f.logs;
    return !f.fail_erase && f.logs!=f.fail_log_at;
}
static void setup(void) {
    memset(&f,0,sizeof(f)); memset(&storage,0,sizeof(storage)); memset(&partitions,0,sizeof(partitions));
    partitions.initialized=true; partitions.config_io.context=&f; partitions.config_io.erase=erase_config;
    partitions.history_io.context=&f; partitions.history_io.erase_sector=erase_history;
    partitions.history_identity.size=12288;
}
static bool bind(void) {
    return ag_maintenance_store_bind(&storage,"nvs",&partitions,&f,erase_credentials,&snapshot,&io);
}
static void restore(unsigned boot) {
    uint8_t sid[16]={0}; sid[0]=(uint8_t)boot;
    assert(bind()); assert(ag_connection_init(&connection,sid,NULL,NULL));
    assert(ag_factory_reset_restore(&reset,&connection,&io,true,&snapshot));
}
static void fresh_boot(unsigned boot) {
    memset(&storage,0,sizeof(storage)); restore(boot);
}
static void baseline(void) { setup(); assert(ag_maintenance_provision_reset("nvs",&local)); restore(1); }
static void begin(void) {
    assert(ag_factory_reset_arm(&reset,&local)==AG_RESET_OK);
    assert(ag_factory_reset_confirm(&reset,&local)==AG_RESET_OK);
}
static void finish(void) {
    while(reset.state==AG_RESET_IN_PROGRESS) assert(ag_factory_reset_step(&reset,true)==AG_RESET_OK);
    assert(reset.state==AG_RESET_IDLE && reset.snapshot.phase==AG_RESET_CLEAN);
}
static void test_provision_and_probe(void) {
    setup(); memset(&snapshot,0xa5,sizeof(snapshot));
    assert(!bind() && !f.space && f.creates==0 && f.sets==0);
    ag_local_reset_event_t remote=local; remote.origin=AG_RESET_ORIGIN_BLE;
    assert(!ag_maintenance_provision_reset("nvs",&remote) && !f.space);
    remote=local; remote.local_confirmed=false;
    assert(!ag_maintenance_provision_reset("nvs",&remote) && !f.space);
    assert(ag_maintenance_provision_reset("nvs",&local));
    assert(!ag_maintenance_provision_reset("nvs",&local) && f.sets==1);
    assert(bind() && snapshot.phase==AG_RESET_CLEAN && f.sets==1);
    assert(!bind());
    for(unsigned byte=0;byte<32;++byte) {
        setup(); assert(ag_maintenance_provision_reset("nvs",&local)); f.blob[byte]^=1;
        assert(!bind() && f.sets==1);
        assert(!ag_maintenance_provision_reset("nvs",&local) && f.sets==1);
    }
}
static void test_reset_phases_resume(void) {
    for(unsigned steps=0;steps<=4;++steps) {
        baseline(); begin();
        for(unsigned i=0;i<steps;++i) assert(ag_factory_reset_step(&reset,true)==AG_RESET_OK);
        fresh_boot(2);
        if(steps<4) {
            assert(connection.maintenance_blocked && reset.state==AG_RESET_FAILED);
            begin(); finish();
        } else assert(!connection.maintenance_blocked);
        assert(f.credentials==1 && f.cfg==2 && f.logs==3);
        fresh_boot(3); assert(reset.state==AG_RESET_IDLE && !connection.maintenance_blocked);
    }
}
static void test_ambiguous_commit_and_fault_latch(void) {
    for(unsigned intent=0;intent<2;++intent) {
        baseline();
        if(intent) assert(ag_factory_reset_arm(&reset,&local)==AG_RESET_OK);
        f.fail_set=true;
        const ag_reset_result_t result=intent?ag_factory_reset_confirm(&reset,&local):ag_factory_reset_arm(&reset,&local);
        assert(result==AG_RESET_IO_FAILED && storage.fault && connection.maintenance_blocked);
        assert(f.credentials==0 && f.cfg==0 && f.logs==0);
        f.fail_set=false; fresh_boot(2);
        assert(reset.snapshot.phase==(intent?AG_RESET_INTENT:AG_RESET_JOURNAL_ARMED));
        begin(); finish(); assert(f.credentials==1 && f.cfg==2 && f.logs==3);
    }
    for(unsigned kind=0;kind<3;++kind) {
        baseline(); begin();
        if(kind==0) f.fail_set=true;
        if(kind==1) f.fail_commit=true;
        if(kind==2) f.bad_readback=true;
        assert(ag_factory_reset_step(&reset,true)==AG_RESET_IO_FAILED);
        assert(storage.fault && connection.maintenance_blocked && f.credentials==1);
        const unsigned sets=f.sets;
        assert(ag_factory_reset_arm(&reset,&local)==AG_RESET_OK);
        assert(ag_factory_reset_confirm(&reset,&local)==AG_RESET_OK);
        assert(ag_factory_reset_step(&reset,true)==AG_RESET_IO_FAILED && f.sets==sets && f.credentials==1);
        f.fail_set=f.fail_commit=f.bad_readback=false;
        fresh_boot(2); assert(reset.snapshot.phase==AG_RESET_CREDENTIALS_ERASED);
        begin(); finish(); assert(f.credentials==1 && f.cfg==2 && f.logs==3);
    }
}
static void test_partial_erase_and_invalid_geometry(void) {
    baseline(); begin();
    assert(ag_factory_reset_step(&reset,true)==AG_RESET_OK);
    f.fail_erase=true;
    assert(ag_factory_reset_step(&reset,true)==AG_RESET_IO_FAILED && storage.fault);
    assert(f.cfg==1 && f.logs==0);
    f.fail_erase=false; fresh_boot(2); begin(); finish();
    assert(f.cfg==3 && f.logs==3); /* Explicit re-confirmation redoes idempotent config erase. */
    baseline(); begin();
    assert(ag_factory_reset_step(&reset,true)==AG_RESET_OK);
    assert(ag_factory_reset_step(&reset,true)==AG_RESET_OK);
    f.fail_log_at=2;
    assert(ag_factory_reset_step(&reset,true)==AG_RESET_IO_FAILED && storage.fault && f.logs==2);
    fresh_boot(2); begin(); finish(); assert(f.logs==5 && f.credentials==1 && f.cfg==2);
    setup(); assert(ag_maintenance_provision_reset("nvs",&local));
    partitions.history_identity.size=4097; assert(!bind());
    partitions.history_identity.size=12288; partition.encrypted=true; assert(!bind()); partition.encrypted=false;
    f.fail_get=true; assert(!bind() && f.sets==1);
}
int main(void) {
    test_provision_and_probe(); test_reset_phases_resume(); test_ambiguous_commit_and_fault_latch();
    test_partial_erase_and_invalid_geometry(); puts("PASS: 4 durable maintenance groups; behavioral NVS, no physical IO");
}
