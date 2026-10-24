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
bool ag_hal_verify_session(void *context, const ag_client_id_t *client,
                           const ag_connection_lease_t *lease,
                           const uint8_t *proof, size_t proof_length) {
    (void)context; (void)client; (void)lease; (void)proof; (void)proof_length;
    return false;
}
ag_hal_result_t ag_hal_reset_write_journal(const ag_reset_snapshot_t *snapshot) {
    (void)snapshot; return AG_HAL_NOT_IMPLEMENTED;
}
ag_hal_result_t ag_hal_reset_erase_credentials(void) { return AG_HAL_NOT_IMPLEMENTED; }
ag_hal_result_t ag_hal_reset_erase_configuration(void) { return AG_HAL_NOT_IMPLEMENTED; }
ag_hal_result_t ag_hal_reset_erase_logs(void) { return AG_HAL_NOT_IMPLEMENTED; }
