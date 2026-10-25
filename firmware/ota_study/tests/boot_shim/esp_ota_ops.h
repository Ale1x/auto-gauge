#ifndef BOOT_TEST_ESP_OTA_OPS_H
#define BOOT_TEST_ESP_OTA_OPS_H
#include "esp_partition.h"
typedef uint32_t esp_ota_handle_t;
typedef enum {ESP_OTA_IMG_VALID=0,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_INVALID} esp_ota_img_states_t;
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_boot_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
#endif
