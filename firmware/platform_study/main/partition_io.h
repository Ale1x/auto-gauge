#ifndef AG_PARTITION_IO_H
#define AG_PARTITION_IO_H
#include "ag_config_store.h"
#include "ag_journal_history.h"
#include "esp_partition.h"
/* TRUSTED local layout only. Never decoded from a phone or persisted config.
 * Custom DATA subtypes>=0x40, distinct bounded labels, no encrypted partitions:
 * existing formats need arbitrary byte writes and 1->0 bitmap programming. */
typedef struct {
    char config_label[17],history_label[17];
    uint8_t config_subtype,history_subtype;
    uint16_t history_sectors;
} ag_partition_layout_t;
typedef struct {
    const esp_partition_t *config,*history;
    esp_partition_t config_identity,history_identity;
    ag_config_flash_io_t config_io;
    ag_journal_io_t history_io;
    bool initialized,busy,write_fault;
} ag_partition_store_t;
/* Static zero-initialized owner, one bind/boot, no reentry. Enumerates/validates
 * registered default-chip partitions and queries chip size. NO partition-data
 * read, erase, write, format, bootstrap
 * or initial config creation. SDK partition-table loading may allocate internally.
 * Config size exactly8192; journal geometry exactly2..512 sectors4KiB.
 * Every selected range must be disjoint from ALL other same-chip partitions. */
bool ag_partition_store_bind(ag_partition_store_t *,const ag_partition_layout_t *);
/* On bind success use store.config_io/history_io with the existing cores.
 * All callers and partition writers serialized. Input/output must not alias
 * store, partition descriptors or each other; input stays immutable throughout.
 * Read diagnostics remain possible after a fault, but writes/erases latch off
 * after any ambiguous SDK IO/readback, metadata change or illegal0->1 request.
 * Successful SDK calls + readback are NOT physical brownout/wear qualification.
 * No in-place clear-fault/retry; reconcile storage via explicit maintenance. */
#endif
