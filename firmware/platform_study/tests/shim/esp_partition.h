#ifndef TEST_ESP_PARTITION_H
#define TEST_ESP_PARTITION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef struct esp_flash_t esp_flash_t;
typedef uint8_t esp_partition_type_t;
typedef uint8_t esp_partition_subtype_t;
#define ESP_PARTITION_TYPE_APP 0u
#define ESP_PARTITION_TYPE_DATA 1u
#define ESP_PARTITION_TYPE_ANY 0xffu
#define ESP_PARTITION_SUBTYPE_ANY 0xffu
typedef struct {
    esp_flash_t *flash_chip;
    esp_partition_type_t type;
    esp_partition_subtype_t subtype;
    uint32_t address,size,erase_size;
    char label[17];
    bool encrypted,readonly;
} esp_partition_t;
typedef struct fixture_iterator *esp_partition_iterator_t;
esp_err_t esp_partition_find_err(esp_partition_type_t,esp_partition_subtype_t,const char *,esp_partition_iterator_t *);
const esp_partition_t *esp_partition_get(esp_partition_iterator_t);
esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t);
void esp_partition_iterator_release(esp_partition_iterator_t);
const esp_partition_t *esp_partition_verify(const esp_partition_t *);
esp_err_t esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
esp_err_t esp_partition_write(const esp_partition_t *,size_t,const void *,size_t);
esp_err_t esp_partition_erase_range(const esp_partition_t *,size_t,size_t);
#endif
