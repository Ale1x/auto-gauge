#include "ag_history.h"
#include "history_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 2) { fprintf(stderr, "Usage: gauge_history_fixture_cli [case_name]\n"); return 2; }
    bool matched = false;
    for (size_t c = 0; c < kHistoryFixtureCount; ++c) {
        const ag_history_fixture_t *fixture = &kHistoryFixtures[c];
        if (argc == 2 && strcmp(argv[1], fixture->name) != 0) continue;
        matched = true;
        ag_history_source_t source;
        assert(ag_history_init(&source, fixture->session, fixture->capacity, fixture->first_sequence));
        for (size_t r = 0; r < fixture->record_count; ++r) {
            const ag_history_record_t *record = &fixture->records[r];
            assert(source.next_sequence == record->sequence);
            assert(ag_history_append(&source, record->pid, record->ecu_source, record->monotonic_ms,
                                     record->status, record->raw, record->raw_length) == AG_HISTORY_OK);
        }
        for (size_t a = 0; a < fixture->ack_count; ++a) {
            assert(ag_history_acknowledge(&source, fixture->session, fixture->acks[a].through) == AG_HISTORY_OK);
            assert(source.count == fixture->acks[a].record_count);
        }
        ag_history_snapshot_t snapshot;
        assert(ag_history_export(&source, &snapshot) == AG_HISTORY_OK);
        for (size_t r = 0; r < fixture->read_count; ++r) {
            char output[AG_HISTORY_PAGE_BYTES + 1];
            ag_history_page_info_t info;
            assert(ag_history_make_page(&snapshot, fixture->session, fixture->reads[r].cursor,
                                        fixture->reads[r].limit, output, sizeof(output), &info) == AG_HISTORY_OK);
            assert(strlen(output) == info.written && info.written <= AG_HISTORY_PAGE_BYTES);
            printf("{\"case\":\"%s\",\"read_index\":%zu,\"page\":%s}\n", fixture->name, r, output);
        }
    }
    if (!matched) { fprintf(stderr, "Unknown fixture case\n"); return 2; }
    return 0;
}
