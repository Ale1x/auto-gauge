#ifndef AG_CORE_H
#define AG_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AG_CONFIG_VERSION 1u
#define AG_MAX_DASHBOARDS 3u
#define AG_MAX_WIDGETS 5u
#define AG_LOG_CAPACITY 64u
#define AG_RAW_CAPACITY 8u
#define AG_LOG_MAX_SERIES 15u
#define AG_SESSION_ID_BYTES 16u
#define AG_PID_MASK_WORDS 8u

/* Internal C objects are not packed wire formats. See shared/ for transport. */
typedef struct {
    int32_t x, y, width, height;
} ag_rect_t;
typedef struct {
    int32_t id, pid;
    ag_rect_t rect;
} ag_widget_t;
typedef struct {
    int32_t id;
    size_t widget_count;
    ag_widget_t widgets[AG_MAX_WIDGETS];
} ag_dashboard_t;
typedef enum { AG_BRIGHTNESS_MANUAL = 0, AG_BRIGHTNESS_AUTO = 1 } ag_brightness_mode_t;
typedef struct {
    uint32_t schema_version;
    int64_t revision;
    size_t dashboard_count;
    ag_dashboard_t dashboards[AG_MAX_DASHBOARDS];
    ag_brightness_mode_t brightness_mode;
    int32_t manual_percent;
} ag_config_t;
typedef enum {
    AG_CONFIG_OK = 0, AG_CONFIG_NULL, AG_CONFIG_VERSION_UNSUPPORTED,
    AG_CONFIG_DASHBOARD_COUNT, AG_CONFIG_WIDGET_COUNT,
    AG_CONFIG_BRIGHTNESS, AG_CONFIG_RECTANGLE, AG_CONFIG_PID_UNSUPPORTED,
    AG_CONFIG_REVISION, AG_CONFIG_ID, AG_CONFIG_DUPLICATE_ID
} ag_config_result_t;
typedef struct { uint32_t words[AG_PID_MASK_WORDS]; } ag_pid_mask_t;
void ag_pid_mask_clear(ag_pid_mask_t *mask);
void ag_pid_mask_set(ag_pid_mask_t *mask, uint8_t pid);
bool ag_pid_mask_has(const ag_pid_mask_t *mask, uint8_t pid);
ag_config_result_t ag_config_validate(const ag_config_t *config,
                                      const ag_pid_mask_t *supported);
/* Atomic only within the owner task, not thread-safe across concurrent writers. */
ag_config_result_t ag_config_apply(ag_config_t *active, const ag_config_t *candidate,
                                   const ag_pid_mask_t *supported);

typedef enum { AG_POWER_UNKNOWN = 0, AG_POWER_NONE, AG_POWER_USB_ONLY,
               AG_POWER_OBD, AG_POWER_OBD_AND_USB } ag_power_source_t;
typedef enum { AG_IGN_UNKNOWN = 0, AG_IGN_OFF, AG_IGN_QUALIFIED_ON } ag_ignition_t;
typedef struct {
    uint32_t ecu_source;
    uint64_t last_ms;
    uint8_t pid;
    bool used;
} ag_log_series_t;
typedef struct {
    ag_power_source_t power;
    ag_ignition_t ignition;
    bool hardware_validated;
    bool rails_valid;
    bool standby_requested;
    bool bus_off;
    bool diagnostic_session_active;
} ag_can_policy_t;
typedef enum { AG_READ_MEASUREMENT = 0, AG_READ_DISCOVERY = 1 } ag_read_kind_t;
typedef struct { uint8_t bytes[2]; size_t length; } ag_obd_request_t;
void ag_can_policy_init(ag_can_policy_t *policy);
bool ag_can_tx_allowed(const ag_can_policy_t *policy);
bool ag_obd_pid_implemented(uint8_t pid);
/* The only request builder fixes service=01. No arbitrary service/raw CAN API. */
bool ag_obd_make_read_request(const ag_can_policy_t *policy,
                              const ag_pid_mask_t *supported,
                              ag_read_kind_t kind, uint8_t pid,
                              ag_obd_request_t *request);

typedef enum { AG_SAMPLE_VALID = 0, AG_SAMPLE_UNSUPPORTED, AG_SAMPLE_STALE,
               AG_SAMPLE_TIMEOUT, AG_SAMPLE_ERROR, AG_SAMPLE_UNAVAILABLE } ag_sample_status_t;
typedef struct {
    uint8_t session_id[AG_SESSION_ID_BYTES];
    uint64_t sequence;
    uint64_t monotonic_ms;
    uint32_t ecu_source;
    uint8_t pid;
    ag_sample_status_t status;
    uint8_t raw_length;
    uint8_t raw[AG_RAW_CAPACITY];
} ag_log_record_t;
typedef struct {
    uint8_t session_id[AG_SESSION_ID_BYTES];
    ag_pid_mask_t configured;
    ag_log_record_t records[AG_LOG_CAPACITY];
    ag_log_series_t series[AG_LOG_MAX_SERIES];
    uint64_t next_sequence;
    uint64_t last_accepted_ms;
    uint64_t dropped_records;
    uint64_t first_dropped_sequence;
    uint64_t last_dropped_sequence;
    size_t head, count;
    bool enabled;
} ag_log_queue_t;
typedef enum { AG_LOG_ACCEPTED = 0, AG_LOG_OVERWROTE_OLDEST,
               AG_LOG_RATE_LIMITED, AG_LOG_DISABLED, AG_LOG_NOT_CONFIGURED,
               AG_LOG_INVALID, AG_LOG_CLOCK_REGRESSION, AG_LOG_SEQUENCE_EXHAUSTED,
               AG_LOG_SERIES_FULL } ag_log_result_t;
bool ag_log_queue_init(ag_log_queue_t *queue,
                       const uint8_t session_id[AG_SESSION_ID_BYTES],
                       const ag_pid_mask_t *configured);
void ag_log_queue_stop(ag_log_queue_t *queue);
/* Single owner task only. No heap, blocking IO, flash or ISR use. */
ag_log_result_t ag_log_queue_append(ag_log_queue_t *queue, uint8_t pid,
                                   uint32_t ecu_source, uint64_t monotonic_ms,
                                   ag_sample_status_t status,
                                   const uint8_t *raw, size_t raw_length);
bool ag_log_queue_pop(ag_log_queue_t *queue, ag_log_record_t *record);

typedef struct {
    uint8_t session_id[AG_SESSION_ID_BYTES];
    uint64_t boot_us, last_us;
    bool valid;
} ag_session_clock_t;
bool ag_session_clock_init(ag_session_clock_t *clock,
                           const uint8_t session_id[AG_SESSION_ID_BYTES], uint64_t boot_us);
bool ag_session_clock_stamp(ag_session_clock_t *clock, uint64_t now_us,
                            uint64_t *monotonic_ms);

#endif
