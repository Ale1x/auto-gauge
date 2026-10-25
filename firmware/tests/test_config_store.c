#define AG_CONFIG_STORE_FIXTURE
#include "ag_config_store.h"
#include "ag_flash_sim.h"
#include "shared_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ag_pid_mask_t capabilities(void) {
    ag_pid_mask_t supported;
    for (size_t i = 0; i < AG_PID_MASK_WORDS; ++i) supported.words[i] = UINT32_MAX;
    return supported;
}
static ag_config_t revision(int64_t number) {
    ag_config_t config = kSharedFixture;
    config.revision = number;
    config.manual_percent = 40 + (int32_t)(number % 50);
    return config;
}
/* Explicit host fixture seed: no provisioning or real flash operation. */
static void seed(ag_flash_sim_t *flash, unsigned slot, const ag_config_t *config, uint64_t generation) {
    const ag_pid_mask_t supported = capabilities();
    uint8_t encoded[AG_CONFIG_RECORD_MAX]; size_t length;
    assert(slot < 2);
    assert(ag_config_record_encode(config, &supported, generation, encoded, sizeof(encoded), &length) == AG_CONFIG_STORE_OK);
    memset(flash->bytes + slot * AG_CONFIG_SLOT_BYTES, 0xff, AG_CONFIG_SLOT_BYTES);
    memcpy(flash->bytes + slot * AG_CONFIG_SLOT_BYTES, encoded, length);
    memcpy(flash->bytes + slot * AG_CONFIG_SLOT_BYTES + AG_CONFIG_COMMIT_OFFSET, ag_config_commit_marker, 4);
}
static void open_store(ag_config_store_t *store, ag_flash_sim_t *flash) {
    const ag_config_flash_io_t io = ag_flash_sim_io(flash);
    const ag_pid_mask_t supported = capabilities();
    assert(ag_config_store_open(store, &io, &supported) == AG_CONFIG_STORE_OK);
}

static void test_canonical_golden_and_shared_config_cases(void) {
    const ag_pid_mask_t supported = capabilities();
    uint8_t encoded[AG_CONFIG_RECORD_MAX]; size_t length;
    assert(ag_config_record_encode(&kSharedFixture, &supported, 1, encoded, sizeof(encoded), &length) == AG_CONFIG_STORE_OK);
    assert(length == sizeof(kConfigRecordGolden) && memcmp(encoded, kConfigRecordGolden, length) == 0);
    for (size_t i = 0; i < sizeof(kSharedCases) / sizeof(kSharedCases[0]); ++i) {
        const ag_config_store_result_t result = ag_config_record_encode(&kSharedCases[i].configuration,
            &supported, 1, encoded, sizeof(encoded), &length);
        assert((result == AG_CONFIG_STORE_OK) == kSharedCases[i].valid);
        if (!kSharedCases[i].valid) { assert(length == 0); continue; }
        ag_flash_sim_t flash;
        ag_flash_sim_init(&flash);
        seed(&flash, 0, &kSharedCases[i].configuration, 1);
        ag_config_store_t store;
        open_store(&store, &flash);
        ag_config_t restored;
        assert(ag_config_store_get(&store, &restored));
        uint8_t again[AG_CONFIG_RECORD_MAX]; size_t again_length;
        assert(ag_config_record_encode(&restored, &supported, 1, again, sizeof(again), &again_length) == AG_CONFIG_STORE_OK);
        assert(again_length == length && memcmp(encoded, again, length) == 0);
    }
    memset(encoded, 0xaa, sizeof(encoded));
    assert(ag_config_record_encode(&kSharedFixture, &supported, 1, encoded, 1, &length) == AG_CONFIG_STORE_OUTPUT_SHORT);
    assert(length == 0 && encoded[0] == 0xaa);
}

static void test_commit_alternates_inactive_and_rejects_stale(void) {
    ag_flash_sim_t flash;
    ag_flash_sim_init(&flash);
    const ag_config_t first = revision(1), second = revision(2), third = revision(3);
    seed(&flash, 0, &first, 1);
    ag_config_store_t store;
    open_store(&store, &flash);
    uint8_t old_slot[AG_CONFIG_SLOT_BYTES];
    memcpy(old_slot, flash.bytes, sizeof(old_slot));
    ag_flash_sim_reboot(&flash);
    assert(ag_config_store_commit(&store, 0, &second) == AG_CONFIG_STORE_EXPECTED_REVISION);
    assert(flash.erases == 0 && flash.programs == 0);
    assert(ag_config_store_commit(&store, 1, &third) == AG_CONFIG_STORE_CONFIG_INVALID);
    assert(flash.erases == 0 && flash.programs == 0);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_OK);
    assert(store.generation == 2 && store.active_slot == 1 && store.active.revision == 2);
    assert(memcmp(old_slot, flash.bytes, sizeof(old_slot)) == 0);
    assert(flash.last_program_offset == AG_CONFIG_SLOT_BYTES + AG_CONFIG_COMMIT_OFFSET && flash.last_program_length == 4);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_EXPECTED_REVISION);
    assert(ag_config_store_commit(&store, 2, &third) == AG_CONFIG_STORE_OK);
    assert(store.active_slot == 0 && store.generation == 3);
}

static void test_every_io_byte_boundary_keeps_a_complete_configuration(void) {
    ag_flash_sim_t base;
    ag_flash_sim_init(&base);
    const ag_config_t older = revision(2), active = revision(3), candidate = revision(4);
    seed(&base, 0, &active, 3);
    seed(&base, 1, &older, 2);
    ag_config_store_t baseline_store;
    open_store(&baseline_store, &base);
    ag_flash_sim_reboot(&base);
    assert(ag_config_store_commit(&baseline_store, 3, &candidate) == AG_CONFIG_STORE_OK);
    const size_t calls = base.operations;
    assert(calls > 0 && calls <= 16);
    size_t lengths[16];
    memcpy(lengths, base.trace_lengths, sizeof(lengths));
    size_t trials = 0;
    for (size_t call = 1; call <= calls; ++call) {
        for (size_t byte = 0; byte <= lengths[call - 1]; ++byte) {
            ag_flash_sim_t flash;
            ag_flash_sim_init(&flash);
            seed(&flash, 0, &active, 3);
            seed(&flash, 1, &older, 2);
            ag_config_store_t store;
            open_store(&store, &flash);
            ag_flash_sim_fail_at(&flash, call, byte);
            const ag_config_store_result_t result = ag_config_store_commit(&store, 3, &candidate);
            assert(memcmp(flash.bytes, base.bytes, AG_CONFIG_SLOT_BYTES) == 0); /* Active sector never written. */
            assert(!flash.invalid_access);
            ag_flash_sim_reboot(&flash);
            ag_config_store_t rebooted;
            const ag_config_flash_io_t io = ag_flash_sim_io(&flash);
            const ag_pid_mask_t supported = capabilities();
            const ag_config_store_result_t recovered = ag_config_store_open(&rebooted, &io, &supported);
            assert(recovered == AG_CONFIG_STORE_OK || recovered == AG_CONFIG_STORE_READ_ONLY);
            ag_config_t restored;
            assert(ag_config_store_get(&rebooted, &restored));
            assert(restored.revision == 3 || restored.revision == 4);
            assert(restored.manual_percent == revision(restored.revision).manual_percent);
            if (result == AG_CONFIG_STORE_OK) assert(restored.revision == 4);
            if (recovered == AG_CONFIG_STORE_READ_ONLY) {
                const size_t writes = flash.programs, erases = flash.erases;
                const ag_config_t repair = revision(restored.revision + 1);
                assert(ag_config_store_commit(&rebooted, (uint32_t)restored.revision, &repair) == AG_CONFIG_STORE_READ_ONLY);
                assert(flash.programs == writes && flash.erases == erases);
            }
            ++trials;
        }
    }
    printf("PASS: %zu exhaustive power-loss IO byte boundaries across %zu phases\n", trials, calls);
}

static void test_every_slot_byte_corruption_falls_back_and_both_bad_have_no_default(void) {
    ag_flash_sim_t base;
    ag_flash_sim_init(&base);
    const ag_config_t first = revision(1), second = revision(2);
    seed(&base, 0, &first, 1);
    seed(&base, 1, &second, 2);
    for (size_t byte = 0; byte < AG_CONFIG_SLOT_BYTES; ++byte) {
        ag_flash_sim_t flash = base;
        flash.bytes[AG_CONFIG_SLOT_BYTES + byte] ^= 1;
        ag_config_store_t store;
        const ag_config_flash_io_t io = ag_flash_sim_io(&flash);
        const ag_pid_mask_t supported = capabilities();
        assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_READ_ONLY);
        assert(store.active.revision == 1 && store.active_slot == 0);
        assert(!store.writable && ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_READ_ONLY);
        assert(flash.programs == 0 && flash.erases == 0);
    }
    base.bytes[0] ^= 1;
    base.bytes[AG_CONFIG_SLOT_BYTES] ^= 1;
    ag_config_store_t store;
    const ag_config_flash_io_t io = ag_flash_sim_io(&base);
    const ag_pid_mask_t supported = capabilities();
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_NO_VALID_CONFIG);
    ag_config_t unchanged = revision(99);
    assert(!ag_config_store_get(&store, &unchanged) && unchanged.revision == 99);
    assert(ag_config_store_commit(&store, 0, &first) == AG_CONFIG_STORE_NO_VALID_CONFIG);
    assert(base.erases == 0 && base.programs == 0);
    ag_flash_sim_init(&base);
    assert(ag_config_store_restore(&store) == AG_CONFIG_STORE_NO_VALID_CONFIG);
}

static bool fake_successful_erase(void *context, uint32_t offset, size_t length) {
    (void)context; (void)offset; (void)length;
    return true; /* Deliberately lying adapter: readback must still stop commit. */
}
static void test_committed_latest_corruption_remains_diagnostic(void) {
    ag_flash_sim_t flash;
    ag_flash_sim_init(&flash);
    const ag_config_t first = revision(1), second = revision(2), third = revision(3);
    seed(&flash, 0, &first, 1);
    ag_config_store_t store;
    open_store(&store, &flash);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_OK);
    /* Complete committed update, then lost application ACK / corruption of newest marker. */
    flash.bytes[store.active_slot * AG_CONFIG_SLOT_BYTES + AG_CONFIG_COMMIT_OFFSET] ^= 1;
    ag_flash_sim_reboot(&flash);
    const ag_config_flash_io_t io = ag_flash_sim_io(&flash);
    const ag_pid_mask_t supported = capabilities();
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_READ_ONLY);
    ag_config_t diagnostic;
    assert(ag_config_store_get(&store, &diagnostic) && diagnostic.revision == 1 && !store.writable);
    assert(ag_config_store_commit(&store, 1, &third) == AG_CONFIG_STORE_READ_ONLY);
    assert(flash.programs == 0 && flash.erases == 0);
}
static void test_short_reads_and_readback_fail_closed(void) {
    ag_flash_sim_t flash;
    ag_flash_sim_init(&flash);
    const ag_config_t first = revision(1), second = revision(2);
    seed(&flash, 0, &first, 1);
    ag_config_store_t store;
    open_store(&store, &flash);
    ag_flash_sim_fail_at(&flash, 2, 10);
    assert(ag_config_store_restore(&store) == AG_CONFIG_STORE_READ_ONLY);
    assert(store.ready && !store.writable && store.active.revision == 1);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_IO_ERROR);
    assert(flash.programs == 0 && flash.erases == 0);
    ag_flash_sim_reboot(&flash);
    open_store(&store, &flash);
    flash.corrupt_program_call = 1;
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_IO_ERROR);
    assert(flash.programs == 1 && flash.bytes[AG_CONFIG_SLOT_BYTES + AG_CONFIG_COMMIT_OFFSET] == 0xff);
    ag_flash_sim_reboot(&flash);
    seed(&flash, 1, &first, 1);
    ag_config_flash_io_t io = ag_flash_sim_io(&flash);
    io.erase = fake_successful_erase;
    const ag_pid_mask_t supported = capabilities();
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_OK);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_IO_ERROR);
    assert(flash.programs == 0);
}

static void test_conflict_revision_generation_and_capabilities(void) {
    ag_flash_sim_t flash;
    ag_flash_sim_init(&flash);
    ag_config_t first = revision(1), second = revision(2);
    seed(&flash, 0, &first, 1);
    seed(&flash, 1, &second, 1);
    ag_config_store_t store;
    ag_config_flash_io_t io = ag_flash_sim_io(&flash);
    ag_pid_mask_t supported = capabilities();
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_CONFLICT);
    assert(!store.ready);
    seed(&flash, 1, &first, 1);
    open_store(&store, &flash); /* Identical duplicates are unambiguous. */
    seed(&flash, 0, &first, 2); seed(&flash, 1, &second, 1);
    assert(ag_config_store_restore(&store) == AG_CONFIG_STORE_CONFLICT);
    ag_flash_sim_init(&flash);
    first = revision(INT32_MAX);
    seed(&flash, 0, &first, 1);
    open_store(&store, &flash);
    second = revision((int64_t)INT32_MAX + 1);
    assert(ag_config_store_commit(&store, INT32_MAX, &second) == AG_CONFIG_STORE_REVISION_EXHAUSTED);
    assert(flash.programs == 0 && flash.erases == 0);
    first = revision(1); second = revision(2);
    seed(&flash, 0, &first, UINT64_MAX);
    open_store(&store, &flash);
    assert(ag_config_store_commit(&store, 1, &second) == AG_CONFIG_STORE_GENERATION_EXHAUSTED);
    assert(flash.programs == 0 && flash.erases == 0);
    supported.words[0] = 0;
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_CAPABILITY_MISMATCH);
    assert(store.ready && store.writable && !store.compatible && store.active.revision == 1);
    /* A newer intact but incompatible config must not roll back to the old one. */
    ag_flash_sim_init(&flash);
    first = revision(1); second = revision(2);
    second.dashboards[0].widgets[0].pid = 255;
    seed(&flash, 0, &first, 1);
    ag_pid_mask_t all = capabilities();
    ag_pid_mask_set(&all, 255);
    uint8_t record[AG_CONFIG_RECORD_MAX]; size_t length = 0;
    assert(ag_config_record_encode(&second, &all, 2, record, sizeof(record), &length) == AG_CONFIG_STORE_OK);
    memcpy(flash.bytes + AG_CONFIG_SLOT_BYTES, record, length);
    memcpy(flash.bytes + AG_CONFIG_SLOT_BYTES + AG_CONFIG_COMMIT_OFFSET, ag_config_commit_marker, 4);
    supported = capabilities();
    supported.words[255u / 32u] &= ~(UINT32_C(1) << (255u % 32u));
    assert(ag_config_store_open(&store, &io, &supported) == AG_CONFIG_STORE_CAPABILITY_MISMATCH);
    ag_config_t retained;
    assert(ag_config_store_get(&store, &retained) && retained.revision == 2 && store.generation == 2);
    assert(store.active_slot == 1 && !store.compatible && flash.erases == 0 && flash.programs == 0);
    ag_config_t repaired = revision(3);
    assert(ag_config_store_commit(&store, 1, &repaired) == AG_CONFIG_STORE_EXPECTED_REVISION);
    assert(ag_config_store_commit(&store, 2, &repaired) == AG_CONFIG_STORE_OK);
    assert(store.compatible && store.active.revision == 3);
}

static void test_simulated_adapter_bounds_and_program_rules(void) {
    ag_flash_sim_t flash;
    ag_flash_sim_init(&flash);
    ag_config_flash_io_t io = ag_flash_sim_io(&flash);
    uint8_t byte = 0;
    assert(io.read(io.context, AG_CONFIG_STORE_BYTES, &byte, 1) == 0 && flash.invalid_access);
    ag_flash_sim_reboot(&flash);
    assert(!io.erase(io.context, 1, AG_CONFIG_SLOT_BYTES) && flash.invalid_access);
    ag_flash_sim_reboot(&flash);
    assert(io.program(io.context, 0, &byte, 1) == 1);
    byte = 0xff;
    assert(io.program(io.context, 0, &byte, 1) == 0 && flash.invalid_access);
    assert(flash.bytes[0] == 0);
}

static void test_explicit_blank_provisioning_and_fault_recovery(void) {
    ag_flash_sim_t baseline;
    ag_flash_sim_init(&baseline);
    ag_config_store_t store;
    ag_pid_mask_t mask=capabilities();
    ag_config_flash_io_t io=ag_flash_sim_io(&baseline);
    const ag_config_t first=revision(1);
    assert(ag_config_store_open(&store,&io,&mask)==AG_CONFIG_STORE_NO_VALID_CONFIG);
    ag_flash_sim_reboot(&baseline);
    assert(ag_config_store_provision_blank(&store,&first)==AG_CONFIG_STORE_OK);
    assert(store.ready && store.active.revision==1 && store.generation==1);
    assert(baseline.erases==0 && baseline.programs==2);
    const size_t calls=baseline.operations;
    size_t lengths[16]; ag_flash_sim_operation_t kinds[16];
    memcpy(lengths,baseline.trace_lengths,sizeof(lengths));
    memcpy(kinds,baseline.trace_operations,sizeof(kinds));
    assert(calls<=16);
    for(size_t call=1;call<=calls;++call) {
        for(size_t cut=0;cut<=lengths[call-1];++cut) {
            if(kinds[call-1]==AG_FLASH_SIM_READ && cut!=0 && cut+1!=lengths[call-1]
                && cut!=lengths[call-1]) continue;
            ag_flash_sim_t flash; ag_flash_sim_init(&flash); io=ag_flash_sim_io(&flash);
            assert(ag_config_store_open(&store,&io,&mask)==AG_CONFIG_STORE_NO_VALID_CONFIG);
            ag_flash_sim_fail_at(&flash,call,cut);
            const ag_config_store_result_t result=ag_config_store_provision_blank(&store,&first);
            const size_t writes=flash.programs;
            assert(ag_config_store_provision_blank(&store,&first)==AG_CONFIG_STORE_INVALID);
            assert(flash.programs==writes && flash.erases==0 && !flash.invalid_access);
            ag_flash_sim_reboot(&flash);
            const ag_config_store_result_t restored=ag_config_store_open(&store,&io,&mask);
            assert(restored==AG_CONFIG_STORE_NO_VALID_CONFIG || restored==AG_CONFIG_STORE_OK);
            if(result==AG_CONFIG_STORE_OK) assert(restored==AG_CONFIG_STORE_OK);
            if(restored==AG_CONFIG_STORE_OK) assert(store.active.revision==1);
            else {
                bool blank=true;
                for(size_t i=0;i<sizeof(flash.bytes);++i) blank=blank && flash.bytes[i]==0xff;
                assert(ag_config_store_provision_blank(&store,&first)==
                    (blank?AG_CONFIG_STORE_OK:AG_CONFIG_STORE_CONFLICT));
            }
        }
    }
    /* No valid record does not imply blank: reserved/inactive dirt is kept. */
    for(unsigned slot=0;slot<2;++slot) {
        ag_flash_sim_t flash; ag_flash_sim_init(&flash);
        flash.bytes[slot*AG_CONFIG_SLOT_BYTES+3000]=0;
        io=ag_flash_sim_io(&flash);
        assert(ag_config_store_open(&store,&io,&mask)==AG_CONFIG_STORE_NO_VALID_CONFIG);
        assert(ag_config_store_provision_blank(&store,&first)==AG_CONFIG_STORE_CONFLICT);
        assert(flash.programs==0 && flash.erases==0);
    }
}

int main(void) {
    test_canonical_golden_and_shared_config_cases();
    test_commit_alternates_inactive_and_rejects_stale();
    test_every_io_byte_boundary_keeps_a_complete_configuration();
    test_every_slot_byte_corruption_falls_back_and_both_bad_have_no_default();
    test_short_reads_and_readback_fail_closed();
    test_conflict_revision_generation_and_capabilities();
    test_simulated_adapter_bounds_and_program_rules();
    test_committed_latest_corruption_remains_diagnostic();
    test_explicit_blank_provisioning_and_fault_recovery();
    printf("PASS: 9 config persistence groups; store=%zu sim=%zu bytes host; zero heap, no hardware IO\n",
           sizeof(ag_config_store_t), sizeof(ag_flash_sim_t));
    return 0;
}
