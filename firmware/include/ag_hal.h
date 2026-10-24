#ifndef AG_HAL_H
#define AG_HAL_H
#include "ag_core.h"
#include "ag_connection.h"

typedef enum { AG_HAL_OK = 0, AG_HAL_NOT_IMPLEMENTED, AG_HAL_BLOCKED } ag_hal_result_t;
/* Snapshot defaults to unknown; CAN traffic does not qualify ignition. */
ag_hal_result_t ag_hal_read_power(ag_can_policy_t *policy);
ag_hal_result_t ag_hal_request_low_power(void);
ag_hal_result_t ag_hal_display_apply(const ag_config_t *config);
ag_hal_result_t ag_hal_log_persist(const ag_log_record_t *record);
ag_hal_result_t ag_hal_ble_start(void);
ag_hal_result_t ag_hal_ota_begin(void);
bool ag_hal_verify_session(void *context, const ag_client_id_t *client,
                           const ag_connection_lease_t *lease,
                           const uint8_t *proof, size_t proof_length);
ag_hal_result_t ag_hal_reset_write_journal(const ag_reset_snapshot_t *snapshot);
ag_hal_result_t ag_hal_reset_erase_credentials(void);
ag_hal_result_t ag_hal_reset_erase_configuration(void);
ag_hal_result_t ag_hal_reset_erase_logs(void);
#endif
