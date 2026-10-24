#ifndef AG_ADMISSION_H
#define AG_ADMISSION_H
#include "ag_connection.h"

/* Development resource bounds, not product timing/cache requirements. */
#define AG_ADMISSION_MAX_WINDOW_MS 600000u
#define AG_ADMISSION_MAX_AUTH_MS 600000u
#define AG_ADMISSION_MAX_COOLDOWN_MS 3600000u
#define AG_ADMISSION_MAX_ATTEMPTS 10u
#define AG_ADMISSION_ENTROPY_TRIES 8u

typedef enum { AG_BOND_UNKNOWN=0, AG_BOND_RECOGNIZED, AG_BOND_LOOKUP_FAILED } ag_bond_lookup_t;
typedef struct {
    bool encrypted, mitm_authenticated, secure_connections, identity_resolved, bonded;
    uint8_t key_bits; /* Exactly 128 required. */
    /* New peer: stack confirms this lease paired using the passkey supplied
     * for this local display attempt. Not just generic MITM via another method. */
    bool current_display_passkey_used;
} ag_link_security_t;
/* Trusted stack/HAL hooks ONLY. Never fill evidence/identity from a BLE payload.
 * All hooks bind current resolved peer+boot/lease to the real link. SC needs
 * explicit evidence beyond NimBLE's basic conn_desc.sec_state fields. */
typedef struct {
    void *context;
    bool (*random_u32)(void *, uint32_t *);
    ag_bond_lookup_t (*lookup)(void *, const ag_client_id_t *);
    bool (*read_security)(void *, const ag_client_id_t *, const ag_connection_lease_t *, ag_link_security_t *);
    /* Confirm the stack retained a usable bond; may fail for storage full.
     * No cache eviction, key generation, cloud or config/log erase in this core. */
    bool (*confirm_bond)(void *, const ag_client_id_t *, const ag_connection_lease_t *);
} ag_admission_hooks_t;
typedef struct { uint32_t window_ms, auth_timeout_ms, cooldown_ms; uint8_t attempts; } ag_admission_config_t;
typedef enum {
    AG_ADMISSION_OK=0, AG_ADMISSION_INVALID, AG_ADMISSION_NOT_LOCAL,
    AG_ADMISSION_BUSY, AG_ADMISSION_WINDOW_CLOSED, AG_ADMISSION_COOLDOWN,
    AG_ADMISSION_STALE, AG_ADMISSION_REJECTED, AG_ADMISSION_ENTROPY_FAILED,
    AG_ADMISSION_TIMEOUT, AG_ADMISSION_CLOCK_FAULT, AG_ADMISSION_STORAGE_FAILED
} ag_admission_result_t;
typedef struct {
    ag_connection_t *connection;
    ag_admission_config_t config;
    ag_admission_hooks_t hooks;
    ag_connection_lease_t lease;
    uint64_t last_ms, window_started_ms, connected_ms, cooldown_started_ms;
    uint32_t passkey;
    uint8_t attempts_used;
    bool initialized, has_time, clock_fault, window_open, cooling_down;
    bool link_open, recognized, passkey_issued, verification_in_progress;
} ag_admission_t;
/* Attach once to an initialized, idle connection with verifier=NULL. Does not
 * bypass/reset its mandatory boot journal gate. Owns its verifier thereafter.
 * Serialized owner task; hooks must not reenter. No hardware adapter supplied. */
bool ag_admission_init(ag_admission_t *, ag_connection_t *, const ag_admission_config_t *,
                        const ag_admission_hooks_t *);
ag_admission_result_t ag_admission_tick(ag_admission_t *, uint64_t now_ms);
/* Only trusted local touch/menu adapter can qualify this event, never BLE. */
ag_admission_result_t ag_admission_open_window(ag_admission_t *, bool qualified_local_menu, uint64_t now_ms);
ag_admission_result_t ag_admission_connect(ag_admission_t *, const ag_client_id_t *, uint64_t now_ms,
                                           ag_connection_lease_t *);
/* Trusted SMP/display-only output. Format %06u; do not advertise/log/persist it.
 * This is NOT a remote passkey comparison or DIY authentication endpoint. */
ag_admission_result_t ag_admission_passkey(ag_admission_t *, const ag_connection_lease_t *,
                                           uint64_t now_ms, char display[7]);
ag_admission_result_t ag_admission_security_changed(ag_admission_t *, const ag_connection_lease_t *, uint64_t now_ms);
/* Mandatory before private reads, notifications, writes or any command. */
bool ag_admission_private_allowed(ag_admission_t *, const ag_connection_lease_t *, uint64_t now_ms);
ag_admission_result_t ag_admission_disconnect(ag_admission_t *, const ag_connection_lease_t *, uint64_t now_ms);
/* Reject repeat pairing without deleting a bond or replacing the active peer. */
ag_admission_result_t ag_admission_repeat_pairing(ag_admission_t *, const ag_connection_lease_t *, uint64_t now_ms);
#endif
