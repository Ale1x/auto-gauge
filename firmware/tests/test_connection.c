#include "ag_connection.h"
#include "ag_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t boot_a[AG_SESSION_ID_BYTES] = {0xa1};
static const uint8_t boot_b[AG_SESSION_ID_BYTES] = {0xb2};
static const ag_client_id_t phone_a = {{0x11}};
static const ag_client_id_t phone_b = {{0x22}};
static const uint8_t proof[] = {0x5a}; /* Opaque fixture, not an auth protocol. */

typedef struct {
    ag_reset_snapshot_t durable;
    bool credentials_present, configuration_present, logs_present;
    unsigned operations, writes, erases, fail_before, fail_after;
    ag_client_id_t externally_verified_client;
    uint64_t externally_verified_counter;
    uint8_t externally_verified_boot[AG_SESSION_ID_BYTES];
} memory_io_t;

static void memory_init(memory_io_t *memory) {
    memset(memory, 0, sizeof(*memory));
    memory->durable = ag_reset_clean_snapshot();
    memory->credentials_present = memory->configuration_present = memory->logs_present = true;
}
static bool operation_begin(memory_io_t *memory) {
    ++memory->operations;
    return memory->operations != memory->fail_before;
}
static bool operation_end(memory_io_t *memory) {
    return memory->operations != memory->fail_after;
}
static bool write_snapshot(void *context, const ag_reset_snapshot_t *snapshot) {
    memory_io_t *memory = context;
    ++memory->writes;
    assert(ag_reset_snapshot_valid(snapshot));
    if (!operation_begin(memory)) return false;
    memory->durable = *snapshot;
    return operation_end(memory);
}
static bool erase_credentials(void *context) {
    memory_io_t *memory = context;
    ++memory->erases;
    if (!operation_begin(memory)) return false;
    memory->credentials_present = false;
    memory->externally_verified_counter = 0;
    memset(&memory->externally_verified_client, 0, sizeof(memory->externally_verified_client));
    memset(memory->externally_verified_boot, 0, sizeof(memory->externally_verified_boot));
    return operation_end(memory);
}
static bool erase_configuration(void *context) {
    memory_io_t *memory = context;
    ++memory->erases;
    if (!operation_begin(memory)) return false;
    memory->configuration_present = false;
    return operation_end(memory);
}
static bool erase_logs(void *context) {
    memory_io_t *memory = context;
    ++memory->erases;
    if (!operation_begin(memory)) return false;
    memory->logs_present = false;
    return operation_end(memory);
}
static ag_reset_io_t memory_callbacks(memory_io_t *memory) {
    return (ag_reset_io_t){memory, write_snapshot, erase_credentials, erase_configuration, erase_logs};
}
static bool external_verifier(void *context, const ag_client_id_t *client,
                              const ag_connection_lease_t *lease,
                              const uint8_t *opaque_proof, size_t length) {
    memory_io_t *memory = context;
    (void)opaque_proof;
    return length != 0 && memory->externally_verified_counter == lease->counter
        && memcmp(lease->boot_id, memory->externally_verified_boot, AG_SESSION_ID_BYTES) == 0
        && memcmp(client, &memory->externally_verified_client, sizeof(*client)) == 0;
}
static void initialize(ag_connection_t *connection, ag_factory_reset_t *reset,
                       memory_io_t *memory, ag_session_verifier_t verifier) {
    assert(ag_connection_init(connection, boot_a, verifier, memory));
    const ag_reset_io_t io = memory_callbacks(memory);
    assert(ag_factory_reset_restore(reset, connection, &io, true, &memory->durable));
}
static ag_local_reset_event_t local_event(bool confirmed) {
    return (ag_local_reset_event_t){AG_RESET_ORIGIN_LOCAL_PHYSICAL, true, true, confirmed};
}
static void verify_current(memory_io_t *memory, ag_connection_t *connection,
                            const ag_client_id_t *client, const ag_connection_lease_t *lease) {
    memory->externally_verified_client = *client;
    memory->externally_verified_counter = lease->counter;
    memcpy(memory->externally_verified_boot, lease->boot_id, AG_SESSION_ID_BYTES);
    assert(ag_connection_authenticate(connection, lease, proof, sizeof(proof)) == AG_CONNECTION_OK);
    assert(ag_connection_mutations_allowed(connection, lease));
}

static void test_sequential_clients_no_reset_or_erase(void) {
    memory_io_t memory;
    memory_init(&memory);
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    ag_connection_lease_t a, busy, b, a_again;
    assert(ag_connection_open(&connection, &phone_a, &a) == AG_CONNECTION_OK);
    verify_current(&memory, &connection, &phone_a, &a);
    assert(ag_connection_open(&connection, &phone_b, &busy) == AG_CONNECTION_BUSY);
    assert(busy.counter == 0 && connection.state == AG_CONNECTION_VERIFIED);
    assert(memcmp(&connection.client, &phone_a, sizeof(phone_a)) == 0);
    assert(ag_connection_close(&connection, &a) == AG_CONNECTION_OK);
    assert(ag_connection_open(&connection, &phone_b, &b) == AG_CONNECTION_OK);
    assert(connection.state == AG_CONNECTION_UNVERIFIED && b.counter > a.counter);
    assert(!ag_connection_mutations_allowed(&connection, &b));
    assert(ag_connection_authenticate(&connection, &b, proof, sizeof(proof)) == AG_CONNECTION_AUTH_REJECTED);
    verify_current(&memory, &connection, &phone_b, &b);
    assert(ag_connection_close(&connection, &a) == AG_CONNECTION_STALE_LEASE);
    assert(ag_connection_authenticate(&connection, &a, proof, sizeof(proof)) == AG_CONNECTION_STALE_LEASE);
    assert(connection.state == AG_CONNECTION_VERIFIED && ag_connection_mutations_allowed(&connection, &b));
    assert(ag_connection_close(&connection, &b) == AG_CONNECTION_OK);
    assert(ag_connection_open(&connection, &phone_a, &a_again) == AG_CONNECTION_OK);
    assert(a_again.counter > b.counter && connection.state == AG_CONNECTION_UNVERIFIED);
    assert(!ag_connection_mutations_allowed(&connection, &a));
    assert(ag_connection_authenticate(&connection, &a_again, proof, sizeof(proof)) == AG_CONNECTION_AUTH_REJECTED);
    assert(memory.operations == 0 && memory.erases == 0 && memory.writes == 0);
    assert(memory.credentials_present && memory.configuration_present && memory.logs_present);
}

static void test_missing_real_auth_and_boot_journal_block(void) {
    memory_io_t memory;
    memory_init(&memory);
    ag_connection_t connection;
    ag_factory_reset_t reset;
    assert(ag_connection_init(&connection, boot_a, NULL, NULL));
    ag_connection_lease_t lease;
    assert(ag_connection_open(&connection, &phone_a, &lease) == AG_CONNECTION_MAINTENANCE);
    const ag_reset_io_t io = memory_callbacks(&memory);
    assert(ag_factory_reset_restore(&reset, &connection, &io, true, &memory.durable));
    assert(ag_connection_open(&connection, &phone_a, &lease) == AG_CONNECTION_OK);
    assert(ag_connection_authenticate(&connection, &lease, proof, sizeof(proof)) == AG_CONNECTION_AUTH_UNAVAILABLE);
    assert(!ag_connection_mutations_allowed(&connection, &lease));
    assert(ag_connection_close(&connection, &lease) == AG_CONNECTION_OK);
    connection.verifier = ag_hal_verify_session;
    assert(ag_connection_open(&connection, &phone_b, &lease) == AG_CONNECTION_OK);
    assert(ag_connection_authenticate(&connection, &lease, proof, sizeof(proof)) == AG_CONNECTION_AUTH_REJECTED);
    assert(!ag_connection_mutations_allowed(&connection, &lease));
    assert(ag_hal_reset_write_journal(&memory.durable) == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_reset_erase_credentials() == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_reset_erase_configuration() == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_reset_erase_logs() == AG_HAL_NOT_IMPLEMENTED);
}

static void test_stale_epoch_and_counter_exhaustion(void) {
    memory_io_t memory;
    memory_init(&memory);
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    ag_connection_lease_t old, current;
    assert(ag_connection_open(&connection, &phone_a, &old) == AG_CONNECTION_OK);
    assert(ag_connection_init(&connection, boot_b, external_verifier, &memory));
    const ag_reset_io_t io = memory_callbacks(&memory);
    assert(ag_factory_reset_restore(&reset, &connection, &io, true, &memory.durable));
    assert(ag_connection_open(&connection, &phone_a, &current) == AG_CONNECTION_OK);
    assert(current.counter == old.counter);
    assert(ag_connection_close(&connection, &old) == AG_CONNECTION_STALE_LEASE);
    assert(ag_connection_close(&connection, &current) == AG_CONNECTION_OK);
    connection.lease_counter = UINT64_MAX - 1;
    assert(ag_connection_open(&connection, &phone_a, &current) == AG_CONNECTION_OK);
    assert(current.counter == UINT64_MAX);
    assert(ag_connection_close(&connection, &current) == AG_CONNECTION_OK);
    assert(ag_connection_open(&connection, &phone_b, &current) == AG_CONNECTION_EXHAUSTED);
    assert(current.counter == 0 && connection.lease_counter == UINT64_MAX);
}

static void test_only_qualified_local_confirmation_starts_reset(void) {
    memory_io_t memory;
    memory_init(&memory);
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    ag_connection_lease_t lease, rejected;
    assert(ag_connection_open(&connection, &phone_a, &lease) == AG_CONNECTION_OK);
    verify_current(&memory, &connection, &phone_a, &lease);
    ag_local_reset_event_t event = local_event(true);
    event.origin = AG_RESET_ORIGIN_USB_PRESENCE;
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_NOT_LOCAL);
    event.origin = AG_RESET_ORIGIN_BLE;
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_NOT_LOCAL);
    event = local_event(true); event.physical_qualified = false;
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_NOT_LOCAL);
    event = local_event(true); event.usb_power_valid = false;
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_USB_INVALID);
    assert(memory.operations == 0 && ag_connection_mutations_allowed(&connection, &lease));
    event = local_event(false);
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_OK);
    assert(memory.durable.phase == AG_RESET_JOURNAL_ARMED && memory.erases == 0);
    assert(!ag_connection_mutations_allowed(&connection, &lease));
    assert(ag_connection_open(&connection, &phone_b, &rejected) == AG_CONNECTION_MAINTENANCE);
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_CONFIRMATION_REQUIRED);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_INVALID && memory.erases == 0);
    event = local_event(true); event.origin = AG_RESET_ORIGIN_BLE;
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_NOT_LOCAL);
    event = local_event(true);
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_OK);
    assert(memory.durable.phase == AG_RESET_INTENT && memory.erases == 0);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_OK);
    assert(!memory.credentials_present && memory.configuration_present && memory.logs_present);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_OK);
    assert(!memory.configuration_present && memory.logs_present);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_OK);
    assert(!memory.logs_present && connection.maintenance_blocked);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_OK);
    assert(reset.state == AG_RESET_IDLE && memory.durable.phase == AG_RESET_CLEAN);
    assert(ag_connection_open(&connection, &phone_b, &rejected) == AG_CONNECTION_OK);
    assert(connection.state == AG_CONNECTION_UNVERIFIED && rejected.counter > lease.counter);
    assert(!ag_connection_mutations_allowed(&connection, &lease));
    assert(ag_connection_close(&connection, &lease) == AG_CONNECTION_STALE_LEASE);
    assert(ag_connection_authenticate(&connection, &lease, proof, sizeof(proof)) == AG_CONNECTION_STALE_LEASE);
    assert(ag_connection_authenticate(&connection, &rejected, proof, sizeof(proof)) == AG_CONNECTION_AUTH_REJECTED);
    assert(connection.state == AG_CONNECTION_UNVERIFIED);
}

static void run_until_failure(ag_factory_reset_t *reset) {
    const ag_local_reset_event_t event = local_event(true);
    if (ag_factory_reset_arm(reset, &event) != AG_RESET_OK) return;
    if (ag_factory_reset_confirm(reset, &event) != AG_RESET_OK) return;
    for (unsigned phase = 0; phase < 4; ++phase)
        if (ag_factory_reset_step(reset, true) != AG_RESET_OK) return;
}
static void test_interruptions_before_and_after_every_io(void) {
    for (unsigned after = 0; after <= 1; ++after) {
        for (unsigned cut = 1; cut <= 9; ++cut) {
            memory_io_t memory;
            memory_init(&memory);
            if (after) memory.fail_after = cut; else memory.fail_before = cut;
            ag_connection_t connection;
            ag_factory_reset_t reset;
            initialize(&connection, &reset, &memory, external_verifier);
            ag_connection_lease_t old, blocked;
            assert(ag_connection_open(&connection, &phone_a, &old) == AG_CONNECTION_OK);
            verify_current(&memory, &connection, &phone_a, &old);
            run_until_failure(&reset);
            assert(reset.state == AG_RESET_FAILED && connection.maintenance_blocked);
            assert(!ag_connection_mutations_allowed(&connection, &old));
            assert(ag_connection_open(&connection, &phone_b, &blocked) == AG_CONNECTION_MAINTENANCE);
            assert(memory.operations == cut);
            if (cut <= 2) assert(memory.erases == 0);
            /* A fresh boot restores ONLY the durable snapshot, not manager RAM. */
            ag_connection_t rebooted;
            ag_factory_reset_t recovered;
            assert(ag_connection_init(&rebooted, boot_b, external_verifier, &memory));
            const ag_reset_io_t io = memory_callbacks(&memory);
            assert(ag_factory_reset_restore(&recovered, &rebooted, &io, true, &memory.durable));
            assert(!ag_connection_mutations_allowed(&rebooted, &old));
            if (memory.durable.phase == AG_RESET_CLEAN) {
                /* Before any durable arm: no reset/erase started. After final
                 * commit: all erases completed. Neither reuses the old lease. */
                assert((cut == 1 && !after) || (cut == 9 && after));
                assert(ag_connection_open(&rebooted, &phone_b, &blocked) == AG_CONNECTION_OK);
                assert(rebooted.state == AG_CONNECTION_UNVERIFIED);
                if (cut == 1) assert(memory.erases == 0 && memory.credentials_present);
                else assert(!memory.credentials_present && !memory.configuration_present && !memory.logs_present);
                continue;
            }
            assert(recovered.state == AG_RESET_FAILED && rebooted.maintenance_blocked);
            assert(ag_connection_open(&rebooted, &phone_b, &blocked) == AG_CONNECTION_MAINTENANCE);
            memory.fail_before = memory.fail_after = 0;
            const ag_local_reset_event_t event = local_event(true);
            assert(ag_factory_reset_arm(&recovered, &event) == AG_RESET_OK);
            assert(ag_factory_reset_confirm(&recovered, &event) == AG_RESET_OK);
            for (unsigned guard = 0; recovered.state == AG_RESET_IN_PROGRESS && guard < 4; ++guard)
                assert(ag_factory_reset_step(&recovered, true) == AG_RESET_OK);
            assert(recovered.state == AG_RESET_IDLE);
            assert(!memory.credentials_present && !memory.configuration_present && !memory.logs_present);
            assert(ag_connection_open(&rebooted, &phone_b, &blocked) == AG_CONNECTION_OK);
            assert(rebooted.state == AG_CONNECTION_UNVERIFIED);
        }
    }
}

static void test_corrupt_missing_and_repeated_restore_stay_blocked(void) {
    const ag_reset_snapshot_t known = ag_reset_clean_snapshot();
    assert(known.checksum == UINT32_C(0x4cb498b1)); /* Independent Python CRC32 fixture. */
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        memory_io_t memory;
        memory_init(&memory);
        ag_connection_t connection;
        ag_factory_reset_t reset;
        assert(ag_connection_init(&connection, boot_a, external_verifier, &memory));
        ag_reset_snapshot_t snapshot = known;
        if (scenario == 0) snapshot.checksum ^= 1;
        if (scenario == 1) snapshot.reset_generation ^= 1;
        if (scenario == 2) snapshot.format_version = 99;
        const ag_reset_io_t io = memory_callbacks(&memory);
        assert(!ag_factory_reset_restore(&reset, &connection, &io, scenario != 3,
                                         scenario == 3 ? NULL : &snapshot));
        ag_connection_lease_t lease;
        assert(ag_connection_open(&connection, &phone_a, &lease) == AG_CONNECTION_MAINTENANCE);
        const ag_local_reset_event_t event = local_event(true);
        assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_JOURNAL_INVALID);
        assert(memory.operations == 0);
    }
    memory_io_t memory;
    memory_init(&memory);
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    const ag_reset_io_t io = memory_callbacks(&memory);
    assert(!ag_factory_reset_restore(&reset, &connection, &io, true, &known));
    assert(connection.maintenance_blocked && reset.state == AG_RESET_FAILED);
}

static void test_usb_loss_and_no_intent_no_erase(void) {
    memory_io_t memory;
    memory_init(&memory);
    memory.fail_before = 2; /* ARM durable, INTENT write fails. */
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    const ag_local_reset_event_t event = local_event(true);
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_OK);
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_IO_FAILED);
    assert(memory.erases == 0 && memory.durable.phase == AG_RESET_JOURNAL_ARMED);
    assert(ag_factory_reset_step(&reset, true) == AG_RESET_INVALID);
    memory.fail_before = 0;
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_OK);
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_OK);
    assert(ag_factory_reset_step(&reset, false) == AG_RESET_USB_INVALID);
    assert(memory.erases == 0 && reset.state == AG_RESET_FAILED && connection.maintenance_blocked);
}

static void test_reset_counter_and_runtime_journal_corruption(void) {
    memory_io_t memory;
    memory_init(&memory);
    memory.durable.reset_generation = UINT64_MAX;
    memory.durable.checksum = UINT32_C(0x1efc5164); /* Independent Python CRC32 fixture. */
    assert(ag_reset_snapshot_valid(&memory.durable));
    ag_connection_t connection;
    ag_factory_reset_t reset;
    initialize(&connection, &reset, &memory, external_verifier);
    const ag_local_reset_event_t event = local_event(true);
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_COUNTER_EXHAUSTED);
    assert(memory.operations == 0 && reset.snapshot.reset_generation == UINT64_MAX);
    assert(connection.maintenance_blocked && reset.state == AG_RESET_FAILED);
    memory_init(&memory);
    initialize(&connection, &reset, &memory, external_verifier);
    assert(ag_factory_reset_arm(&reset, &event) == AG_RESET_OK);
    reset.snapshot.completed_mask ^= 1;
    assert(ag_factory_reset_confirm(&reset, &event) == AG_RESET_JOURNAL_INVALID);
    assert(memory.erases == 0 && connection.maintenance_blocked && !reset.journal_valid);
}

int main(void) {
    test_sequential_clients_no_reset_or_erase();
    test_missing_real_auth_and_boot_journal_block();
    test_stale_epoch_and_counter_exhaustion();
    test_only_qualified_local_confirmation_starts_reset();
    test_interruptions_before_and_after_every_io();
    test_corrupt_missing_and_repeated_restore_stay_blocked();
    test_usb_loss_and_no_intent_no_erase();
    test_reset_counter_and_runtime_journal_corruption();
    printf("PASS: 8 connection/reset groups, 18 power-loss IO points; N sequential clients, one active connection, no physical IO\n");
    return 0;
}
