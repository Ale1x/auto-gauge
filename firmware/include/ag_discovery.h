#ifndef AG_DISCOVERY_H
#define AG_DISCOVERY_H
#include "ag_diagnostics.h"
#define AG_DISCOVERY_ECU_MAX 8u
/* Known physical address candidates on ONE selected bus variant. This module
 * never guesses bitrate/format or discovers identities from wire data. */
typedef enum { AG_DISCOVERY_READY=0,AG_DISCOVERY_PREPARED,AG_DISCOVERY_WAITING,
    AG_DISCOVERY_COMPLETE,AG_DISCOVERY_ABORTED,AG_DISCOVERY_CLOCK_FAULT } ag_discovery_phase_t;
typedef enum { AG_DISCOVERY_ECU_UNSEEN=0,AG_DISCOVERY_ECU_COMPLETE,
    AG_DISCOVERY_ECU_TIMEOUT,AG_DISCOVERY_ECU_MALFORMED,AG_DISCOVERY_ECU_NEGATIVE,
    AG_DISCOVERY_ECU_TX_FAILED } ag_discovery_ecu_result_t;
typedef struct {
    ag_can_profile_t profile;
    ag_obd_capabilities_t capabilities;
    ag_discovery_ecu_result_t result;
    uint8_t negative_response_code;
} ag_discovery_ecu_t;
typedef struct {
    ag_discovery_ecu_t ecus[AG_DISCOVERY_ECU_MAX];
    size_t count,current;
    uint32_t response_timeout_ms;
    uint64_t last_ms,started_ms;
    uint8_t page;
    ag_isotp_rx_t rx;
    ag_discovery_phase_t phase;
    bool initialized,has_time;
} ag_discovery_t;
typedef enum { AG_DISCOVERY_RX_IGNORED=0,AG_DISCOVERY_RX_ACCEPTED,
    AG_DISCOVERY_RX_ERROR,AG_DISCOVERY_RX_BLOCKED } ag_discovery_rx_result_t;
/* Zero/uninitialized or explicitly quiesced object only, no concurrent access.
 * Inputs must not alias destination storage. No heap or IO. */
bool ag_discovery_init(ag_discovery_t *,const ag_can_profile_t *,size_t count,uint32_t response_timeout_ms);
/* Any policy revocation/clock regression is terminal for this discovery: callers
 * must explicitly initialize a fresh attempt rather than replay old work. */
bool ag_discovery_tick(ag_discovery_t *,const ag_can_policy_t *,uint64_t now_ms);
bool ag_discovery_prepare(ag_discovery_t *,const ag_can_policy_t *,uint64_t now_ms);
/* Current policy rechecked at handoff. Host dispatch, not physical TX: adapter
 * rechecks at driver and reports failed/uncertain TX through tx_failed. */
bool ag_discovery_dispatch(ag_discovery_t *,const ag_can_policy_t *,uint64_t now_ms,ag_simulated_dispatch_t *);
ag_discovery_rx_result_t ag_discovery_receive(ag_discovery_t *,const ag_can_policy_t *,const ag_can_frame_t *,uint64_t now_ms);
bool ag_discovery_tx_failed(ag_discovery_t *,const ag_can_policy_t *,uint64_t now_ms);
/* Available only after all bounded candidates finish, including explicit timeout
 * and partial-page failures. Never fabricates usable PIDs from missing pages. */
bool ag_discovery_usable(const ag_discovery_t *,ag_pid_mask_t *);
bool ag_discovery_capabilities(const ag_discovery_t *,size_t ecu,ag_obd_capabilities_t *);
typedef struct {uint8_t maximum_dashboards,widgets_per_dashboard,manual_percent;} ag_default_config_options_t;
typedef enum {AG_DEFAULT_CONFIG_OK=0,AG_DEFAULT_CONFIG_INVALID,AG_DEFAULT_CONFIG_NOT_READY,
    AG_DEFAULT_CONFIG_EMPTY} ag_default_config_result_t;
/* Pure proposal from actual usable PIDs: no implicit flash bootstrap/provision.
 * Count/layout/brightness are explicit development options, not product policy.
 * Revision1, manual brightness, canonical bounded inscribed-square row layout. */
ag_default_config_result_t ag_discovery_build_defaults(const ag_discovery_t *,const ag_default_config_options_t *,ag_config_t *);
#endif
