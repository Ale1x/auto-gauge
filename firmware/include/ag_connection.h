#ifndef AG_CONNECTION_H
#define AG_CONNECTION_H
#include "ag_core.h"

#define AG_CLIENT_ID_BYTES 16u
#define AG_RESET_SNAPSHOT_VERSION 1u

typedef struct { uint8_t bytes[AG_CLIENT_ID_BYTES]; } ag_client_id_t;
typedef struct { uint8_t boot_id[AG_SESSION_ID_BYTES]; uint64_t counter; } ag_connection_lease_t;
typedef enum { AG_CONNECTION_IDLE = 0, AG_CONNECTION_UNVERIFIED,
               AG_CONNECTION_VERIFIED } ag_connection_state_t;
typedef enum { AG_CONNECTION_OK = 0, AG_CONNECTION_INVALID, AG_CONNECTION_BUSY,
               AG_CONNECTION_MAINTENANCE, AG_CONNECTION_EXHAUSTED,
               AG_CONNECTION_STALE_LEASE, AG_CONNECTION_AUTH_UNAVAILABLE,
               AG_CONNECTION_AUTH_REJECTED } ag_connection_result_t;
/* Trusted adapter hook, NOT a BLE command or a cryptographic implementation. */
typedef bool (*ag_session_verifier_t)(void *context, const ag_client_id_t *client,
                                      const ag_connection_lease_t *lease,
                                      const uint8_t *proof, size_t proof_length);
typedef struct {
    uint8_t boot_id[AG_SESSION_ID_BYTES];
    uint64_t lease_counter;
    ag_client_id_t client;
    ag_connection_state_t state;
    ag_session_verifier_t verifier;
    void *verifier_context;
    bool initialized, boot_journal_loaded, maintenance_blocked;
} ag_connection_t;
/* New unique boot_id on every initialization; no persistent owner registry. */
bool ag_connection_init(ag_connection_t *connection,
                         const uint8_t boot_id[AG_SESSION_ID_BYTES],
                         ag_session_verifier_t verifier, void *verifier_context);
ag_connection_result_t ag_connection_open(ag_connection_t *connection,
                                           const ag_client_id_t *client,
                                           ag_connection_lease_t *lease);
ag_connection_result_t ag_connection_close(ag_connection_t *connection,
                                            const ag_connection_lease_t *lease);
ag_connection_result_t ag_connection_authenticate(ag_connection_t *connection,
                                                   const ag_connection_lease_t *lease,
                                                   const uint8_t *proof, size_t proof_length);
bool ag_connection_mutations_allowed(const ag_connection_t *connection,
                                      const ag_connection_lease_t *lease);

typedef enum { AG_RESET_CLEAN = 0, AG_RESET_JOURNAL_ARMED, AG_RESET_INTENT,
               AG_RESET_CREDENTIALS_ERASED, AG_RESET_CONFIG_ERASED,
               AG_RESET_LOGS_ERASED } ag_reset_phase_t;
typedef struct {
    uint32_t format_version;
    uint64_t reset_generation;
    ag_reset_phase_t phase;
    uint8_t completed_mask;
    uint32_t checksum;
} ag_reset_snapshot_t;
typedef enum { AG_RESET_IDLE = 0, AG_RESET_ARMED, AG_RESET_IN_PROGRESS,
               AG_RESET_FAILED } ag_reset_state_t;
typedef enum { AG_RESET_OK = 0, AG_RESET_INVALID, AG_RESET_NOT_LOCAL,
               AG_RESET_USB_INVALID, AG_RESET_CONFIRMATION_REQUIRED,
               AG_RESET_IO_FAILED, AG_RESET_JOURNAL_INVALID,
               AG_RESET_COUNTER_EXHAUSTED } ag_reset_result_t;
typedef enum { AG_RESET_ORIGIN_UNKNOWN = 0, AG_RESET_ORIGIN_LOCAL_PHYSICAL,
               AG_RESET_ORIGIN_USB_PRESENCE, AG_RESET_ORIGIN_BLE } ag_reset_origin_t;
typedef struct {
    ag_reset_origin_t origin;
    bool physical_qualified, usb_power_valid, local_confirmed;
} ag_local_reset_event_t;
typedef bool (*ag_reset_write_t)(void *context, const ag_reset_snapshot_t *snapshot);
typedef bool (*ag_reset_erase_t)(void *context);
typedef struct {
    void *context;
    ag_reset_write_t write_snapshot;
    /* Credentials erase includes persistent bonds AND runtime key/bond/auth
     * caches. No verifier may keep trusting a cached pre-reset credential. */
    ag_reset_erase_t erase_credentials, erase_configuration, erase_logs;
} ag_reset_io_t;
typedef struct {
    ag_connection_t *connection;
    ag_reset_snapshot_t snapshot;
    ag_reset_io_t io;
    ag_reset_state_t state;
    bool journal_valid;
} ag_factory_reset_t;
/* Canonical CRC is accidental-corruption detection, NOT authentication. */
ag_reset_snapshot_t ag_reset_clean_snapshot(void);
bool ag_reset_snapshot_valid(const ag_reset_snapshot_t *snapshot);
/* Boot must restore before accepting connections. One restore per connection init. */
bool ag_factory_reset_restore(ag_factory_reset_t *reset, ag_connection_t *connection,
                              const ag_reset_io_t *io, bool load_succeeded,
                              const ag_reset_snapshot_t *snapshot);
ag_reset_result_t ag_factory_reset_arm(ag_factory_reset_t *reset, const ag_local_reset_event_t *event);
ag_reset_result_t ag_factory_reset_confirm(ag_factory_reset_t *reset, const ag_local_reset_event_t *event);
/* One bounded erase/write phase per call; IO callbacks must be idempotent. */
ag_reset_result_t ag_factory_reset_step(ag_factory_reset_t *reset, bool usb_power_valid);

#endif
