#include "ag_core.h"
#include <string.h>

static bool valid_session(const uint8_t *session_id) {
    uint8_t any = 0;
    if (session_id == NULL) return false;
    for (size_t i = 0; i < AG_SESSION_ID_BYTES; ++i) any |= session_id[i];
    return any != 0;
}

bool ag_log_queue_init(ag_log_queue_t *queue,
                       const uint8_t session_id[AG_SESSION_ID_BYTES],
                       const ag_pid_mask_t *configured) {
    if (queue == NULL) return false;
    memset(queue, 0, sizeof(*queue));
    if (!valid_session(session_id) || configured == NULL) return false;
    memcpy(queue->session_id, session_id, AG_SESSION_ID_BYTES);
    queue->configured = *configured;
    queue->next_sequence = 1;
    queue->enabled = true;
    return true;
}
void ag_log_queue_stop(ag_log_queue_t *queue) {
    if (queue != NULL) queue->enabled = false;
}
ag_log_result_t ag_log_queue_append(ag_log_queue_t *queue, uint8_t pid,
                                   uint32_t ecu_source, uint64_t monotonic_ms,
                                   ag_sample_status_t status,
                                   const uint8_t *raw, size_t raw_length) {
    if (queue == NULL || raw_length > AG_RAW_CAPACITY
        || (raw_length > 0 && raw == NULL)
        || status < AG_SAMPLE_VALID || status > AG_SAMPLE_ERROR
        || (status != AG_SAMPLE_VALID && raw_length != 0)
        || (status == AG_SAMPLE_VALID && raw_length == 0)) return AG_LOG_INVALID;
    if (!queue->enabled) return AG_LOG_DISABLED;
    if (!ag_pid_mask_has(&queue->configured, pid)) return AG_LOG_NOT_CONFIGURED;
    if (queue->next_sequence > 1 && monotonic_ms < queue->last_accepted_ms)
        return AG_LOG_CLOCK_REGRESSION;
    ag_log_series_t *series = NULL;
    ag_log_series_t *free_series = NULL;
    for (size_t i = 0; i < AG_LOG_MAX_SERIES; ++i) {
        if (!queue->series[i].used) { free_series = &queue->series[i]; continue; }
        if (queue->series[i].pid == pid && queue->series[i].ecu_source == ecu_source) {
            series = &queue->series[i];
            break;
        }
    }
    if (series != NULL) {
        if (monotonic_ms < series->last_ms) return AG_LOG_CLOCK_REGRESSION;
        if (monotonic_ms - series->last_ms < 1000) return AG_LOG_RATE_LIMITED;
    } else {
        if (free_series == NULL) return AG_LOG_SERIES_FULL;
        series = free_series;
    }
    if (queue->next_sequence == UINT64_MAX) return AG_LOG_SEQUENCE_EXHAUSTED;
    ag_log_result_t result = AG_LOG_ACCEPTED;
    if (queue->count == AG_LOG_CAPACITY) {
        if (queue->dropped_records == UINT64_MAX) return AG_LOG_SEQUENCE_EXHAUSTED;
        if (queue->dropped_records == 0)
            queue->first_dropped_sequence = queue->records[queue->head].sequence;
        queue->last_dropped_sequence = queue->records[queue->head].sequence;
        ++queue->dropped_records;
        queue->head = (queue->head + 1) % AG_LOG_CAPACITY;
        --queue->count;
        result = AG_LOG_OVERWROTE_OLDEST;
    }
    ag_log_record_t *record = &queue->records[(queue->head + queue->count) % AG_LOG_CAPACITY];
    memset(record, 0, sizeof(*record));
    memcpy(record->session_id, queue->session_id, AG_SESSION_ID_BYTES);
    record->sequence = queue->next_sequence++;
    record->monotonic_ms = monotonic_ms;
    record->pid = pid;
    record->ecu_source = ecu_source;
    record->status = status;
    record->raw_length = (uint8_t)raw_length;
    if (raw_length > 0) memcpy(record->raw, raw, raw_length);
    ++queue->count;
    queue->last_accepted_ms = monotonic_ms;
    series->pid = pid;
    series->ecu_source = ecu_source;
    series->last_ms = monotonic_ms;
    series->used = true;
    return result;
}
bool ag_log_queue_pop(ag_log_queue_t *queue, ag_log_record_t *record) {
    if (queue == NULL || record == NULL || queue->count == 0) return false;
    *record = queue->records[queue->head];
    queue->head = (queue->head + 1) % AG_LOG_CAPACITY;
    --queue->count;
    return true;
}
