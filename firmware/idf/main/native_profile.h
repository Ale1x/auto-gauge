#ifndef AG_NATIVE_PROFILE_H
#define AG_NATIVE_PROFILE_H
#include "ag_application.h"
#include "ag_gauge_renderer.h"
#include "partition_io.h"
#include "twai_port.h"
#include "manifest_verify.h"

typedef enum { AG_LOCAL_NONE=0, AG_LOCAL_TAP, AG_LOCAL_PAIR,
    AG_LOCAL_OTA, AG_LOCAL_RESET_ARM, AG_LOCAL_RESET_CONFIRM } ag_native_event_kind_t;
typedef struct {
    ag_native_event_kind_t kind;
    bool physically_qualified,confirmed;
} ag_native_local_event_t;
/* Compile-time/local board integration, NEVER a phone/downloaded structure.
 * No shipped implementation returns a product profile. Strings, callbacks and
 * contexts live until reboot. Trust and quantitative health are locally owned.
 * All callbacks after prepare execute on the single NimBLE host owner task;
 * ISRs enqueue local events, never call application/storage/bridge directly. */
typedef struct {
    void *context;
    const char *device_id;
    const char *nvs_label;
    ag_partition_layout_t partitions;
    ag_twai_port_config_t can;
    ag_can_profile_t ecus[AG_DISCOVERY_ECU_MAX];
    size_t ecu_count;
    ag_manifest_trust_t trust;
    ag_admission_config_t admission;
    uint32_t required_health,response_timeout_ms,poll_interval_ms,stale_after_ms;
    ag_gauge_ui_options_t ui;
    uint16_t panel_width,panel_height;
    uint32_t maximum_frame_ms; /* Explicit board IO bound, 1..1000ms. */
    uint32_t ota_idle_timeout_ms,ota_total_timeout_ms;
    bool allow_degraded_logging,allow_factory_bootstrap;
    /* Explicit board preparation (rails/NVS already qualified). Must never
     * start CAN/radio or auto erase/provision reset/boot/config/trust on
     * missing/corrupt storage. Partial failure must leave power safe. */
    bool (*prepare)(void *);
    bool (*read_policy)(void *,ag_can_policy_t *);
    bool (*radio_allowed)(void *);
    bool (*usb_power_valid)(void *);
    bool (*random_u32)(void *,uint32_t *);
    bool (*local_event)(void *,ag_native_local_event_t *); /* false means no event */
    bool (*display_power)(void *,ag_gauge_power_t,ag_gauge_brightness_t,uint8_t);
    bool (*write_row)(void *,uint16_t,const uint16_t *,size_t);
    bool (*display_passkey)(void *,const char[7]);
    void (*clear_passkey)(void *);
    uint32_t (*health_mask)(void *); /* Entire fresh passed mask, no added bits. */
    bool (*als_percent)(void *,uint8_t *); /* optional; no invented lux curve */
    bool (*ota_allowed)(void *,const ag_connection_lease_t *);
    /* Required only for RESET. Must confirm both persistent bonds and all RAM/
     * privacy caches gone, preserving reset/boot/trust namespaces. NULL denies
     * reset. NimBLE generic unpair/store_clear alone cannot prove this. */
    bool (*erase_credentials)(void *);
} ag_native_board_profile_t;

/* Weak default returns NULL. A reviewed local board source overrides it. */
const ag_native_board_profile_t *ag_native_board_profile(void);
/* One-shot. Missing profile returns false before any board/NVS/radio operation.
 * With a profile composes real SDK adapters and starts the single NimBLE host
 * timer, which advances discovery/app/renderer/TWAI and the shared RPC/OTA route.
 * No auto provisioning, key generation, flash fallback, select or reboot. */
bool ag_native_profile_start(void);
/* Bootstrap-task-only recovery for a valid interrupted reset. No NimBLE/CAN
 * starts in this branch. Poll trusted local events and step one reset phase;
 * completion leaves erased stores requiring explicit commissioning/reboot. */
bool ag_native_profile_recovery_required(void);
bool ag_native_profile_recovery_tick(void);
#endif
