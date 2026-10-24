#include "ag_history.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool valid_session(const uint8_t *session) {
    if (session == NULL) return false;
    uint8_t any = 0;
    for (size_t i = 0; i < AG_SESSION_ID_BYTES; ++i) any |= session[i];
    return any != 0;
}
static const char *status_name(ag_sample_status_t status) {
    switch (status) {
        case AG_SAMPLE_VALID: return "valid";
        case AG_SAMPLE_UNSUPPORTED: return "unsupported";
        case AG_SAMPLE_UNAVAILABLE: return "unavailable";
        case AG_SAMPLE_STALE: return "stale";
        case AG_SAMPLE_TIMEOUT: return "timeout";
        case AG_SAMPLE_ERROR: return "error";
        default: return NULL;
    }
}
static bool valid_record(const ag_history_record_t *record) {
    return record->sequence != 0 && record->pid != 0 && record->ecu_source <= 0x1fffffff
        && status_name(record->status) != NULL && record->raw_length <= AG_HISTORY_RAW_CAPACITY
        && ((record->status == AG_SAMPLE_VALID) == (record->raw_length > 0));
}
static bool valid_source(const ag_history_source_t *source) {
    return source != NULL && source->initialized && valid_session(source->session_id)
        && source->capacity >= 1 && source->capacity <= AG_HISTORY_CAPACITY
        && source->head < source->capacity && source->count <= source->capacity
        && source->next_sequence != 0;
}
bool ag_history_init(ag_history_source_t *source,
                      const uint8_t session_id[AG_SESSION_ID_BYTES],
                      size_t capacity, uint64_t first_sequence) {
    if (source == NULL) return false;
    uint8_t session_copy[AG_SESSION_ID_BYTES] = {0};
    if (session_id != NULL) memcpy(session_copy, session_id, sizeof(session_copy));
    memset(source, 0, sizeof(*source));
    if (!valid_session(session_copy) || capacity < 1 || capacity > AG_HISTORY_CAPACITY || first_sequence == 0)
        return false;
    memcpy(source->session_id, session_copy, sizeof(session_copy));
    source->capacity = capacity;
    source->next_sequence = first_sequence;
    source->initialized = true;
    return true;
}
ag_history_result_t ag_history_append(ag_history_source_t *source, uint8_t pid,
                                       uint32_t ecu_source, uint64_t monotonic_ms,
                                       ag_sample_status_t status,
                                       const uint8_t *raw, size_t raw_length) {
    if (!valid_source(source)) return AG_HISTORY_UNINITIALIZED;
    if (source->exhausted) return AG_HISTORY_SEQUENCE_EXHAUSTED;
    if (pid == 0 || ecu_source > 0x1fffffff || status_name(status) == NULL
        || raw_length > AG_HISTORY_RAW_CAPACITY || (raw_length > 0 && raw == NULL)
        || ((status == AG_SAMPLE_VALID) != (raw_length > 0))) return AG_HISTORY_INVALID;
    if (monotonic_ms < source->last_ms) return AG_HISTORY_CLOCK_REGRESSION;
    ag_history_record_t record;
    memset(&record, 0, sizeof(record));
    record.sequence = source->next_sequence;
    record.monotonic_ms = monotonic_ms;
    record.pid = pid;
    record.ecu_source = ecu_source;
    record.status = status;
    record.raw_length = (uint8_t)raw_length;
    if (raw_length > 0) memcpy(record.raw, raw, raw_length);
    if (source->count == source->capacity) {
        source->head = (source->head + 1) % source->capacity;
        --source->count;
    }
    source->records[(source->head + source->count) % source->capacity] = record;
    ++source->count;
    source->last_ms = monotonic_ms;
    if (source->next_sequence == UINT64_MAX) source->exhausted = true;
    else ++source->next_sequence;
    return AG_HISTORY_OK;
}
bool ag_history_snapshot_valid(const ag_history_snapshot_t *snapshot) {
    if (snapshot == NULL || !valid_session(snapshot->session_id)
        || snapshot->capacity < 1 || snapshot->capacity > AG_HISTORY_CAPACITY
        || snapshot->count > snapshot->capacity || snapshot->next_sequence == 0) return false;
    if (snapshot->count == 0) return !snapshot->exhausted && snapshot->last_ms == 0;
    for (size_t i = 0; i < snapshot->count; ++i) {
        const ag_history_record_t *record = &snapshot->records[i];
        if (!valid_record(record)) return false;
        if (i > 0 && (snapshot->records[i - 1].sequence == UINT64_MAX
                      || record->sequence != snapshot->records[i - 1].sequence + 1
                      || record->monotonic_ms < snapshot->records[i - 1].monotonic_ms)) return false;
    }
    const ag_history_record_t *last = &snapshot->records[snapshot->count - 1];
    return last->monotonic_ms == snapshot->last_ms
        && (snapshot->exhausted ? last->sequence == UINT64_MAX && snapshot->next_sequence == UINT64_MAX
            : last->sequence < UINT64_MAX && snapshot->next_sequence == last->sequence + 1);
}
ag_history_result_t ag_history_export(const ag_history_source_t *source,
                                       ag_history_snapshot_t *snapshot) {
    if (snapshot == NULL) return AG_HISTORY_INVALID;
    if (!valid_source(source)) { memset(snapshot, 0, sizeof(*snapshot)); return AG_HISTORY_UNINITIALIZED; }
    memset(snapshot, 0, sizeof(*snapshot));
    memcpy(snapshot->session_id, source->session_id, sizeof(snapshot->session_id));
    snapshot->next_sequence = source->next_sequence;
    snapshot->last_ms = source->last_ms;
    snapshot->capacity = source->capacity;
    snapshot->count = source->count;
    snapshot->exhausted = source->exhausted;
    for (size_t i = 0; i < source->count; ++i)
        snapshot->records[i] = source->records[(source->head + i) % source->capacity];
    return ag_history_snapshot_valid(snapshot) ? AG_HISTORY_OK : AG_HISTORY_SNAPSHOT_INVALID;
}
ag_history_result_t ag_history_restore(ag_history_source_t *source,
                                        const ag_history_snapshot_t *snapshot) {
    if (source == NULL) return AG_HISTORY_INVALID;
    if (!ag_history_snapshot_valid(snapshot)) {
        memset(source, 0, sizeof(*source));
        return AG_HISTORY_SNAPSHOT_INVALID;
    }
    memset(source, 0, sizeof(*source));
    memcpy(source->session_id, snapshot->session_id, sizeof(source->session_id));
    source->next_sequence = snapshot->next_sequence;
    source->last_ms = snapshot->last_ms;
    source->capacity = snapshot->capacity;
    source->count = snapshot->count;
    source->exhausted = snapshot->exhausted;
    for (size_t i = 0; i < snapshot->count; ++i) source->records[i] = snapshot->records[i];
    source->initialized = true;
    return AG_HISTORY_OK;
}
static void hex_encode(const uint8_t *data, size_t length, char *encoded) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        encoded[2 * i] = digits[data[i] >> 4];
        encoded[2 * i + 1] = digits[data[i] & 15];
    }
    encoded[2 * length] = '\0';
}
static size_t record_json(const ag_history_record_t *record, char *output, size_t capacity) {
    char raw[2 * AG_HISTORY_RAW_CAPACITY + 1];
    hex_encode(record->raw, record->raw_length, raw);
    const int written = snprintf(output, capacity,
        "{\"sequence\":\"%" PRIu64 "\",\"monotonic_ms\":\"%" PRIu64
        "\",\"pid\":%u,\"ecu_source\":%" PRIu32 ",\"status\":\"%s\",\"raw_hex\":\"%s\"}",
        record->sequence, record->monotonic_ms, (unsigned)record->pid,
        record->ecu_source, status_name(record->status), raw);
    return written < 0 || (size_t)written >= capacity ? 0 : (size_t)written;
}
ag_history_result_t ag_history_make_page(const ag_history_snapshot_t *snapshot,
                                         const uint8_t requested_session[AG_SESSION_ID_BYTES],
                                         uint64_t after_sequence, size_t limit,
                                         char *output, size_t output_capacity,
                                         ag_history_page_info_t *info) {
    if (output != NULL && output_capacity > 0) output[0] = '\0';
    if (info != NULL) memset(info, 0, sizeof(*info));
    if (output == NULL || info == NULL || limit < 1 || limit > AG_HISTORY_PAGE_RECORDS)
        return AG_HISTORY_INVALID;
    if (!ag_history_snapshot_valid(snapshot)) return AG_HISTORY_SNAPSHOT_INVALID;
    if (requested_session == NULL || memcmp(requested_session, snapshot->session_id, AG_SESSION_ID_BYTES) != 0)
        return AG_HISTORY_SESSION_MISMATCH;
    const uint64_t latest = snapshot->exhausted ? UINT64_MAX : snapshot->next_sequence - 1;
    if (after_sequence > latest) return AG_HISTORY_CURSOR_AHEAD;
    ag_history_page_info_t page = {.through_sequence = after_sequence};
    const uint64_t first = snapshot->count > 0 ? snapshot->records[0].sequence : snapshot->next_sequence;
    if (after_sequence < first - 1) {
        page.has_loss = true;
        page.lost_first = after_sequence + 1;
        page.lost_last = first - 1;
        page.through_sequence = page.lost_last;
    }
    char suffix[128];
    const int suffix_length = page.has_loss ? snprintf(suffix, sizeof(suffix),
        "],\"lost\":[{\"first\":\"%" PRIu64 "\",\"last\":\"%" PRIu64 "\"}]}", page.lost_first, page.lost_last)
        : snprintf(suffix, sizeof(suffix), "],\"lost\":[]}");
    if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix)) return AG_HISTORY_INVALID;
    char staging[AG_HISTORY_PAGE_BYTES + 1], session[2 * AG_SESSION_ID_BYTES + 1];
    hex_encode(snapshot->session_id, AG_SESSION_ID_BYTES, session);
    const int prefix_length = snprintf(staging, sizeof(staging), "{\"session_id\":\"%s\",\"records\":[", session);
    if (prefix_length < 0 || (size_t)prefix_length >= sizeof(staging)) return AG_HISTORY_INVALID;
    size_t position = (size_t)prefix_length;
    for (size_t i = 0; i < snapshot->count && page.record_count < limit; ++i) {
        const ag_history_record_t *record = &snapshot->records[i];
        if (record->sequence <= after_sequence) continue;
        char encoded[384];
        const size_t length = record_json(record, encoded, sizeof(encoded));
        if (length == 0) return AG_HISTORY_INVALID;
        const size_t comma = page.record_count > 0 ? 1 : 0;
        if (position + comma + length + (size_t)suffix_length > AG_HISTORY_PAGE_BYTES) break;
        if (comma) staging[position++] = ',';
        memcpy(staging + position, encoded, length);
        position += length;
        page.through_sequence = record->sequence;
        ++page.record_count;
    }
    memcpy(staging + position, suffix, (size_t)suffix_length + 1);
    position += (size_t)suffix_length;
    if (output_capacity <= position) return AG_HISTORY_OUTPUT_SHORT;
    memcpy(output, staging, position + 1);
    page.written = position;
    *info = page;
    return AG_HISTORY_OK;
}
ag_history_result_t ag_history_acknowledge(const ag_history_source_t *source,
                                           const uint8_t requested_session[AG_SESSION_ID_BYTES],
                                           uint64_t through_sequence) {
    if (!valid_source(source)) return AG_HISTORY_UNINITIALIZED;
    if (requested_session == NULL || memcmp(requested_session, source->session_id, AG_SESSION_ID_BYTES) != 0)
        return AG_HISTORY_SESSION_MISMATCH;
    const uint64_t latest = source->exhausted ? UINT64_MAX : source->next_sequence - 1;
    return through_sequence <= latest ? AG_HISTORY_OK : AG_HISTORY_CURSOR_AHEAD;
}
