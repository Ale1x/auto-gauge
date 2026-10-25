#ifndef AG_GAUGE_UI_H
#define AG_GAUGE_UI_H
#include "ag_diagnostics.h"
#define AG_GAUGE_UI_READING_MAX AG_POLL_JOB_CAPACITY
/* Explicit source choice supplied by the owner; UI never picks an ECU from a
 * duplicate PID or turns advertised support into a fabricated reading. */
typedef struct {uint8_t pid;uint32_t ecu_source;} ag_gauge_binding_t;
typedef enum {AG_GAUGE_POWER_UNAVAILABLE=0,AG_GAUGE_POWER_OFF,AG_GAUGE_POWER_ON} ag_gauge_power_t;
typedef enum {AG_GAUGE_BRIGHTNESS_UNAVAILABLE=0,AG_GAUGE_BRIGHTNESS_MANUAL,AG_GAUGE_BRIGHTNESS_AUTO} ag_gauge_brightness_t;
typedef struct {
    int32_t id;
    uint8_t pid;
    ag_rect_t rect;
    uint32_t ecu_source;
    ag_sample_status_t quality;
    ag_unit_t unit;
    double value;
    bool has_source,has_value;
} ag_gauge_widget_t;
typedef struct {
    uint32_t configuration_revision;
    int32_t dashboard_id;
    uint8_t dashboard_count,widget_count;
    ag_gauge_widget_t widgets[AG_MAX_WIDGETS];
    ag_gauge_power_t power;
    ag_gauge_brightness_t brightness;
    uint8_t brightness_percent;
} ag_gauge_scene_t;
typedef struct {uint32_t maximum_sample_age_ms,tap_debounce_ms;} ag_gauge_ui_options_t;
typedef struct {
    const ag_config_t *configuration;
    const ag_pid_mask_t *supported;
    const ag_gauge_binding_t *bindings;
    size_t binding_count;
    const ag_measurement_t *readings;
    size_t reading_count;
    const ag_can_policy_t *power_policy;
    uint64_t now_ms;
    bool als_valid;
    uint8_t als_percent; /* trusted normalized0..100, not a lux-curve guess */
} ag_gauge_input_t;
typedef struct {
    ag_gauge_ui_options_t options;
    ag_config_t configuration;
    size_t selected;
    uint64_t last_ms,last_tap_ms;
    bool initialized,has_configuration,has_time,clock_fault,has_tap;
} ag_gauge_ui_t;
typedef enum {AG_GAUGE_UI_OK=0,AG_GAUGE_UI_INVALID,AG_GAUGE_UI_NOT_READY,
    AG_GAUGE_UI_DENIED,AG_GAUGE_UI_DEBOUNCED,AG_GAUGE_UI_CLOCK_FAULT,
    AG_GAUGE_UI_REVISION_CONFLICT} ag_gauge_ui_result_t;
/* Zero/uninitialized or explicitly quiesced instance, one serialized owner.
 * Inputs/snapshots must remain immutable during calls and must not alias output
 * or controller. No panel, GPIO, touch interrupt, allocation or storage IO. */
bool ag_gauge_ui_init(ag_gauge_ui_t *,const ag_gauge_ui_options_t *);
/* Safe diagnostic scene (no widgets/value, power/brightness unavailable) on
 * failure, never a stale previous scene. Revisions cannot silently go backwards
 * or change meaning at the same revision. Fresh boot/vehicle context needs a
 * fresh owner/controller; sample times are monotonic within that same session. */
ag_gauge_ui_result_t ag_gauge_ui_project(ag_gauge_ui_t *,const ag_gauge_input_t *,ag_gauge_scene_t *);
/* qualified_local must come from a trusted local touch adapter, never RPC.
 * Policy is current, not cached from the preceding projection. */
ag_gauge_ui_result_t ag_gauge_ui_tap(ag_gauge_ui_t *,bool qualified_local,const ag_can_policy_t *,uint64_t now_ms);
#endif
