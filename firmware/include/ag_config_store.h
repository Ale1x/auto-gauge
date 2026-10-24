#ifndef AG_CONFIG_STORE_H
#define AG_CONFIG_STORE_H
#include "ag_core.h"

#define AG_CONFIG_SLOT_BYTES 4096u
#define AG_CONFIG_SLOT_COUNT 2u
#define AG_CONFIG_STORE_BYTES (AG_CONFIG_SLOT_BYTES * AG_CONFIG_SLOT_COUNT)
#define AG_CONFIG_HEADER_BYTES 32u
#define AG_CONFIG_PAYLOAD_MAX 167u
#define AG_CONFIG_RECORD_MAX (AG_CONFIG_HEADER_BYTES + AG_CONFIG_PAYLOAD_MAX)
#define AG_CONFIG_COMMIT_OFFSET (AG_CONFIG_SLOT_BYTES - 4u)

extern const uint8_t ag_config_commit_marker[4];
/* Trusted synchronous adapter; offsets are relative to this 8192-byte store.
 * Successful program/erase must be durable before returning; read must observe
 * those writes. No reentrancy/concurrent access. No physical adapter supplied. */
typedef size_t (*ag_config_flash_read_t)(void *, uint32_t offset, uint8_t *, size_t);
typedef size_t (*ag_config_flash_program_t)(void *, uint32_t offset, const uint8_t *, size_t);
typedef bool (*ag_config_flash_erase_t)(void *, uint32_t offset, size_t);
typedef struct {
    void *context;
    ag_config_flash_read_t read;
    ag_config_flash_program_t program;
    ag_config_flash_erase_t erase;
} ag_config_flash_io_t;
typedef enum {
    AG_CONFIG_STORE_OK = 0, AG_CONFIG_STORE_READ_ONLY,
    AG_CONFIG_STORE_INVALID, AG_CONFIG_STORE_NO_VALID_CONFIG,
    AG_CONFIG_STORE_CONFLICT, AG_CONFIG_STORE_IO_ERROR,
    AG_CONFIG_STORE_EXPECTED_REVISION, AG_CONFIG_STORE_REVISION_EXHAUSTED,
    AG_CONFIG_STORE_GENERATION_EXHAUSTED, AG_CONFIG_STORE_CONFIG_INVALID,
    AG_CONFIG_STORE_OUTPUT_SHORT, AG_CONFIG_STORE_CAPABILITY_MISMATCH
} ag_config_store_result_t;
typedef struct {
    ag_config_flash_io_t io;
    ag_pid_mask_t supported;
    ag_config_t active;
    uint64_t generation;
    unsigned active_slot;
    bool initialized, ready, writable, compatible;
} ag_config_store_t;

/* Canonical development record header+payload only; NO commit marker or IO.
 * Useful for golden fixtures. This is not a device provisioning endpoint. */
ag_config_store_result_t ag_config_record_encode(const ag_config_t *, const ag_pid_mask_t *,
                                                 uint64_t generation, uint8_t *output,
                                                 size_t capacity, size_t *written);
ag_config_store_result_t ag_config_store_open(ag_config_store_t *, const ag_config_flash_io_t *,
                                              const ag_pid_mask_t *supported);
ag_config_store_result_t ag_config_store_restore(ag_config_store_t *);
/* Diagnostic snapshot, not proof of operational readiness. READ_ONLY may expose
 * an older revision: the unreadable slot might contain a newer record. Check
 * restore status plus writable/compatible before operational use. A snapshot
 * remains available on CAPABILITY_MISMATCH for revision/repair UI only.
 * Restore/commit have nested automatic 4096-byte buffers and configuration/slot
 * copies; a measured, explicitly sized task stack is required before runtime.
 * The default app_main stack is not qualified for these calls. */
bool ag_config_store_get(const ag_config_store_t *, ag_config_t *configuration);
/* Single owner task/lock. Fresh restore + expected revision checked before IO.
 * No valid configuration means no commit; no implicit defaults/bootstrap. */
ag_config_store_result_t ag_config_store_commit(ag_config_store_t *, uint32_t expected_revision,
                                                const ag_config_t *candidate);
#endif
