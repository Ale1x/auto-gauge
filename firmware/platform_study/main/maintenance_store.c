#include "maintenance_store.h"
#include <string.h>

#define RESET_BYTES 32u
static const char reset_namespace[] = "ag_reset_dev";
static const char reset_key[] = "state_v1";

static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i=0; i<4; ++i) p[i]=(uint8_t)(n>>(8u*i));
}
static uint32_t get32(const uint8_t *p) {
    uint32_t n=0; for (unsigned i=0; i<4; ++i) n|=(uint32_t)p[i]<<(8u*i); return n;
}
static void encode(const ag_reset_snapshot_t *s, uint8_t out[RESET_BYTES]) {
    memset(out,0,RESET_BYTES); memcpy(out,"AGRS",4); put32(out+4,s->format_version);
    for (unsigned i=0; i<8; ++i) out[8+i]=(uint8_t)(s->reset_generation>>(8u*i));
    out[16]=(uint8_t)s->phase; out[17]=s->completed_mask; put32(out+28,s->checksum);
}
static bool decode(const uint8_t in[RESET_BYTES], ag_reset_snapshot_t *out) {
    if (memcmp(in,"AGRS",4)) return false;
    for (unsigned i=18; i<28; ++i) if(in[i]) return false;
    ag_reset_snapshot_t s={.format_version=get32(in+4),.phase=(ag_reset_phase_t)in[16],
        .completed_mask=in[17],.checksum=get32(in+28)};
    for (unsigned i=0; i<8; ++i) s.reset_generation|=(uint64_t)in[8+i]<<(8u*i);
    if (!ag_reset_snapshot_valid(&s)) return false;
    *out=s; return true;
}
static bool label_valid(const char *label) {
    if (label==NULL) return false;
    size_t n=0; while(n<17 && label[n]) ++n;
    if(n==0 || n>16) return false;
    const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,label);
    return p!=NULL && esp_partition_verify(p)==p && !p->readonly && !p->encrypted
        && p->size>=12288 && p->address%4096==0 && p->size%4096==0;
}
static bool read_snapshot(nvs_handle_t handle, ag_reset_snapshot_t *out) {
    uint8_t bytes[RESET_BYTES]; size_t length=sizeof(bytes);
    return nvs_get_blob(handle,reset_key,bytes,&length)==ESP_OK
        && length==sizeof(bytes) && decode(bytes,out);
}
static bool write_checked(nvs_handle_t handle,const ag_reset_snapshot_t *snapshot) {
    uint8_t expected[RESET_BYTES], actual[RESET_BYTES]; size_t length=sizeof(actual);
    encode(snapshot,expected);
    return nvs_set_blob(handle,reset_key,expected,sizeof(expected))==ESP_OK
        && nvs_commit(handle)==ESP_OK
        && nvs_get_blob(handle,reset_key,actual,&length)==ESP_OK
        && length==sizeof(actual) && memcmp(expected,actual,sizeof(actual))==0;
}
static bool enter(ag_maintenance_store_t *s) {
    if(s==NULL || !s->initialized || s->fault || s->busy) return false;
    s->busy=true; return true;
}
static bool leave(ag_maintenance_store_t *s,bool success) {
    s->busy=false; if(!success) s->fault=true; return success;
}
static bool write_snapshot(void *context,const ag_reset_snapshot_t *snapshot) {
    ag_maintenance_store_t *s=context;
    if(snapshot==NULL || !ag_reset_snapshot_valid(snapshot) || !enter(s)) return false;
    return leave(s,write_checked(s->handle,snapshot));
}
static bool credentials(void *context) {
    ag_maintenance_store_t *s=context; if(!enter(s)) return false;
    return leave(s,s->erase_credentials(s->credentials_context));
}
static bool erase_configuration(void *context) {
    ag_maintenance_store_t *s=context; if(!enter(s)) return false;
    bool ok=s->partitions->initialized && !s->partitions->write_fault;
    for(uint32_t at=0; ok && at<AG_CONFIG_STORE_BYTES; at+=AG_CONFIG_SLOT_BYTES)
        ok=s->partitions->config_io.erase(s->partitions->config_io.context,at,AG_CONFIG_SLOT_BYTES);
    return leave(s,ok);
}
static bool erase_logs(void *context) {
    ag_maintenance_store_t *s=context; if(!enter(s)) return false;
    bool ok=s->partitions->initialized && !s->partitions->write_fault;
    for(uint32_t at=0; ok && at<s->partitions->history_identity.size; at+=AG_JOURNAL_SECTOR_BYTES)
        ok=s->partitions->history_io.erase_sector(s->partitions->history_io.context,at);
    return leave(s,ok);
}
bool ag_maintenance_provision_reset(const char *label,const ag_local_reset_event_t *event) {
    if(event==NULL || event->origin!=AG_RESET_ORIGIN_LOCAL_PHYSICAL || !event->physical_qualified
        || !event->usb_power_valid || !event->local_confirmed || !label_valid(label)) return false;
    nvs_handle_t existing; uint8_t bytes[RESET_BYTES]; size_t length=sizeof(bytes);
    esp_err_t status=nvs_open_from_partition(label,reset_namespace,NVS_READONLY,&existing);
    if(status==ESP_OK) {
        status=nvs_get_blob(existing,reset_key,bytes,&length); nvs_close(existing);
        if(status!=ESP_ERR_NVS_NOT_FOUND) return false;
    } else if(status!=ESP_ERR_NVS_NOT_FOUND) return false;
    nvs_handle_t handle;
    if(nvs_open_from_partition(label,reset_namespace,NVS_READWRITE,&handle)!=ESP_OK) return false;
    const ag_reset_snapshot_t clean=ag_reset_clean_snapshot();
    const bool success=write_checked(handle,&clean); nvs_close(handle); return success;
}
bool ag_maintenance_store_bind(ag_maintenance_store_t *s,const char *label,
    ag_partition_store_t *partitions,void *credentials_context,ag_reset_erase_t erase,
    ag_reset_snapshot_t *snapshot,ag_reset_io_t *io) {
    if(s==NULL || s->initialized || partitions==NULL || !partitions->initialized
        || partitions->write_fault || partitions->config_io.erase==NULL
        || partitions->history_io.erase_sector==NULL || erase==NULL || snapshot==NULL || io==NULL
        || partitions->history_identity.size<2*AG_JOURNAL_SECTOR_BYTES
        || partitions->history_identity.size>AG_JOURNAL_MAX_SECTORS*AG_JOURNAL_SECTOR_BYTES
        || partitions->history_identity.size%AG_JOURNAL_SECTOR_BYTES || !label_valid(label)) return false;
    nvs_handle_t probe; ag_reset_snapshot_t loaded;
    if(nvs_open_from_partition(label,reset_namespace,NVS_READONLY,&probe)!=ESP_OK) return false;
    const bool valid=read_snapshot(probe,&loaded); nvs_close(probe);
    if(!valid) return false;
    nvs_handle_t handle;
    if(nvs_open_from_partition(label,reset_namespace,NVS_READWRITE,&handle)!=ESP_OK) return false;
    memset(s,0,sizeof(*s)); s->handle=handle; s->partitions=partitions;
    s->credentials_context=credentials_context; s->erase_credentials=erase; s->initialized=true;
    *snapshot=loaded; *io=(ag_reset_io_t){s,write_snapshot,credentials,erase_configuration,erase_logs};
    return true;
}
