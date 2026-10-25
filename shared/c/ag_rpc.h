#ifndef AG_RPC_H
#define AG_RPC_H
#include "ag_config_json.h"
#include "ag_history.h"
#define AG_RPC_MAX_PAYLOAD 4096u
#define AG_RPC_MAX_DEPTH 8u
#define AG_RPC_MAX_TOKENS 512u
#define AG_RPC_MAX_STRING 64u

typedef enum {
    AG_RPC_HELLO = 0, AG_RPC_CONFIG_GET, AG_RPC_CONFIG_VALIDATE,
    AG_RPC_CONFIG_APPLY, AG_RPC_LOGS_READ, AG_RPC_LOGS_ACK, AG_RPC_LOGS_SESSIONS
} ag_rpc_operation_t;
typedef struct {
    ag_rpc_operation_t operation;
    ag_config_t configuration;
    int32_t expected_revision;
    uint8_t session_id[AG_SESSION_ID_BYTES];
    uint64_t sequence; /* after_sequence or acknowledged_through, never double. */
    uint8_t limit;
    bool has_after_session_id;
    uint8_t after_session_id[AG_SESSION_ID_BYTES];
} ag_rpc_request_t;
typedef enum {
    AG_RPC_OK = 0, AG_RPC_ARGUMENT, AG_RPC_SIZE, AG_RPC_SYNTAX, AG_RPC_TYPE,
    AG_RPC_NUMBER, AG_RPC_STRING_SUBSET, AG_RPC_DEPTH, AG_RPC_TOKENS,
    AG_RPC_UNKNOWN_KEY, AG_RPC_DUPLICATE_KEY, AG_RPC_FIELDS, AG_RPC_CONFIG,
    AG_RPC_OUTPUT_SHORT, AG_RPC_UNSUPPORTED, AG_RPC_HISTORY_INVALID,
    AG_RPC_SESSION_MISMATCH, AG_RPC_CURSOR_AHEAD
} ag_rpc_result_t;
typedef struct {
    size_t offset;
    ag_config_json_result_t configuration_error;
    ag_config_result_t validation;
} ag_rpc_error_t;
/* Payload only. Frame kind/request_id, lease, authentication and authorization
 * must be checked separately. Parse never changes output on failure.
 * Printable ASCII strings/escapes only; schema is strict, unknown keys rejected.
 * Refer to RPC_C_V0.1.md. No heap or retained pointers. */
ag_rpc_result_t ag_rpc_request_parse(const uint8_t *, size_t, const ag_pid_mask_t *,
                                     ag_rpc_request_t *, ag_rpc_error_t *);
const char *ag_rpc_operation_name(ag_rpc_operation_t);

typedef enum {
    AG_RPC_REPLY_HELLO = 0, AG_RPC_REPLY_CURRENT, AG_RPC_REPLY_VALIDATED,
    AG_RPC_REPLY_RECEIVED, AG_RPC_REPLY_APPLIED, AG_RPC_REPLY_REJECTED,
    AG_RPC_REPLY_LOGS_ACK, AG_RPC_REPLY_LOGS_READ, AG_RPC_REPLY_LOGS_SESSIONS
} ag_rpc_reply_kind_t;
typedef enum { AG_RPC_PERSISTENCE_UNSPECIFIED=0, AG_RPC_VOLATILE, AG_RPC_DURABLE } ag_rpc_persistence_t;
typedef enum {
    AG_RPC_ERROR_UNSPECIFIED=0, AG_RPC_ERROR_REVISION_CONFLICT,
    AG_RPC_ERROR_SESSION_MISMATCH, AG_RPC_ERROR_HISTORY_UNAVAILABLE,
    AG_RPC_ERROR_INVALID_CONFIGURATION, AG_RPC_ERROR_INVALID_HISTORY_REQUEST,
    AG_RPC_ERROR_NOT_IMPLEMENTED, AG_RPC_ERROR_UNAUTHORIZED, AG_RPC_ERROR_INVALID_REQUEST,
    AG_RPC_ERROR_INVALID_MESSAGE, AG_RPC_ERROR_UNSUPPORTED_PID
} ag_rpc_wire_error_t;
#define AG_HISTORY_SESSIONS_MAX 16u
/* Typed read-only cursor page; next_after_session_id is allzero iff !has_more.
 * SIDs are nonzero, unique, lexicographically sorted and greater than request cursor. */
typedef struct {
    uint8_t session_ids[AG_HISTORY_SESSIONS_MAX][AG_SESSION_ID_BYTES];
    uint8_t count;
    bool has_more;
    uint8_t next_after_session_id[AG_SESSION_ID_BYTES];
} ag_history_sessions_page_t;
typedef struct {
    ag_rpc_reply_kind_t kind;
    bool simulation, authenticated;
    const char *device_id; /* HELLO only: NUL terminated, printable ASCII1..128. */
    const ag_pid_mask_t *supported; /* HELLO only; not an admission authority. */
    const ag_config_t *configuration; /* CURRENT only, copied during encode. */
    ag_rpc_persistence_t persistence; /* CURRENT/APPLIED: explicit caller assertion. */
    int32_t applied_revision;
    ag_rpc_wire_error_t error;
    uint8_t session_id[AG_SESSION_ID_BYTES];
    uint64_t acknowledged_through;
    const ag_history_snapshot_t *history; /* LOGS_READ: immutable typed copy owned by caller. */
    uint64_t after_sequence;
    uint8_t limit; /* LOGS_READ/LOGS_SESSIONS1..16. */
    const ag_history_sessions_page_t *sessions;
    bool has_after_session_id;
    uint8_t after_session_id[AG_SESSION_ID_BYTES];
} ag_rpc_reply_t;
/* Serialization only; caller chooses truthful phase/auth/persistence after the
 * corresponding verified action. No storage, state transition or inference.
 * Output untouched on any failure; written becomes0. On success written excludes
 * NUL, output capacity must include NUL. Payload budget4096 excludes NUL.
 * LOGS_READ uses only the typed history snapshot and its internal encoder,
 * reserving the whole RPC envelope; no arbitrary JSON page is accepted.
 * Objects, buffers and written/error must not overlap; caller serializes access. */
ag_rpc_result_t ag_rpc_reply_encode(const ag_rpc_reply_t *, uint8_t *, size_t, size_t *);
#endif
