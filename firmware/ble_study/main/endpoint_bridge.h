#ifndef AG_NIMBLE_ENDPOINT_BRIDGE_H
#define AG_NIMBLE_ENDPOINT_BRIDGE_H
#include "nimble_transport.h"
#include "ag_device_endpoint.h"
/* Platform callbacks are trusted local functions, never wire booleans. */
typedef struct {
    void *context;
    bool (*awake_allowed)(void *);
    bool (*random_u32)(void *,uint32_t *); /* Qualified entropy, no fallback. */
    bool (*display_passkey)(void *,const char digits[7]);
    void (*clear_passkey)(void *);
    void (*fault)(void *,ag_nimble_fault_t);
} ag_nimble_platform_t;
typedef struct {
    ag_admission_t admission;
    ag_device_endpoint_t endpoint;
    ag_connection_t *connection;
    ag_nimble_platform_t platform;
    ag_connection_lease_t lease;
    uint8_t packet[AG_FRAME_MAX_SIZE];
    uint64_t generation;
    bool initialized,has_lease,displayed,awaiting;
} ag_nimble_bridge_t;
/* Static/program lifetime. One owner task, no callback reentry. Connection must
 * have a fresh boot ID, restored clean journal, idle state and no verifier.
 * Config/store/history IO must already be initialized and truthful. No defaults,
 * NVS erase, physical driver or RNG initialization is performed here. */
bool ag_nimble_bridge_init(ag_nimble_bridge_t *,ag_connection_t *,ag_config_store_t *,
    const ag_device_endpoint_options_t *,const ag_admission_config_t *,const ag_nimble_platform_t *);
/* Starts actual radio IF invoked. Isolated study app_main never calls it. */
esp_err_t ag_nimble_bridge_start(ag_nimble_bridge_t *);
ag_admission_result_t ag_nimble_bridge_pairing_window(ag_nimble_bridge_t *,bool local,uint64_t now_ms);
#endif
