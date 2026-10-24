#include "ag_journal_history.h"
#include "ag_flash_history_sim.h"
#include "history_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ag_journal_io_t journal_io(ag_flash_history_sim_t *sim) {
    ag_flash_history_io_t io = ag_flash_history_sim_io(sim);
    return (ag_journal_io_t){io.context, io.read, io.program, io.erase_sector};
}

static void test_shared_golden_pages(void) {
    unsigned pages = 0;
    for (size_t f = 0; f < kHistoryFixtureCount; ++f) {
        const ag_history_fixture_t *fixture = &kHistoryFixtures[f];
        uint8_t bytes[8192], filler_sid[16] = {0x80, 0};
        ag_flash_history_sim_t sim;
        ag_journal_history_t store;
        assert(ag_flash_history_sim_init(&sim, bytes, sizeof(bytes)));
        ag_journal_io_t io = journal_io(&sim);
        assert(ag_journal_open(&store, &io, 2) == AG_JOURNAL_EMPTY);
        const bool overwritten = fixture->record_count > fixture->capacity;
        unsigned filler = 0;
        assert(ag_journal_bootstrap(&store, overwritten ? filler_sid : fixture->session,
            fixture->first_sequence, 1) == AG_JOURNAL_OK);
        if (overwritten) {
            /* The shared four-record ring retains samples3..6. Journal eviction
             * is whole-sector: position its session-start at entry28, samples1/2
             * at29/30, then reclaim that sector using unrelated session controls.
             * This is fixture setup, not a four-record journal capacity claim. */
            assert(fixture->capacity == 4 && fixture->record_count == 6);
            for (unsigned n = 0; n < 27; ++n) {
                filler_sid[1] = (uint8_t)++filler;
                assert(ag_journal_begin_session(&store, filler_sid, 1) == AG_JOURNAL_OK);
            }
            assert(ag_journal_begin_session(&store, fixture->session,
                fixture->first_sequence) == AG_JOURNAL_OK);
        }
        for (size_t n = 0; n < fixture->record_count; ++n) {
            const ag_history_record_t *record = &fixture->records[n];
            assert(ag_journal_append(&store, record->pid, record->ecu_source,
                record->monotonic_ms, record->status, record->raw,
                record->raw_length) == AG_JOURNAL_OK);
        }
        if (overwritten) {
            while (store.info.next_serial <= 61) {
                filler_sid[1] = (uint8_t)++filler;
                assert(ag_journal_begin_session(&store, filler_sid, 1) == AG_JOURNAL_OK);
            }
        }
        for (size_t n = 0; n < fixture->read_count; ++n) {
            const ag_history_read_fixture_t *read = &fixture->reads[n];
            char actual[4097];
            ag_history_page_info_t info;
            assert(ag_journal_make_page(&store, fixture->session, read->cursor,
                read->limit, actual, sizeof(actual), &info) == AG_JOURNAL_OK);
            assert(strcmp(actual, read->json) == 0);
            assert(info.record_count == read->record_count && info.through_sequence == read->through);
            ++pages;
        }
        assert(!sim.invalid_access);
    }
    assert(kHistoryFixtureCount == 6 && pages == 12);
    puts("PASS: 6 journal shared golden cases / 12 byte-exact pages");
}

/* Independent bounded operation history, not copied from journal metadata. */
typedef struct {
    uint64_t serial;
    uint8_t sid[16];
    ag_history_record_t record;
} model_record_t;
static model_record_t model[600];
static size_t model_count;
static uint8_t model_media[4 * 4096];

static void compare_model_page(ag_journal_history_t *store, const uint8_t sid[16],
    uint64_t cursor, uint64_t first_serial, bool provably_empty) {
    ag_history_snapshot_t expected = {.capacity = 16};
    memcpy(expected.session_id, sid, 16);
    bool retained = false;
    ag_history_record_t last = {0};
    for (size_t i = 0; i < model_count; ++i) {
        if (model[i].serial < first_serial || memcmp(model[i].sid, sid, 16) != 0) continue;
        retained = true;
        last = model[i].record;
        if (last.sequence > cursor && expected.count < 16)
            expected.records[expected.count++] = last;
    }
    char actual[4097], wanted[4097];
    ag_history_page_info_t actual_info, wanted_info;
    ag_journal_result_t result = ag_journal_make_page(store, sid, cursor, 16,
        actual, sizeof(actual), &actual_info);
    if (!retained && !provably_empty) {
        assert(result == AG_JOURNAL_NOT_RETAINED && actual[0] == 0);
        return;
    }
    if (cursor > (retained ? last.sequence : 0)) {
        assert(result == AG_JOURNAL_CURSOR_AHEAD && actual[0] == 0);
        return;
    }
    assert(result == AG_JOURNAL_OK);
    if (!retained) expected.next_sequence = 1;
    else {
        if (expected.count == 0) expected.records[expected.count++] = last;
        const ag_history_record_t *tail = &expected.records[expected.count - 1];
        expected.last_ms = tail->monotonic_ms;
        expected.next_sequence = tail->sequence + 1;
    }
    assert(ag_history_make_page(&expected, sid, cursor, 16, wanted, sizeof(wanted),
        &wanted_info) == AG_HISTORY_OK);
    assert(strcmp(actual, wanted) == 0);
    assert(actual_info.record_count == wanted_info.record_count
        && actual_info.through_sequence == wanted_info.through_sequence
        && actual_info.has_loss == wanted_info.has_loss);
}

static void test_independent_operation_model(void) {
    ag_journal_history_t store;
    ag_flash_history_sim_t sim;
    assert(ag_flash_history_sim_init(&sim, model_media, sizeof(model_media)));
    ag_journal_io_t io = journal_io(&sim);
    assert(ag_journal_open(&store, &io, 4) == AG_JOURNAL_EMPTY);
    uint8_t sid[16] = {1};
    assert(ag_journal_bootstrap(&store, sid, 1, 1) == AG_JOURNAL_OK);
    uint64_t serial = 1, sequence = 1, ms = 0;
    unsigned session = 1, sample_counts[100] = {0};
    uint64_t session_starts[100] = {0};
    model_count = 0;
    for (unsigned operation = 1; operation <= 480; ++operation) {
        if (operation % 19 == 0 || operation % 23 == 0) {
            ++session;
            assert(session < 100);
            session_starts[session] = serial;
            sid[0] = (uint8_t)session;
            assert(ag_journal_begin_session(&store, sid, 1) == AG_JOURNAL_OK);
            ++serial;
            sequence = 1;
            ms = 0;
        } else {
            uint8_t raw[64];
            memset(raw, (int)(operation & 255), sizeof(raw));
            size_t length = operation % 3 ? 1 : 64;
            ag_sample_status_t status = operation % 7 ? AG_SAMPLE_VALID : AG_SAMPLE_TIMEOUT;
            if (status != AG_SAMPLE_VALID) length = 0;
            assert(ag_journal_append(&store, 12, 0x7e8, ms, status, raw, length) == AG_JOURNAL_OK);
            assert(model_count < sizeof(model) / sizeof(model[0]));
            model_record_t *sample = &model[model_count++];
            memset(sample, 0, sizeof(*sample));
            ++sample_counts[session];
            sample->serial = serial++;
            memcpy(sample->sid, sid, 16);
            sample->record = (ag_history_record_t){.sequence = sequence++, .monotonic_ms = ms++,
                .ecu_source = 0x7e8, .pid = 12, .raw_length = (uint8_t)length, .status = status};
            memcpy(sample->record.raw, raw, length);
        }
        /* Count-based FIFO oracle: controls and samples each consume one entry.
         * No store field is used to derive the expected retained boundary. */
        const uint64_t generation = (serial - 2) / 30 + 1;
        const uint64_t first = generation > 4 ? (generation - 4) * 30 + 1 : 1;
        assert(store.info.next_serial == serial && store.info.retained_first_serial == first);
        for (unsigned back = 0; back < 3 && back < session; ++back) {
            uint8_t wanted_sid[16] = {(uint8_t)(session - back)};
            bool empty = sample_counts[session - back] == 0
                && (back == 0 || session_starts[session - back] >= first);
            compare_model_page(&store, wanted_sid, 0, first, empty);
            compare_model_page(&store, wanted_sid, 2, first, empty);
            compare_model_page(&store, wanted_sid, UINT64_MAX, first, empty);
        }
        if (operation % 13 == 0) {
            ag_flash_history_sim_reboot(&sim);
            ag_journal_history_t reopened;
            assert(ag_journal_open(&reopened, &io, 4) == AG_JOURNAL_OK);
            assert(!reopened.writer_attached);
            compare_model_page(&reopened, sid, 0, first, sequence == 1);
        }
    }
    assert(!sim.invalid_access);
    puts("PASS: independent 480-operation / 4-sector journal model, controls and reopen queries");
}

int main(void) {
    test_shared_golden_pages();
    test_independent_operation_model();
    return 0;
}
