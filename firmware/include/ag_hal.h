#ifndef AG_HAL_H
#define AG_HAL_H
#include "ag_core.h"

typedef enum { AG_HAL_OK = 0, AG_HAL_NOT_IMPLEMENTED, AG_HAL_BLOCKED } ag_hal_result_t;
/* Snapshot defaults to unknown; CAN traffic does not qualify ignition. */
ag_hal_result_t ag_hal_read_power(ag_can_policy_t *policy);
ag_hal_result_t ag_hal_request_low_power(void);
ag_hal_result_t ag_hal_display_apply(const ag_config_t *config);
ag_hal_result_t ag_hal_log_persist(const ag_log_record_t *record);
ag_hal_result_t ag_hal_ble_start(void);
ag_hal_result_t ag_hal_ota_begin(void);
#endif
