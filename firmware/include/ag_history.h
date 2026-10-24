#ifndef AG_HISTORY_H
#define AG_HISTORY_H
#include "ag_core.h"

#define AG_HISTORY_CAPACITY 64u
#define AG_HISTORY_RAW_CAPACITY 64u
#define AG_HISTORY_PAGE_RECORDS 16u
#define AG_HISTORY_PAGE_BYTES 4096u

/* History records are distinct from the consuming RAM logger queue. */
typedef struct {
    uint64_t sequence, monotonic_ms;
    uint32_t ecu_source;
    uint8_t pid, raw_length;
    ag_sample_status_t status;
    uint8_t raw[AG_HISTORY_RAW_CAPACITY];
} ag_history_record_t;
typedef struct {
    uint8_t session_id[AG_SESSION_ID_BYTES];
    ag_history_record_t records[AG_HISTORY_CAPACITY];
    uint64_t next_sequence, last_ms;
    size_t capacity, head, count;
    bool exhausted, initialized;
} ag_history_source_t;
typedef struct {
    uint8_t session_id[AG_SESSION_ID_BYTES];
    ag_history_record_t records[AG_HISTORY_CAPACITY]; /* Chronological value copy. */
    uint64_t next_sequence, last_ms;
    size_t capacity, count;
    bool exhausted;
} ag_history_snapshot_t;
typedef struct {
    size_t written, record_count;
    uint64_t through_sequence, lost_first, lost_last;
    bool has_loss;
} ag_history_page_info_t;
typedef enum { AG_HISTORY_OK = 0, AG_HISTORY_INVALID, AG_HISTORY_UNINITIALIZED,
               AG_HISTORY_SEQUENCE_EXHAUSTED, AG_HISTORY_CLOCK_REGRESSION,
               AG_HISTORY_SESSION_MISMATCH, AG_HISTORY_CURSOR_AHEAD,
               AG_HISTORY_OUTPUT_SHORT, AG_HISTORY_SNAPSHOT_INVALID } ag_history_result_t;

/* first_sequence is verified recovery/test metadata, NEVER a remote command. */
bool ag_history_init(ag_history_source_t *source,
                      const uint8_t session_id[AG_SESSION_ID_BYTES],
                      size_t capacity, uint64_t first_sequence);
ag_history_result_t ag_history_append(ag_history_source_t *source, uint8_t pid,
                                       uint32_t ecu_source, uint64_t monotonic_ms,
                                       ag_sample_status_t status,
                                       const uint8_t *raw, size_t raw_length);
bool ag_history_snapshot_valid(const ag_history_snapshot_t *snapshot);
ag_history_result_t ag_history_export(const ag_history_source_t *source,
                                       ag_history_snapshot_t *snapshot);
/* An invalid restore disables the destination, never serves its old session. */
ag_history_result_t ag_history_restore(ag_history_source_t *source,
                                        const ag_history_snapshot_t *snapshot);
/* Output is all-or-nothing, written excludes NUL; provision 4097 bytes.
 * through_sequence describes page coverage, NOT an import/erase ACK. */
ag_history_result_t ag_history_make_page(const ag_history_snapshot_t *snapshot,
                                         const uint8_t requested_session[AG_SESSION_ID_BYTES],
                                         uint64_t after_sequence, size_t limit,
                                         char *output, size_t output_capacity,
                                         ag_history_page_info_t *info);
/* Stateless progress signal only: no consumption, deletion or client map. */
ag_history_result_t ag_history_acknowledge(const ag_history_source_t *source,
                                           const uint8_t requested_session[AG_SESSION_ID_BYTES],
                                           uint64_t through_sequence);
#endif
