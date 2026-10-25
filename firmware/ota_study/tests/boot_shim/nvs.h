#ifndef BOOT_TEST_NVS_H
#define BOOT_TEST_NVS_H
#include "esp_partition.h"
typedef uint32_t nvs_handle_t;
#define NVS_READONLY 0u
#define NVS_READWRITE 1u
#define ESP_ERR_NVS_NOT_FOUND -2
esp_err_t nvs_open_from_partition(const char *,const char *,uint8_t,nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_commit(nvs_handle_t);
#endif
