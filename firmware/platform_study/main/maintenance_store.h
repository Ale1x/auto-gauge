#ifndef AG_MAINTENANCE_STORE_H
#define AG_MAINTENANCE_STORE_H
#include "ag_connection.h"
#include "partition_io.h"
#include "nvs.h"

/* Local owner only. The credentials hook MUST erase persistent bonds AND
 * in-memory auth caches, while preserving reset/trust/boot-floor namespaces.
 * Initialized NVS and partition binding are supplied; no automatic NVS init,
 * erase-all, defaults, touch/USB qualification or hardware reset is invented. */
typedef struct {
    nvs_handle_t handle;
    ag_partition_store_t *partitions;
    void *credentials_context;
    ag_reset_erase_t erase_credentials;
    bool initialized, fault, busy;
} ag_maintenance_store_t;

/* Explicit confirmed local provisioning. Creates ONLY a missing reset record;
 * existing/corrupt data are never overwritten. May create NVS namespace. An
 * error may follow durable effects; reopen/read before any further decision. */
bool ag_maintenance_provision_reset(const char *nvs_label,
                                   const ag_local_reset_event_t *);
/* Bind probes namespace READONLY before RW, and performs no writes. Static
 * zero object, one bind/boot. Output snapshot/io untouched on failure. */
bool ag_maintenance_store_bind(ag_maintenance_store_t *, const char *nvs_label,
    ag_partition_store_t *, void *credentials_context, ag_reset_erase_t,
    ag_reset_snapshot_t *, ag_reset_io_t *);
/* Bounded restore/reconcile is performed by the existing factory-reset core.
 * Reset phases erase bound config/log partitions; no trust/floor erase. Each
 * callback checks readback through partition_io and latches after any error.
 * A fresh boot/local arm+confirm may resume idempotent phases. No retry in the
 * same faulted owner. Caller must stop runtime/BLE/OTA before maintenance. */
#endif
