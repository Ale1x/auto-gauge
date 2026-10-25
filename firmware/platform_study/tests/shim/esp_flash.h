#ifndef TEST_ESP_FLASH_H
#define TEST_ESP_FLASH_H
#include "esp_partition.h"
extern esp_flash_t *esp_flash_default_chip;
esp_err_t esp_flash_get_size(esp_flash_t *,uint32_t *);
#endif
