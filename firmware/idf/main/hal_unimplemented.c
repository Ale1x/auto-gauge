#include "ag_hal.h"

ag_hal_result_t ag_hal_read_power(ag_can_policy_t *policy) {
    ag_can_policy_init(policy);
    return AG_HAL_NOT_IMPLEMENTED;
}
ag_hal_result_t ag_hal_request_low_power(void) { return AG_HAL_NOT_IMPLEMENTED; }
ag_hal_result_t ag_hal_display_apply(const ag_config_t *config) {
    (void)config; return AG_HAL_NOT_IMPLEMENTED;
}
ag_hal_result_t ag_hal_log_persist(const ag_log_record_t *record) {
    (void)record; return AG_HAL_NOT_IMPLEMENTED;
}
ag_hal_result_t ag_hal_ble_start(void) { return AG_HAL_BLOCKED; }
ag_hal_result_t ag_hal_ota_begin(void) { return AG_HAL_BLOCKED; }
