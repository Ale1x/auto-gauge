/* Actual adapter/config/journal; simulated SDK partitions/flash only. */
#include "partition_io.h"
#include "esp_flash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct esp_flash_t {int id;};
struct fixture_iterator {unsigned index;};
static struct esp_flash_t default_chip={1},other_chip={2};
esp_flash_t *esp_flash_default_chip=&default_chip;
static struct {
    esp_partition_t partitions[6];unsigned count,reads,writes,erases,finds,releases;
    struct fixture_iterator iterator;
    uint8_t config[8192],history[8192];
    bool enumeration_error,verify_error,read_error,write_error,erase_error,bad_write,bad_erase,readback_error,reentry;
    bool write_happened,chip_size_error;
    uint32_t chip_bytes;
} fixture;
static ag_partition_store_t storage;
static ag_partition_layout_t layout;
esp_err_t esp_flash_get_size(esp_flash_t *chip,uint32_t *size){assert(chip==&default_chip);*size=fixture.chip_bytes;return fixture.chip_size_error?ESP_FAIL:ESP_OK;}
static uint8_t *memory(const esp_partition_t *p){assert(p==&fixture.partitions[0]||p==&fixture.partitions[1]);return p==&fixture.partitions[0]?fixture.config:fixture.history;}
esp_err_t esp_partition_find_err(esp_partition_type_t type,esp_partition_subtype_t subtype,const char *name,esp_partition_iterator_t *out){
    assert(type==ESP_PARTITION_TYPE_ANY&&subtype==ESP_PARTITION_SUBTYPE_ANY&&name==NULL);++fixture.finds;
    if(fixture.enumeration_error){*out=NULL;return ESP_FAIL;}
    fixture.iterator.index=0;*out=fixture.count?&fixture.iterator:NULL;return ESP_OK;
}
const esp_partition_t *esp_partition_get(esp_partition_iterator_t it){assert(it==&fixture.iterator&&it->index<fixture.count);return &fixture.partitions[it->index];}
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t it){assert(it==&fixture.iterator);if(++it->index>=fixture.count){++fixture.releases;return NULL;}return it;}
void esp_partition_iterator_release(esp_partition_iterator_t it){if(it){assert(it==&fixture.iterator);++fixture.releases;}}
const esp_partition_t *esp_partition_verify(const esp_partition_t *p){return fixture.verify_error?NULL:p;}
static void nested(void){
    if(fixture.reentry){uint8_t data[4]={0};fixture.reentry=false;
        assert(storage.config_io.read(&storage,0,data,sizeof(data))==0);
        assert(storage.history_io.program(&storage,0,data,sizeof(data))==0);
        assert(!storage.config_io.erase(&storage,0,4096));
        assert(!ag_partition_store_bind(&storage,&layout));
    }
}
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t length){
    assert(offset<=p->size&&length<=p->size-offset);++fixture.reads;nested();
    if(fixture.read_error||(fixture.readback_error&&fixture.write_happened))return ESP_FAIL;
    memcpy(out,memory(p)+offset,length);return ESP_OK;
}
esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *bytes,size_t length){
    assert(offset<=p->size&&length<=p->size-offset);++fixture.writes;nested();
    uint8_t *dest=memory(p)+offset;const uint8_t *src=bytes;
    for(size_t i=0;i<length;++i)dest[i]&=src[i];
    fixture.write_happened=true;if(fixture.bad_write&&length)dest[0]^=1;
    return fixture.write_error?ESP_FAIL:ESP_OK;
}
esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t length){
    assert(offset%4096==0&&length==4096&&offset<=p->size&&length<=p->size-offset);++fixture.erases;nested();
    memset(memory(p)+offset,0xff,length);fixture.write_happened=true;
    if(fixture.bad_erase)memory(p)[offset]=0;
    return fixture.erase_error?ESP_FAIL:ESP_OK;
}
static void boot(void){
    memset(&fixture,0,sizeof(fixture));memset(&storage,0,sizeof(storage));memset(&layout,0,sizeof(layout));
    memset(fixture.config,0xff,sizeof(fixture.config));memset(fixture.history,0xff,sizeof(fixture.history));
    fixture.count=4;fixture.chip_bytes=0x400000;
    fixture.partitions[0]=(esp_partition_t){&default_chip,ESP_PARTITION_TYPE_DATA,0x40,0x310000,8192,4096,"ag_cfg_dev",false,false};
    fixture.partitions[1]=(esp_partition_t){&default_chip,ESP_PARTITION_TYPE_DATA,0x41,0x312000,8192,4096,"ag_logs_dev",false,false};
    fixture.partitions[2]=(esp_partition_t){&default_chip,ESP_PARTITION_TYPE_APP,0,0x10000,0x300000,4096,"factory",false,false};
    fixture.partitions[3]=(esp_partition_t){&default_chip,ESP_PARTITION_TYPE_DATA,0,0xd000,8192,4096,"otadata",false,false};
    memcpy(layout.config_label,"ag_cfg_dev",11);memcpy(layout.history_label,"ag_logs_dev",12);layout.config_subtype=0x40;layout.history_subtype=0x41;layout.history_sectors=2;
}
static void bind(void){assert(ag_partition_store_bind(&storage,&layout));assert(fixture.reads==0&&fixture.writes==0&&fixture.erases==0);assert(!ag_partition_store_bind(&storage,&layout));}
static void test_binding(void){
    boot();bind();assert(storage.config_io.context==&storage&&storage.history_io.context==&storage);
    for(unsigned n=0;n<21;++n){boot();switch(n){
        case 0:fixture.count=0;break;case 1:fixture.enumeration_error=true;break;case 2:fixture.verify_error=true;break;
        case 3:fixture.partitions[0].encrypted=true;break;case 4:fixture.partitions[1].readonly=true;break;
        case 5:fixture.partitions[0].erase_size=8192;break;case 6:fixture.partitions[0].address++;break;
        case 7:fixture.partitions[0].size=4096;break;case 8:fixture.partitions[1].size=12288;break;
        case 9:fixture.partitions[0].type=ESP_PARTITION_TYPE_APP;break;case 10:fixture.partitions[1].subtype=0;break;
        case 11:fixture.partitions[0].flash_chip=&other_chip;break;case 12:fixture.partitions[1].address=fixture.partitions[0].address;break;
        case 13:fixture.partitions[2].size+=4096;break;case 14:fixture.partitions[3].address=fixture.partitions[0].address;break;
        case 15:fixture.partitions[4]=fixture.partitions[0];fixture.count=5;break;
        case 16:memset(fixture.partitions[0].label,'x',17);break;
        case 17:fixture.partitions[2].address=UINT32_MAX-4095;fixture.partitions[2].size=8192;break;
        case 18:fixture.chip_size_error=true;break;case 19:fixture.chip_bytes=0;break;case 20:fixture.chip_bytes=0x310000;break;
    }assert(!ag_partition_store_bind(&storage,&layout));assert(fixture.reads==0&&fixture.writes==0&&fixture.erases==0);}
    for(unsigned n=0;n<6;++n){boot();switch(n){case 0:memset(layout.config_label,'x',17);break;case 1:layout.history_subtype=layout.config_subtype;break;case 2:layout.config_subtype=0;break;case 3:layout.history_sectors=1;break;case 4:layout.history_sectors=513;break;case 5:memcpy(layout.history_label,layout.config_label,17);break;}assert(!ag_partition_store_bind(&storage,&layout));}
}
static void test_bounds_and_bytes(void){
    boot();bind();uint8_t bytes[600],out[600];memset(bytes,0xa0,sizeof(bytes));
    assert(storage.config_io.program(&storage,1,bytes,sizeof(bytes))==sizeof(bytes));
    assert(storage.config_io.read(&storage,1,out,sizeof(out))==sizeof(out)&&!memcmp(bytes,out,sizeof(out)));
    assert(storage.history_io.program(&storage,8191,bytes,1)==1);
    unsigned reads=fixture.reads,writes=fixture.writes,erases=fixture.erases;
    assert(storage.config_io.read(&storage,8192,out,1)==0);assert(storage.config_io.read(&storage,UINT32_MAX,out,SIZE_MAX)==0);
    assert(storage.history_io.program(&storage,8191,bytes,2)==0);assert(storage.config_io.program(&storage,0,bytes,0)==0);
    assert(!storage.config_io.erase(&storage,1,4096));assert(!storage.config_io.erase(&storage,0,8192));assert(!storage.history_io.erase_sector(&storage,8192));
    assert(fixture.reads==reads&&fixture.writes==writes&&fixture.erases==erases&&!storage.write_fault);
    assert(storage.config_io.erase(&storage,0,4096));assert(storage.history_io.erase_sector(&storage,4096));
    assert(storage.config_io.read(&storage,0,out,sizeof(out))==sizeof(out));for(size_t i=0;i<sizeof(out);++i)assert(out[i]==0xff);
}
static void test_fault_latch(void){
    for(unsigned n=0;n<8;++n){boot();bind();uint8_t bytes[4]={0,1,2,3},out[4];
        switch(n){case 0:fixture.read_error=true;break;case 1:fixture.write_error=true;break;case 2:fixture.bad_write=true;break;case 3:fixture.readback_error=true;break;case 4:fixture.erase_error=true;break;case 5:fixture.bad_erase=true;break;case 6:fixture.verify_error=true;break;case 7:fixture.partitions[0].address+=4096;break;}
        if(n==4||n==5)assert(!storage.config_io.erase(&storage,0,4096));else assert(storage.config_io.program(&storage,0,bytes,sizeof(bytes))==0);
        assert(storage.write_fault);unsigned writes=fixture.writes,erases=fixture.erases;
        fixture.read_error=fixture.readback_error=false;fixture.write_happened=false;
        assert(storage.history_io.program(&storage,0,bytes,sizeof(bytes))==0&&!storage.history_io.erase_sector(&storage,0));
        assert(fixture.writes==writes&&fixture.erases==erases);
        if(n<6)assert(storage.config_io.read(&storage,0,out,sizeof(out))==sizeof(out));
    }
    boot();bind();uint8_t diagnostic[4];fixture.read_error=true;
    assert(storage.config_io.read(&storage,0,diagnostic,sizeof(diagnostic))==0&&storage.write_fault);
    fixture.read_error=false;assert(storage.history_io.read(&storage,0,diagnostic,sizeof(diagnostic))==sizeof(diagnostic));
    boot();bind();uint8_t zero=0,one=1;assert(storage.config_io.program(&storage,0,&zero,1)==1);
    unsigned calls=fixture.writes;assert(storage.config_io.program(&storage,0,&one,1)==0&&storage.write_fault&&fixture.writes==calls);
}
static void test_reentry_and_core_integration(void){
    boot();bind();fixture.reentry=true;uint8_t bytes[4]={1,2,3,4};assert(storage.config_io.program(&storage,0,bytes,sizeof(bytes))==sizeof(bytes)&&!storage.write_fault);
    boot();bind();ag_config_store_t config;ag_pid_mask_t supported;ag_pid_mask_clear(&supported);ag_pid_mask_set(&supported,0x0c);
    assert(ag_config_store_open(&config,&storage.config_io,&supported)==AG_CONFIG_STORE_NO_VALID_CONFIG&&fixture.writes==0&&fixture.erases==0);
    ag_config_t first={0};first.schema_version=1;first.revision=1;first.dashboard_count=1;first.dashboards[0].id=1;first.dashboards[0].widget_count=1;
    first.dashboards[0].widgets[0]=(ag_widget_t){1,0x0c,{0,0,10000,10000}};first.brightness_mode=AG_BRIGHTNESS_MANUAL;first.manual_percent=30;
    uint8_t record[AG_CONFIG_RECORD_MAX];size_t length=0;assert(ag_config_record_encode(&first,&supported,1,record,sizeof(record),&length)==AG_CONFIG_STORE_OK);
    /* Explicit test provisioning, never an adapter default. */
    assert(storage.config_io.program(&storage,0,record,length)==length);
    assert(storage.config_io.program(&storage,AG_CONFIG_COMMIT_OFFSET,ag_config_commit_marker,4)==4);
    assert(ag_config_store_restore(&config)==AG_CONFIG_STORE_OK);first.revision=2;assert(ag_config_store_commit(&config,1,&first)==AG_CONFIG_STORE_OK);
    ag_journal_history_t history;assert(ag_journal_open(&history,&storage.history_io,2)==AG_JOURNAL_EMPTY);
    uint8_t sid[16]={1},raw[2]={3,4};assert(ag_journal_bootstrap(&history,sid,1,1)==AG_JOURNAL_OK);
    assert(ag_journal_append(&history,0x0c,0x7e8,10,AG_SAMPLE_VALID,raw,sizeof(raw))==AG_JOURNAL_OK);
    ag_history_snapshot_t snapshot;assert(ag_journal_window(&history,sid,0,&snapshot)==AG_JOURNAL_OK&&snapshot.count==1);
    assert(!storage.write_fault);
}
int main(void){test_binding();test_bounds_and_bytes();test_fault_latch();test_reentry_and_core_integration();puts("PASS: 4 partition adapter groups; actual config/journal with behavioral SDK flash");return 0;}
