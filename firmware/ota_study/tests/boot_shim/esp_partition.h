#ifndef BOOT_TEST_ESP_PARTITION_H
#define BOOT_TEST_ESP_PARTITION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_PARTITION_TYPE_APP 0u
#define ESP_PARTITION_TYPE_DATA 1u
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2u
#define ESP_PARTITION_SUBTYPE_APP_FACTORY 0u
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10u
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11u
typedef struct {uint32_t address,size;uint8_t type,subtype;bool encrypted,readonly;uint32_t erase_size;char label[17];} esp_partition_t;
const esp_partition_t *esp_partition_find_first(uint8_t,uint8_t,const char *);
const esp_partition_t *esp_partition_verify(const esp_partition_t *);
esp_err_t esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
#endif
