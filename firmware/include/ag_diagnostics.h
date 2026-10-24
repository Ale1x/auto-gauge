#ifndef AG_DIAGNOSTICS_H
#define AG_DIAGNOSTICS_H
#include "ag_core.h"

#define AG_ISOTP_CAPACITY 256u
#define AG_POLL_JOB_CAPACITY 15u

typedef enum { AG_CAN_ID_11 = 11, AG_CAN_ID_29 = 29 } ag_can_id_format_t;
typedef struct {
    ag_can_id_format_t id_format;
    uint32_t bitrate, request_id, response_id;
} ag_can_profile_t;
typedef struct {
    uint32_t id;
    bool extended, rtr, fd;
    uint8_t length;
    uint8_t data[8];
} ag_can_frame_t;
bool ag_can_profile_valid(const ag_can_profile_t *profile);

typedef enum {
    AG_ISOTP_IDLE = 0, AG_ISOTP_IGNORED, AG_ISOTP_WAITING, AG_ISOTP_COMPLETE,
    AG_ISOTP_MALFORMED, AG_ISOTP_SEQUENCE_ERROR, AG_ISOTP_OVERFLOW,
    AG_ISOTP_TIMEOUT, AG_ISOTP_CLOCK_REGRESSION, AG_ISOTP_UNSUPPORTED,
    AG_ISOTP_BUSY, AG_ISOTP_NOT_INITIALIZED
} ag_isotp_result_t;
typedef struct {
    ag_can_profile_t profile;
    uint32_t timeout_ms;
    uint64_t last_observed_ms, last_frame_ms;
    size_t expected_length, received_length;
    uint8_t next_sequence;
    uint8_t payload[AG_ISOTP_CAPACITY];
    bool initialized, has_time, active, complete;
} ag_isotp_rx_t;
/* Classic CAN + normal ISO-TP addressing only. Never sends FlowControl. */
bool ag_isotp_init(ag_isotp_rx_t *rx, const ag_can_profile_t *profile, uint32_t timeout_ms);
void ag_isotp_reset(ag_isotp_rx_t *rx);
ag_isotp_result_t ag_isotp_poll(ag_isotp_rx_t *rx, uint64_t now_ms);
ag_isotp_result_t ag_isotp_feed(ag_isotp_rx_t *rx, const ag_can_frame_t *frame, uint64_t now_ms);
bool ag_isotp_take(ag_isotp_rx_t *rx, uint8_t *payload, size_t capacity, size_t *length);

typedef enum { AG_UNIT_UNKNOWN = 0, AG_UNIT_PERCENT, AG_UNIT_CELSIUS,
               AG_UNIT_RPM, AG_UNIT_KM_PER_HOUR, AG_UNIT_GRAMS_PER_SECOND } ag_unit_t;
typedef struct {
    uint8_t pid;
    uint32_t ecu_source;
    uint64_t updated_ms;
    ag_sample_status_t status;
    bool has_value;
    double value;
    ag_unit_t unit;
    uint8_t raw_length, raw[AG_RAW_CAPACITY];
    uint8_t negative_response_code;
} ag_measurement_t;
typedef enum { AG_OBD_OK = 0, AG_OBD_MALFORMED, AG_OBD_WRONG_SERVICE,
               AG_OBD_WRONG_PID, AG_OBD_UNSUPPORTED_PID, AG_OBD_NEGATIVE_RESPONSE } ag_obd_result_t;
void ag_measurement_unavailable(ag_measurement_t *sample, ag_sample_status_t status);
ag_obd_result_t ag_obd_parse_measurement(const uint8_t *payload, size_t length,
                                        uint8_t expected_pid, uint32_t ecu_source,
                                        uint64_t now_ms, ag_measurement_t *sample);
typedef struct {
    uint32_t ecu_source;
    ag_pid_mask_t advertised;
    uint8_t pages_seen;
} ag_obd_capabilities_t;
void ag_obd_capabilities_init(ag_obd_capabilities_t *caps, uint32_t ecu_source);
bool ag_obd_capabilities_apply(ag_obd_capabilities_t *caps, uint32_t ecu_source,
                               const uint8_t *payload, size_t length);
void ag_obd_usable_pids(const ag_obd_capabilities_t *caps, ag_pid_mask_t *usable);

typedef struct {
    uint8_t pid;
    ag_can_profile_t profile;
    uint32_t interval_ms, response_timeout_ms, stale_after_ms;
} ag_poll_job_config_t;
typedef struct {
    ag_poll_job_config_t config;
    uint64_t next_due_ms;
    ag_measurement_t sample;
} ag_poll_job_t;
typedef struct {
    ag_poll_job_t jobs[AG_POLL_JOB_CAPACITY];
    size_t count, cursor, pending_index, active_index;
    uint64_t last_ms, active_started_ms;
    ag_isotp_rx_t rx;
    bool has_time, clock_fault;
} ag_scheduler_t;
typedef struct {
    ag_can_profile_t profile;
    ag_obd_request_t request;
} ag_simulated_dispatch_t;
typedef enum { AG_SCHED_RX_IGNORED = 0, AG_SCHED_RX_WAITING, AG_SCHED_RX_ACCEPTED,
               AG_SCHED_RX_ERROR, AG_SCHED_RX_BLOCKED } ag_scheduler_rx_result_t;
void ag_scheduler_init(ag_scheduler_t *scheduler);
bool ag_scheduler_add(ag_scheduler_t *scheduler, const ag_poll_job_config_t *config,
                       const ag_obd_capabilities_t *caps);
bool ag_scheduler_tick(ag_scheduler_t *scheduler, const ag_can_policy_t *policy, uint64_t now_ms);
bool ag_scheduler_prepare(ag_scheduler_t *scheduler, const ag_can_policy_t *policy, uint64_t now_ms);
/* Rechecks current policy. Output is a host handoff, NEVER a physical CAN send. */
bool ag_scheduler_dispatch(ag_scheduler_t *scheduler, const ag_can_policy_t *policy,
                            uint64_t now_ms, ag_simulated_dispatch_t *dispatch);
ag_scheduler_rx_result_t ag_scheduler_receive(ag_scheduler_t *scheduler,
                                              const ag_can_policy_t *policy,
                                              const ag_can_frame_t *frame, uint64_t now_ms);

#endif
