/* Explicit local factory transition, real installer + behavioral SDK/hash. */
#define main installer_baseline_main
#define esp_partition_find_first baseline_find_partition
#include "installer_driver.c"
#undef esp_partition_find_first
#undef main
static bool factory_mode;
static esp_partition_t factory;
const esp_partition_t *esp_partition_find_first(uint8_t type,uint8_t subtype,const char *label){
    if(factory_mode&&type==ESP_PARTITION_TYPE_APP){
        if(subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY)return &factory;
        if(subtype==ESP_PARTITION_SUBTYPE_APP_OTA_0)return &mock.b;
        if(subtype==ESP_PARTITION_SUBTYPE_APP_OTA_1)return &mock.a;
    }
    return baseline_find_partition(type,subtype,label);
}
static void factory_boot(void){
    factory_mode=false;boot();factory_mode=true;
    factory=(esp_partition_t){0x10000,0x100000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY};
    mock.a=(esp_partition_t){0x210000,0x100000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1};
    mock.b=(esp_partition_t){0x110000,0x100000,ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0};
    mock.running=mock.boot=&factory;mock.next=&mock.b;
}
int main(void){
    assert(installer_baseline_main()==0);
    factory_boot();assert(begin()==AG_OTA_PARTITIONS&&mock.begin_calls==0);
    factory_boot();installer.hooks.allow_factory_bootstrap=true;
    assert(begin()==AG_OTA_OK&&installer.running==&factory&&installer.target==&mock.b);
    write_all();assert(ag_ota_installer_finish(&installer,LEASE)==AG_OTA_OK&&mock.select_calls==0);
    assert(ag_ota_installer_select(&installer,LEASE)==AG_OTA_OK&&mock.boot==&mock.b);
    factory_boot();installer.hooks.allow_factory_bootstrap=true;factory.size=0x200000;
    assert(begin()==AG_OTA_PARTITIONS&&mock.begin_calls==0);
    factory_boot();installer.hooks.allow_factory_bootstrap=true;mock.next=&mock.a;
    assert(begin()==AG_OTA_PARTITIONS&&mock.begin_calls==0);
    factory_boot();installer.hooks.allow_factory_bootstrap=true;mock.allowed=false;
    assert(begin()==AG_OTA_DENIED&&mock.begin_calls==0);
    puts("PASS: explicit factory bootstrap, no implicit opt-in or physical IO");return 0;
}
