#ifndef AG_APPLICATION_H
#define AG_APPLICATION_H
#include "ag_boot_health.h"
#include "ag_discovery.h"
#include "ag_gauge_ui.h"
#include "ag_journal_endpoint.h"
#include "ag_runtime.h"
/* One serialized task owns this object AND every referenced core/adapter. ISR
 * and BLE callbacks enqueue events; they must not enter these APIs directly. */
typedef enum { AG_APP_BLOCKED=0, AG_APP_WAIT_POWER, AG_APP_DISCOVERING,
    AG_APP_CONFIG_REQUIRED, AG_APP_RUNNING, AG_APP_OTA, AG_APP_MAINTENANCE,
    AG_APP_FAULT } ag_application_phase_t;
typedef struct {
    void *context;
    bool (*clock_ms)(void *,uint64_t *);
    bool (*read_policy)(void *,ag_can_policy_t *);
    /* Exactly one core is non-NULL. Pump bounded RX/TX using existing TWAI
     * completion-aware adapter or declared simulation; fresh guards at driver.
     * Return false on terminal/ambiguous IO, true also for no work. */
    bool (*can_step)(void *,ag_discovery_t *,ag_runtime_t *);
    /* Stops/drains hardware references to old core before config/mode switch.
     * Success guarantees no old ISR/completion can touch that core afterward. */
    bool (*can_quiesce)(void *);
    bool (*render)(void *,const ag_gauge_scene_t *);
    /* Optional normalized ALS policy, never inferred from a lux value. */
    bool (*als_percent)(void *,uint8_t *);
    /* Optional additional local boot-health bits. Required product policy is
     * set by boot controller; simulation never invokes mark-valid. */
    uint32_t (*health_mask)(void *);
    /* True only while a Service01 request is queued awaiting actual TX
     * completion. The adapter owns that deadline; FC must return false so it
     * cannot extend the already-running response deadline. */
    bool (*can_request_pending)(void *);
} ag_application_hooks_t;
typedef struct {
    ag_config_store_t *configuration;
    ag_journal_history_t *journal;
    ag_admission_t *admission;
    ag_boot_health_t *boot;
    const char *device_id;
    uint8_t session_id[16];
    ag_can_profile_t profiles[AG_DISCOVERY_ECU_MAX];
    size_t profile_count;
    uint32_t response_timeout_ms,poll_interval_ms,stale_after_ms;
    ag_gauge_ui_options_t ui_options;
    ag_application_hooks_t hooks;
    bool simulation,allow_degraded_logging;
    bool rpc_externally_pumped; /* Same owner bridge handles endpoint steps. */
} ag_application_options_t;
typedef struct {
    ag_application_options_t options;
    ag_discovery_t discovery;
    ag_runtime_t runtime;
    ag_gauge_ui_t ui;
    ag_device_endpoint_t rpc;
    ag_config_t configuration;
    ag_pid_mask_t supported;
    ag_gauge_binding_t bindings[AG_POLL_JOB_CAPACITY];
    size_t binding_count;
    ag_gauge_scene_t scene;
    uint64_t last_ms;
    ag_application_phase_t phase;
    bool initialized,busy,has_time,runtime_started,transport_quiesced;
    bool quiesce_attempted,fault_cleanup_attempted,commissioning_required;
} ag_application_t;
/* Does not format/open stores, provision keys, start radio or touch GPIO. The
 * external storage/boot/reset owners must already be restored successfully.
 * Invalid/missing profile leaves BLOCKED; no default pins or authentication.
 * Unready/readonly initialized storage instead enters local MAINTENANCE with
 * all routes denied. Explicit external provisioning/recovery must finish before
 * leave_exclusive can resume discovery; init itself never writes defaults. */
bool ag_application_init(ag_application_t *,const ag_application_options_t *);
/* Bounded one CAN pump + one journal append + one RPC phase + one projection.
 * Fresh clock/power after every callback/IO. No automatic recovery of faults. */
bool ag_application_tick(ag_application_t *);
/* Trusted physical/local events only. Route entry requires idle connection,
 * quiescent CAN, empty runtime queue and reconciled boot; no wire command. */
bool ag_application_enter_exclusive(ag_application_t *,bool qualified_local,bool ota);
bool ag_application_leave_exclusive(ag_application_t *,bool cleanup_known,bool selection_unchanged);
bool ag_application_route_allowed(void *,bool ota);
ag_gauge_ui_result_t ag_application_tap(ag_application_t *,bool qualified_local);
ag_boot_result_t ag_application_confirm_health(ag_application_t *);
#endif
