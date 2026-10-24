#include "ag_history.h"
#include "history_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t session[AG_SESSION_ID_BYTES] = {0x05};
static const uint8_t session_b[AG_SESSION_ID_BYTES] = {0x06};

static void append_samples(ag_history_source_t *source, unsigned count) {
    const uint8_t raw[] = {0x1a, 0x10};
    for (unsigned i = 0; i < count; ++i)
        assert(ag_history_append(source, 12, 2024, source->last_ms + 1000,
                                 AG_SAMPLE_VALID, raw, sizeof(raw)) == AG_HISTORY_OK);
}
static void test_shared_golden_pages_and_ack_is_stateless(void) {
    size_t pages = 0;
    for (size_t c = 0; c < kHistoryFixtureCount; ++c) {
        const ag_history_fixture_t *fixture = &kHistoryFixtures[c];
        ag_history_source_t source;
        assert(ag_history_init(&source, fixture->session, fixture->capacity, fixture->first_sequence));
        for (size_t r = 0; r < fixture->record_count; ++r) {
            const ag_history_record_t *record = &fixture->records[r];
            assert(source.next_sequence == record->sequence);
            assert(ag_history_append(&source, record->pid, record->ecu_source, record->monotonic_ms,
                                     record->status, record->raw, record->raw_length) == AG_HISTORY_OK);
        }
        const ag_history_source_t before = source;
        for (size_t a = 0; a < fixture->ack_count; ++a) {
            assert(ag_history_acknowledge(&source, fixture->session, fixture->acks[a].through) == AG_HISTORY_OK);
            assert(source.count == fixture->acks[a].record_count);
            assert(memcmp(&source, &before, sizeof(source)) == 0);
        }
        ag_history_snapshot_t snapshot;
        assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
        for (size_t r = 0; r < fixture->read_count; ++r) {
            char output[AG_HISTORY_PAGE_BYTES + 1];
            ag_history_page_info_t info;
            assert(ag_history_make_page(&snapshot, fixture->session, fixture->reads[r].cursor,
                                        fixture->reads[r].limit, output, sizeof(output), &info) == AG_HISTORY_OK);
            assert(strcmp(output, fixture->reads[r].json) == 0);
            assert(info.written == strlen(output) && info.record_count == fixture->reads[r].record_count);
            assert(info.through_sequence == fixture->reads[r].through);
            ++pages;
        }
    }
    printf("PASS: %zu shared pagination fixtures, %zu exact JSON golden pages\n", kHistoryFixtureCount, pages);
}

static void test_snapshot_is_value_copy_and_restore_is_validated(void) {
    ag_history_source_t source, restored;
    assert(ag_history_init(&source, session, 4, 1));
    append_samples(&source, 6);
    ag_history_snapshot_t snapshot, original;
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    original = snapshot;
    snapshot.records[0].raw[0] ^= 0xff;
    snapshot.records[0].sequence = 99;
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    assert(memcmp(&snapshot, &original, sizeof(snapshot)) == 0);
    assert(ag_history_restore(&restored, &snapshot) == AG_HISTORY_OK);
    append_samples(&restored, 1);
    assert(restored.next_sequence == 8 && source.next_sequence == 7);
    for (unsigned mutation = 0; mutation < 6; ++mutation) {
        snapshot = original;
        switch (mutation) {
            case 0: snapshot.count = 65; break;
            case 1: snapshot.next_sequence = 0; break;
            case 2: snapshot.records[1].sequence = 99; break;
            case 3: snapshot.records[1].monotonic_ms = 0; break;
            case 4: snapshot.records[0].status = (ag_sample_status_t)99; break;
            case 5: snapshot.records[0].raw_length = 65; break;
        }
        assert(!ag_history_snapshot_valid(&snapshot));
        assert(ag_history_restore(&restored, &snapshot) == AG_HISTORY_SNAPSHOT_INVALID);
        assert(!restored.initialized);
    }
}

static void test_short_output_has_no_partial_page_or_cursor(void) {
    ag_history_source_t source;
    assert(ag_history_init(&source, session, 4, 1));
    append_samples(&source, 5);
    ag_history_snapshot_t snapshot;
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    char output[AG_HISTORY_PAGE_BYTES + 1];
    ag_history_page_info_t info;
    assert(ag_history_make_page(&snapshot, session, 0, 16, output, sizeof(output), &info) == AG_HISTORY_OK);
    const size_t required_without_nul = info.written;
    assert(info.has_loss && info.lost_first == 1 && info.lost_last == 1);
    memset(output, 'x', sizeof(output));
    memset(&info, 0xff, sizeof(info));
    assert(ag_history_make_page(&snapshot, session, 0, 16, output, required_without_nul, &info) == AG_HISTORY_OUTPUT_SHORT);
    assert(output[0] == '\0' && info.written == 0 && info.through_sequence == 0 && !info.has_loss);
    output[0] = 'x';
    assert(ag_history_make_page(&snapshot, session, 0, 16, output, 0, &info) == AG_HISTORY_OUTPUT_SHORT);
    assert(output[0] == 'x' && info.written == 0);
    assert(source.count == 4 && source.next_sequence == 6);
}

static void test_budget_raw_and_uint64_terminal(void) {
    ag_history_source_t source;
    assert(ag_history_init(&source, session, 64, UINT64_MAX - 19));
    uint8_t raw[AG_HISTORY_RAW_CAPACITY];
    for (unsigned i = 0; i < sizeof(raw); ++i) raw[i] = (uint8_t)i;
    for (unsigned i = 0; i < 20; ++i)
        assert(ag_history_append(&source, 255, 0x1fffffff, UINT64_MAX,
                                 AG_SAMPLE_VALID, raw, sizeof(raw)) == AG_HISTORY_OK);
    assert(source.exhausted && source.next_sequence == UINT64_MAX);
    assert(ag_history_append(&source, 255, 0x1fffffff, UINT64_MAX, AG_SAMPLE_VALID, raw, 64) == AG_HISTORY_SEQUENCE_EXHAUSTED);
    ag_history_snapshot_t snapshot;
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    char output[AG_HISTORY_PAGE_BYTES + 1];
    ag_history_page_info_t info;
    assert(ag_history_make_page(&snapshot, session, UINT64_MAX - 20, 16, output, sizeof(output), &info) == AG_HISTORY_OK);
    assert(info.record_count == 15 && info.written == 4046 && info.written <= AG_HISTORY_PAGE_BYTES);
    assert(info.through_sequence == UINT64_MAX - 5 && !info.has_loss);
    assert(ag_history_make_page(&snapshot, session, info.through_sequence, 16, output, sizeof(output), &info) == AG_HISTORY_OK);
    assert(info.record_count == 5 && info.through_sequence == UINT64_MAX);
    assert(ag_history_acknowledge(&source, session, UINT64_MAX) == AG_HISTORY_OK);
    assert(ag_history_make_page(&snapshot, session, UINT64_MAX, 16, output, sizeof(output), &info) == AG_HISTORY_OK);
    assert(info.record_count == 0 && info.through_sequence == UINT64_MAX);
}

static void test_invalid_inputs_timestamp_and_new_boot(void) {
    ag_history_source_t source;
    assert(!ag_history_init(&source, session, 65, 1));
    assert(!ag_history_init(&source, session, 4, 0));
    assert(ag_history_init(&source, session, 4, 1));
    const uint8_t raw[] = {0};
    assert(ag_history_append(&source, 12, 2024, 100, AG_SAMPLE_VALID, raw, 1) == AG_HISTORY_OK);
    const ag_history_source_t before = source;
    assert(ag_history_append(&source, 12, 2024, 99, AG_SAMPLE_VALID, raw, 1) == AG_HISTORY_CLOCK_REGRESSION);
    assert(ag_history_append(&source, 12, 2024, 100, AG_SAMPLE_VALID, raw, 65) == AG_HISTORY_INVALID);
    assert(ag_history_append(&source, 12, 2024, 100, AG_SAMPLE_TIMEOUT, raw, 1) == AG_HISTORY_INVALID);
    assert(ag_history_append(&source, 12, 2024, 100, AG_SAMPLE_VALID, NULL, 0) == AG_HISTORY_INVALID);
    assert(memcmp(&source, &before, sizeof(source)) == 0);
    ag_history_snapshot_t snapshot;
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    char output[AG_HISTORY_PAGE_BYTES + 1];
    ag_history_page_info_t info;
    assert(ag_history_make_page(&snapshot, session, 2, 16, output, sizeof(output), &info) == AG_HISTORY_CURSOR_AHEAD);
    assert(ag_history_make_page(&snapshot, session, 0, 17, output, sizeof(output), &info) == AG_HISTORY_INVALID);
    assert(ag_history_acknowledge(&source, session_b, 1) == AG_HISTORY_SESSION_MISMATCH);
    assert(ag_history_acknowledge(&source, session, 2) == AG_HISTORY_CURSOR_AHEAD);
    assert(ag_history_init(&source, session_b, 4, 1));
    assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
    assert(ag_history_make_page(&snapshot, session, 0, 16, output, sizeof(output), &info) == AG_HISTORY_SESSION_MISMATCH);
    assert(ag_history_make_page(&snapshot, session_b, 1, 16, output, sizeof(output), &info) == AG_HISTORY_CURSOR_AHEAD);
}

int main(void) {
    test_shared_golden_pages_and_ack_is_stateless();
    test_snapshot_is_value_copy_and_restore_is_validated();
    test_short_output_has_no_partial_page_or_cursor();
    test_budget_raw_and_uint64_terminal();
    test_invalid_inputs_timestamp_and_new_boot();
    printf("PASS: 5 history groups; source=%zu snapshot=%zu bytes; no consuming pop or per-client map\n",
           sizeof(ag_history_source_t), sizeof(ag_history_snapshot_t));
    return 0;
}
