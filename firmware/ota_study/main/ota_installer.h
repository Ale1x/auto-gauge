#ifndef AG_OTA_INSTALLER_H
#define AG_OTA_INSTALLER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_ota_ops.h"

#define AG_OTA_CHUNK 1024u
#define AG_OTA_IMAGE_MAX (16u*1024u*1024u)
typedef struct {
    uint32_t image_bytes,minimum_secure_version;
    uint8_t sha256[32];
    char firmware_version[32];
} ag_ota_plan_t;
typedef struct {
    void *context;
    /* Mandatory TRUSTED verifier: authenticate manifest/key/hardware/counter and
     * return its plan. Never a wire boolean or an unverified JSON decoder.
     * manifest_verify.c supplies the verifier; production trust provisioning
     * and platform binding remain explicit caller responsibilities. */
    bool (*verify_manifest)(void *,const uint8_t *,size_t,ag_ota_plan_t *);
    bool (*maintenance_allowed)(void *,uint64_t lease);
    uint64_t (*clock_ms)(void *);
    uint32_t idle_timeout_ms,total_timeout_ms;
} ag_ota_hooks_t;
typedef enum { AG_OTA_IDLE=0,AG_OTA_WRITING,AG_OTA_READY,AG_OTA_SELECTED,AG_OTA_FAULT } ag_ota_phase_t;
typedef enum { AG_OTA_OK=0,AG_OTA_INVALID,AG_OTA_DENIED,AG_OTA_EXPIRED,
    AG_OTA_PARTITIONS,AG_OTA_IO,AG_OTA_IMAGE,AG_OTA_HASH,AG_OTA_BUSY } ag_ota_result_t;
typedef struct {
    ag_ota_hooks_t hooks;
    ag_ota_plan_t plan;
    const esp_partition_t *running,*target;
    esp_ota_handle_t handle;
    uint64_t lease,started_ms,last_ms,activity_ms;
    uint32_t written;
    ag_ota_phase_t phase;
    bool initialized,busy,handle_active,cleanup_unknown,selection_may_have_changed;
} ag_ota_installer_t;
/* ZERO-initialized static object, one installation/boot. One owner of ALL OTA/partition
 * writers, no callback reentry. Hooks/context live throughout the installation.
 * APIs below WOULD erase/write real flash if called; study app_main calls none. */
bool ag_ota_installer_init(ag_ota_installer_t *,const ag_ota_hooks_t *);
ag_ota_result_t ag_ota_installer_begin(ag_ota_installer_t *,uint64_t lease,const uint8_t *manifest,size_t);
ag_ota_result_t ag_ota_installer_write(ag_ota_installer_t *,uint64_t lease,uint32_t offset,const uint8_t *,size_t);
/* SDK image verification + SHA256 of bytes reread from inactive partition.
 * Does not select boot; finish consumes SDK handle even when SDK rejects image. */
ag_ota_result_t ag_ota_installer_finish(ag_ota_installer_t *,uint64_t lease);
/* Explicit separate operation: rehash/recheck then SDK boot selection. Any SDK
 * selection attempt can be ambiguous; never blindly retry or promise rollback.
 * No reboot, health confirmation or durable release-floor advancement here. */
ag_ota_result_t ag_ota_installer_select(ag_ota_installer_t *,uint64_t lease);
ag_ota_result_t ag_ota_installer_poll(ag_ota_installer_t *,uint64_t lease);
ag_ota_result_t ag_ota_installer_cancel(ag_ota_installer_t *,uint64_t lease);
#endif
