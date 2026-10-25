#ifndef AG_TWAI_PORT_H
#define AG_TWAI_PORT_H
#include "ag_discovery.h"
#include "ag_runtime.h"
#include "esp_twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#define AG_TWAI_EVENT_CAPACITY 16u
/* Local board profile only, never wire input. No default GPIO or auto bitrate. */
typedef struct {
    int tx_gpio,rx_gpio;
    uint32_t bitrate,rx_filter_id,rx_filter_mask,tx_timeout_ms,maximum_rx_age_ms;
    ag_can_id_format_t id_format;
    bool hardware_profile_qualified;
    void *policy_context;
    bool (*read_policy)(void *,ag_can_policy_t *);
} ag_twai_port_config_t;
typedef enum {AG_TWAI_INITIALIZED=0,AG_TWAI_RUNNING,AG_TWAI_FAULT,AG_TWAI_CLOSED} ag_twai_port_phase_t;
typedef enum {AG_TWAI_TX_NONE=0,AG_TWAI_TX_QUEUED,AG_TWAI_TX_COMPLETE,AG_TWAI_TX_FAILED,AG_TWAI_TX_UNKNOWN} ag_twai_tx_state_t;
typedef enum {AG_TWAI_OK=0,AG_TWAI_NO_WORK,AG_TWAI_QUEUED,AG_TWAI_COMPLETED,
    AG_TWAI_INVALID,AG_TWAI_NOT_READY,AG_TWAI_BUSY,AG_TWAI_DENIED,AG_TWAI_TIMEOUT,
    AG_TWAI_IO,AG_TWAI_BUS_OFF,AG_TWAI_CLOCK_FAULT} ag_twai_result_t;
typedef struct {
    uint8_t kind;
    bool success;
    uint64_t observed_ms;
    ag_can_frame_t frame;
} ag_twai_event_t;
typedef struct {
    ag_twai_port_config_t config;
    twai_node_handle_t node;
    StaticQueue_t queue_storage;
    uint8_t queue_bytes[AG_TWAI_EVENT_CAPACITY*sizeof(ag_twai_event_t)];
    QueueHandle_t events;
    twai_frame_t tx_frame;
    uint8_t tx_bytes[8];
    ag_twai_event_t deferred[AG_TWAI_EVENT_CAPACITY];
    size_t deferred_count;
    ag_runtime_t *runtime;
    ag_discovery_t *discovery;
    uint64_t last_ms,tx_started_ms,tx_completed_ms,last_cf_ms;
    uint64_t rx_accepted,rx_rejected,tx_completed;
    ag_twai_port_phase_t phase;
    ag_twai_tx_state_t tx_state;
    ag_twai_result_t fault_reason;
    atomic_bool irq_fault;
    bool initialized,busy,enabled,has_time,pending,has_cf,cleanup_unknown,disable_attempted,delete_attempted;
} ag_twai_port_t;
/* One static/internal-DRAM object for program lifetime, including failed start.
 * Exclusive node/core owner. ISR callbacks only copy bounded events; no core
 * business logic/IO/logging in ISR. No reinit/restart/recover. No CAN activity in
 * init; start explicitly configures GPIO/node and can participate in the bus. */
bool ag_twai_port_init(ag_twai_port_t *,const ag_twai_port_config_t *);
ag_twai_result_t ag_twai_port_start(ag_twai_port_t *);
/* Only these builders may transmit, always Service01 reads or its generated FC.
 * SDK ESP_OK means queued, never completed. Poll consumes actual TX callbacks. */
ag_twai_result_t ag_twai_port_send_discovery(ag_twai_port_t *,ag_discovery_t *);
ag_twai_result_t ag_twai_port_send_runtime(ag_twai_port_t *,ag_runtime_t *);
ag_twai_result_t ag_twai_port_send_flow_control(ag_twai_port_t *,ag_runtime_t *);
ag_twai_result_t ag_twai_port_poll(ag_twai_port_t *);
ag_twai_result_t ag_twai_port_close(ag_twai_port_t *);
/* Clock is esp_timer_get_time()/1000 for both core handoffs and ISR observation.
 * Bound discovery/runtime must use this same boot epoch; never mixed sessions. */
#endif
