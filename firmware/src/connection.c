#include "ag_connection.h"
#include <string.h>

static bool nonzero(const uint8_t *bytes, size_t size) {
    if (bytes == NULL) return false;
    uint8_t any = 0;
    for (size_t i = 0; i < size; ++i) any |= bytes[i];
    return any != 0;
}
static void invalidate_session(ag_connection_t *connection) {
    connection->state = AG_CONNECTION_IDLE;
    memset(&connection->client, 0, sizeof(connection->client));
}
static bool lease_current(const ag_connection_t *connection, const ag_connection_lease_t *lease) {
    return connection != NULL && lease != NULL && connection->initialized
        && connection->boot_journal_loaded && !connection->maintenance_blocked
        && connection->state != AG_CONNECTION_IDLE && lease->counter != 0
        && lease->counter == connection->lease_counter
        && memcmp(lease->boot_id, connection->boot_id, AG_SESSION_ID_BYTES) == 0;
}
bool ag_connection_init(ag_connection_t *connection,
                         const uint8_t boot_id[AG_SESSION_ID_BYTES],
                         ag_session_verifier_t verifier, void *verifier_context) {
    if (connection == NULL) return false;
    uint8_t boot_copy[AG_SESSION_ID_BYTES] = {0};
    if (boot_id != NULL) memcpy(boot_copy, boot_id, sizeof(boot_copy));
    memset(connection, 0, sizeof(*connection));
    connection->maintenance_blocked = true;
    if (!nonzero(boot_copy, sizeof(boot_copy))) return false;
    memcpy(connection->boot_id, boot_copy, sizeof(boot_copy));
    connection->verifier = verifier;
    connection->verifier_context = verifier_context;
    connection->initialized = true;
    return true;
}
ag_connection_result_t ag_connection_open(ag_connection_t *connection,
                                           const ag_client_id_t *client,
                                           ag_connection_lease_t *lease) {
    if (lease != NULL) memset(lease, 0, sizeof(*lease));
    if (connection == NULL || !connection->initialized || lease == NULL || client == NULL
        || !nonzero(client->bytes, sizeof(client->bytes))) return AG_CONNECTION_INVALID;
    if (!connection->boot_journal_loaded || connection->maintenance_blocked) return AG_CONNECTION_MAINTENANCE;
    if (connection->state != AG_CONNECTION_IDLE) return AG_CONNECTION_BUSY;
    if (connection->lease_counter == UINT64_MAX) return AG_CONNECTION_EXHAUSTED;
    ++connection->lease_counter;
    connection->client = *client;
    connection->state = AG_CONNECTION_UNVERIFIED;
    memcpy(lease->boot_id, connection->boot_id, sizeof(lease->boot_id));
    lease->counter = connection->lease_counter;
    return AG_CONNECTION_OK;
}
ag_connection_result_t ag_connection_close(ag_connection_t *connection,
                                            const ag_connection_lease_t *lease) {
    if (!lease_current(connection, lease)) return AG_CONNECTION_STALE_LEASE;
    invalidate_session(connection);
    return AG_CONNECTION_OK;
}
ag_connection_result_t ag_connection_authenticate(ag_connection_t *connection,
                                                   const ag_connection_lease_t *lease,
                                                   const uint8_t *proof, size_t proof_length) {
    if (!lease_current(connection, lease)) return AG_CONNECTION_STALE_LEASE;
    connection->state = AG_CONNECTION_UNVERIFIED;
    if (connection->verifier == NULL) return AG_CONNECTION_AUTH_UNAVAILABLE;
    if (proof == NULL || proof_length == 0) return AG_CONNECTION_AUTH_REJECTED;
    const bool verified = connection->verifier(connection->verifier_context, &connection->client,
                                                lease, proof, proof_length);
    if (!lease_current(connection, lease)) return AG_CONNECTION_STALE_LEASE;
    if (!verified) return AG_CONNECTION_AUTH_REJECTED;
    connection->state = AG_CONNECTION_VERIFIED;
    return AG_CONNECTION_OK;
}
bool ag_connection_mutations_allowed(const ag_connection_t *connection,
                                      const ag_connection_lease_t *lease) {
    return lease_current(connection, lease) && connection->state == AG_CONNECTION_VERIFIED
        && connection->verifier != NULL;
}

/* CRC-32/ISO-HDLC over canonical LE integers, no raw struct/padding involved. */
static uint32_t checksum(const ag_reset_snapshot_t *snapshot) {
    uint8_t bytes[14];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (uint8_t)(snapshot->format_version >> (8u * i));
    for (unsigned i = 0; i < 8; ++i) bytes[4 + i] = (uint8_t)(snapshot->reset_generation >> (8u * i));
    bytes[12] = (uint8_t)snapshot->phase;
    bytes[13] = snapshot->completed_mask;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) != 0 ? UINT32_C(0xedb88320) : 0);
    }
    return crc ^ UINT32_MAX;
}
static uint8_t expected_mask(ag_reset_phase_t phase) {
    switch (phase) {
        case AG_RESET_CLEAN: case AG_RESET_JOURNAL_ARMED: case AG_RESET_INTENT: return 0;
        case AG_RESET_CREDENTIALS_ERASED: return 1;
        case AG_RESET_CONFIG_ERASED: return 3;
        case AG_RESET_LOGS_ERASED: return 7;
        default: return UINT8_MAX;
    }
}
ag_reset_snapshot_t ag_reset_clean_snapshot(void) {
    ag_reset_snapshot_t snapshot = {.format_version = AG_RESET_SNAPSHOT_VERSION, .phase = AG_RESET_CLEAN};
    snapshot.checksum = checksum(&snapshot);
    return snapshot;
}
bool ag_reset_snapshot_valid(const ag_reset_snapshot_t *snapshot) {
    return snapshot != NULL && snapshot->format_version == AG_RESET_SNAPSHOT_VERSION
        && expected_mask(snapshot->phase) != UINT8_MAX
        && snapshot->completed_mask == expected_mask(snapshot->phase)
        && (snapshot->phase == AG_RESET_CLEAN || snapshot->reset_generation != 0)
        && snapshot->checksum == checksum(snapshot);
}
static void maintenance_block(ag_factory_reset_t *reset) {
    reset->connection->maintenance_blocked = true;
    invalidate_session(reset->connection);
}
bool ag_factory_reset_restore(ag_factory_reset_t *reset, ag_connection_t *connection,
                              const ag_reset_io_t *io, bool load_succeeded,
                              const ag_reset_snapshot_t *snapshot) {
    if (reset == NULL || connection == NULL || !connection->initialized) return false;
    const bool already_loaded = connection->boot_journal_loaded;
    ag_reset_snapshot_t copy = {0};
    if (snapshot != NULL) copy = *snapshot;
    ag_reset_io_t io_copy = {0};
    if (io != NULL) io_copy = *io;
    memset(reset, 0, sizeof(*reset));
    reset->connection = connection;
    reset->io = io_copy;
    reset->snapshot = copy;
    if (already_loaded) {
        reset->state = AG_RESET_FAILED;
        maintenance_block(reset);
        return false;
    }
    reset->journal_valid = load_succeeded && ag_reset_snapshot_valid(&copy);
    connection->boot_journal_loaded = true;
    if (!reset->journal_valid || copy.phase != AG_RESET_CLEAN) {
        reset->state = AG_RESET_FAILED;
        maintenance_block(reset);
        return reset->journal_valid;
    }
    reset->state = AG_RESET_IDLE;
    connection->maintenance_blocked = false;
    invalidate_session(connection);
    return true;
}
static ag_reset_result_t qualified_event(const ag_local_reset_event_t *event, bool confirmation) {
    if (event == NULL || event->origin != AG_RESET_ORIGIN_LOCAL_PHYSICAL || !event->physical_qualified)
        return AG_RESET_NOT_LOCAL;
    if (!event->usb_power_valid) return AG_RESET_USB_INVALID;
    if (confirmation && !event->local_confirmed) return AG_RESET_CONFIRMATION_REQUIRED;
    return AG_RESET_OK;
}
static bool journal_valid(ag_factory_reset_t *reset) {
    if (reset->journal_valid && ag_reset_snapshot_valid(&reset->snapshot)) return true;
    reset->journal_valid = false;
    reset->state = AG_RESET_FAILED;
    maintenance_block(reset);
    return false;
}
static bool persist(ag_factory_reset_t *reset, ag_reset_phase_t phase, uint64_t generation) {
    ag_reset_snapshot_t candidate = {.format_version = AG_RESET_SNAPSHOT_VERSION,
        .phase = phase, .reset_generation = generation, .completed_mask = expected_mask(phase)};
    candidate.checksum = checksum(&candidate);
    if (reset->io.write_snapshot == NULL || !reset->io.write_snapshot(reset->io.context, &candidate)) {
        reset->state = AG_RESET_FAILED;
        maintenance_block(reset);
        return false;
    }
    reset->snapshot = candidate;
    return true;
}
ag_reset_result_t ag_factory_reset_arm(ag_factory_reset_t *reset, const ag_local_reset_event_t *event) {
    if (reset == NULL || reset->connection == NULL) return AG_RESET_INVALID;
    if (!journal_valid(reset)) return AG_RESET_JOURNAL_INVALID;
    const ag_reset_result_t qualification = qualified_event(event, false);
    if (qualification != AG_RESET_OK) return qualification;
    if (reset->state != AG_RESET_IDLE && reset->state != AG_RESET_FAILED) return AG_RESET_INVALID;
    maintenance_block(reset);
    if (reset->snapshot.phase == AG_RESET_CLEAN) {
        if (reset->snapshot.reset_generation == UINT64_MAX) {
            reset->state = AG_RESET_FAILED;
            return AG_RESET_COUNTER_EXHAUSTED;
        }
        if (!persist(reset, AG_RESET_JOURNAL_ARMED, reset->snapshot.reset_generation + 1))
            return AG_RESET_IO_FAILED;
    }
    reset->state = AG_RESET_ARMED;
    return AG_RESET_OK;
}
ag_reset_result_t ag_factory_reset_confirm(ag_factory_reset_t *reset, const ag_local_reset_event_t *event) {
    if (reset == NULL || reset->connection == NULL) return AG_RESET_INVALID;
    if (!journal_valid(reset)) return AG_RESET_JOURNAL_INVALID;
    const ag_reset_result_t qualification = qualified_event(event, true);
    if (qualification != AG_RESET_OK) return qualification;
    if (reset->state != AG_RESET_ARMED) return AG_RESET_INVALID;
    if (reset->snapshot.phase == AG_RESET_JOURNAL_ARMED) {
        if (!persist(reset, AG_RESET_INTENT, reset->snapshot.reset_generation)) return AG_RESET_IO_FAILED;
    }
    reset->state = AG_RESET_IN_PROGRESS;
    return AG_RESET_OK;
}
ag_reset_result_t ag_factory_reset_step(ag_factory_reset_t *reset, bool usb_power_valid) {
    if (reset == NULL || reset->connection == NULL || reset->state != AG_RESET_IN_PROGRESS)
        return AG_RESET_INVALID;
    if (!journal_valid(reset)) return AG_RESET_JOURNAL_INVALID;
    if (!usb_power_valid) { reset->state = AG_RESET_FAILED; return AG_RESET_USB_INVALID; }
    ag_reset_erase_t erase = NULL;
    ag_reset_phase_t next;
    switch (reset->snapshot.phase) {
        case AG_RESET_INTENT: erase = reset->io.erase_credentials; next = AG_RESET_CREDENTIALS_ERASED; break;
        case AG_RESET_CREDENTIALS_ERASED: erase = reset->io.erase_configuration; next = AG_RESET_CONFIG_ERASED; break;
        case AG_RESET_CONFIG_ERASED: erase = reset->io.erase_logs; next = AG_RESET_LOGS_ERASED; break;
        case AG_RESET_LOGS_ERASED: next = AG_RESET_CLEAN; break;
        default: reset->state = AG_RESET_FAILED; return AG_RESET_JOURNAL_INVALID;
    }
    if (next != AG_RESET_CLEAN && (erase == NULL || !erase(reset->io.context))) {
        reset->state = AG_RESET_FAILED;
        return AG_RESET_IO_FAILED;
    }
    if (!persist(reset, next, reset->snapshot.reset_generation)) return AG_RESET_IO_FAILED;
    if (next == AG_RESET_CLEAN) {
        reset->state = AG_RESET_IDLE;
        reset->connection->maintenance_blocked = false;
        invalidate_session(reset->connection);
    }
    return AG_RESET_OK;
}
