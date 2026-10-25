#ifndef AG_NIMBLE_ENDPOINT_BRIDGE_H
#define AG_NIMBLE_ENDPOINT_BRIDGE_H
#include "nimble_transport.h"
#include "ag_device_endpoint.h"
#include "ota_framed_endpoint.h"
/* Platform callbacks are trusted local functions, never wire booleans. */
typedef struct {
    void *context;
    bool (*awake_allowed)(void *);
    bool (*route_allowed)(void *,bool ota); /* Fresh application mode/health. */
    bool (*random_u32)(void *,uint32_t *); /* Qualified entropy, no fallback. */
    bool (*display_passkey)(void *,const char digits[7]);
    void (*clear_passkey)(void *);
    void (*fault)(void *,ag_nimble_fault_t);
    bool (*owner_tick)(void *); /* Optional; same host task, even idle. */
} ag_nimble_platform_t;
/* OTA is armed only by the serialized local application while no link is
 * present. The callback is fresh local maintenance/trust policy, not a packet
 * field. Owner and options contexts must outlive this bridge. */
typedef struct {
    void *context;
    bool (*allowed)(void *,const ag_connection_lease_t *);
    void (*closed)(void *,ag_ot_phase_t,bool cleanup_unknown,bool selection_may_have_changed);
    uint32_t frame_timeout_ms,request_timeout_ms;
} ag_nimble_ota_options_t;
typedef struct {
    ag_admission_t admission;
    ag_device_endpoint_t endpoint;
    ag_admission_t *admission_ref;
    ag_device_endpoint_t *endpoint_ref;
    ag_ot_endpoint_t ota_endpoint;
    ag_ot_owner_t *ota_owner;
    ag_nimble_ota_options_t ota_options;
    ag_connection_t *connection;
    ag_nimble_platform_t platform;
    ag_connection_lease_t lease;
    uint8_t packet[AG_FRAME_MAX_SIZE];
    uint64_t generation;
    bool initialized,has_lease,displayed,awaiting;
    bool ota_armed,ota_active,ota_close_reported;
} ag_nimble_bridge_t;
/* Static/program lifetime. One owner task, no callback reentry. Connection must
 * have a fresh boot ID, restored clean journal, idle state and no verifier.
 * Config/store/history IO must already be initialized and truthful. No defaults,
 * NVS erase, physical driver or RNG initialization is performed here. */
bool ag_nimble_bridge_init(ag_nimble_bridge_t *,ag_connection_t *,ag_config_store_t *,
    const ag_device_endpoint_options_t *,const ag_admission_config_t *,const ag_nimble_platform_t *);
/* Bind the application's existing admission/RPC, replacing its trusted hooks
 * with this actual NimBLE mapping while both are idle and no window is open.
 * A zero uninitialized RPC may be supplied for deferred discovery startup;
 * advertisements/connect remain denied until it is initialized to this admission.
 * Set application's rpc_externally_pumped=true: the bridge alone advances RPC
 * so received must get final EDONE before commit. Objects outlive bridge. */
bool ag_nimble_bridge_init_bound(ag_nimble_bridge_t *,ag_connection_t *,ag_admission_t *,
    ag_device_endpoint_t *,const ag_nimble_platform_t *);
/* No live switching, automatic fallback, owner reset or trust provisioning.
 * A bridge's OTA endpoint is used once; a closed endpoint cannot be rearmed.
 * prepare_rpc refuses ambiguous cleanup or a possible boot selection change. */
bool ag_nimble_bridge_prepare_ota(ag_nimble_bridge_t *,ag_ot_owner_t *,const ag_nimble_ota_options_t *);
bool ag_nimble_bridge_prepare_rpc(ag_nimble_bridge_t *);
/* Starts actual radio IF invoked. Isolated study app_main never calls it. */
esp_err_t ag_nimble_bridge_start(ag_nimble_bridge_t *);
ag_admission_result_t ag_nimble_bridge_pairing_window(ag_nimble_bridge_t *,bool local,uint64_t now_ms);
#endif
