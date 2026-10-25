#ifndef AG_OTA_BOOT_STORAGE_H
#define AG_OTA_BOOT_STORAGE_H
#include "ag_boot_health.h"
#include "manifest_verify.h"
#include "nvs.h"
typedef struct {nvs_handle_t handle;bool initialized,fault;} ag_ota_boot_storage_t;
/* Named NVS partition must be initialized explicitly by the platform first.
 * No NVS erase/init/fallback/provision/health confirmation is automatic here.
 * Local label only, stable object/one owner. Handle lives until reboot. */
/* Explicit local maintenance provisioning of namespace metadata only. May
 * write NVS even before a record exists; no auto erase/retry/floor default. */
bool ag_ota_boot_storage_provision_namespace(const char *partition_label);
bool ag_ota_boot_storage_bind(ag_ota_boot_storage_t *,const char *partition_label,ag_boot_hooks_t *);
/* Suitable trusted prepare_selection hook context: this boot state. Requires
 * ready durable floor matching verifier trust, verified manifest and installer
 * target. No package or wire boolean can stand in for this durable operation. */
bool ag_ota_boot_prepare_selection(void *boot_context,const ag_manifest_verified_t *,const esp_partition_t *target);
#endif
