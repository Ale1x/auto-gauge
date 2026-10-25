#ifndef AG_DEVICE_ENDPOINT_H
#define AG_DEVICE_ENDPOINT_H
#include "ag_admission.h"
#include "ag_config_store.h"
#include "ag_rpc.h"
#include "ag_stream.h"

typedef enum { AG_DEVICE_HISTORY_OK=0, AG_DEVICE_HISTORY_SESSION_MISMATCH,
    AG_DEVICE_HISTORY_UNAVAILABLE, AG_DEVICE_HISTORY_NOT_RETAINED,
    AG_DEVICE_HISTORY_READ_ONLY, AG_DEVICE_HISTORY_CURSOR_AHEAD } ag_device_history_result_t;
/* Read-only bounded typed snapshot. No client cursor table or erase/ACK hook.
 * Must validate requested SID/cursor and return a self-contained valid snapshot;
 * may return a <=64-record window. No retained mutable/input pointers. */
typedef ag_device_history_result_t (*ag_device_history_read_t)(void *,const uint8_t sid[16],
    uint64_t after,ag_history_snapshot_t *);
/* Enumerate actual retained sessions, strictly greater than optional cursor.
 * No record payloads, client state or erase. Empty valid catalog is allowed. */
typedef ag_device_history_result_t (*ag_device_history_sessions_t)(void *,const uint8_t after_sid_or_null[16],
    uint8_t limit,ag_history_sessions_page_t *);
typedef struct {
    void *history_context;
    ag_device_history_read_t history_read;
    const char *device_id; /* Trusted stable printable ASCII1..128, must outlive endpoint. */
    ag_pid_mask_t supported;
    ag_rpc_persistence_t config_persistence; /* Trustworthy adapter assertion: RAM=VOLATILE. */
    uint32_t frame_timeout_ms,request_timeout_ms;
    bool simulation;
    ag_device_history_sessions_t history_sessions; /* Optional; NULL => not_implemented. */
} ag_device_endpoint_options_t;
typedef enum { AG_DEVICE_OK=0,AG_DEVICE_INVALID,AG_DEVICE_AUTH_REQUIRED,AG_DEVICE_STALE,
    AG_DEVICE_REPLAY,AG_DEVICE_BUSY,AG_DEVICE_PROTOCOL_ERROR,AG_DEVICE_OUTPUT_SHORT,
    AG_DEVICE_NO_OUTPUT,AG_DEVICE_BACKEND_ERROR } ag_device_result_t;
typedef enum { AG_DEVICE_IDLE=0,AG_DEVICE_PENDING,AG_DEVICE_APPLY_COMMIT,AG_DEVICE_FINAL } ag_device_phase_t;
typedef struct {
    ag_admission_t *admission;ag_config_store_t *configuration;
    ag_device_endpoint_options_t options;
    ag_connection_lease_t lease;
    ag_stream_t stream;
    ag_rpc_request_t pending;
    uint8_t output[AG_FRAME_MAX_SIZE];size_t output_length;
    uint32_t last_request_id,pending_id;
    uint64_t now_ms,request_started_ms;
    ag_device_phase_t phase;
    ag_device_result_t handler_result;
    bool initialized,attached,has_lease,stepping;
} ag_device_endpoint_t;
/* One owner task/lock; no hook reentry. Init does not open/admit a link, start
 * radio or provision defaults. Admission/store/backend lifetimes exceed this. */
bool ag_device_endpoint_init(ag_device_endpoint_t *,ag_admission_t *,ag_config_store_t *,
    const ag_device_endpoint_options_t *);
ag_device_result_t ag_device_endpoint_attach(ag_device_endpoint_t *,const ag_connection_lease_t *,uint64_t now_ms);
/* Stale leases never cancel the currently attached peer. Matching disconnect
 * cancels uncommitted work and invalidates admission; physical close is adapter's job. */
ag_device_result_t ag_device_endpoint_disconnect(ag_device_endpoint_t *,const ag_connection_lease_t *,uint64_t now_ms);
ag_device_result_t ag_device_endpoint_feed(ag_device_endpoint_t *,const ag_connection_lease_t *,
    const uint8_t *,size_t,uint64_t now_ms);
/* One phase per call. Apply emits RECEIVED first; commit is blocked until that
 * frame is consumed, allowing explicit disconnect/cancellation before commit. */
ag_device_result_t ag_device_endpoint_step(ag_device_endpoint_t *,uint64_t now_ms);
/* Supply a FRESH monotonic now_ms after IO, never reuse the precommit timestamp.
 * One framed response<=4112 bytes, no partial consume. Security/lease rechecked
 * on every emission. No private hello or unauthenticated error payload is sent. */
ag_device_result_t ag_device_endpoint_take_output(ag_device_endpoint_t *,const ag_connection_lease_t *,
    uint64_t now_ms,uint8_t *,size_t,size_t *written);
#endif
