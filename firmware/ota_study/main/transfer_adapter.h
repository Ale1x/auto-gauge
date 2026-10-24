#ifndef AG_OTA_TRANSFER_ADAPTER_H
#define AG_OTA_TRANSFER_ADAPTER_H
#include "manifest_verify.h"
#include "ota_transfer.h"
/* Trusted platform callbacks, not remote assertions. Context must outlive this
 * zero-initialized static owner. One owner of every OTA/partition writer. */
typedef struct {
    void *context;
    bool (*maintenance_allowed)(void *,uint64_t lease);
    uint64_t (*clock_ms)(void *);
    uint32_t idle_timeout_ms,total_timeout_ms;
} ag_ota_transfer_platform_t;
typedef struct {
    ag_ot_owner_t transfer;
    ag_ota_installer_t installer;
    ag_manifest_trust_t trust;
    ag_manifest_verified_t verified;
    ag_ota_transfer_platform_t platform;
    bool initialized,manifest_verified;
} ag_ota_transfer_adapter_t;
/* Copies trust/config, retains only their explicitly supplied callback contexts.
 * No flash or crypto runs during init. No default/product trust is provided. */
bool ag_ota_transfer_adapter_init(ag_ota_transfer_adapter_t *,const ag_manifest_trust_t *,
                                  const ag_ota_transfer_platform_t *);
/* Complete OTA payloads only. Platform must establish authenticated lease,
 * local approval and exclusive OTA ownership before dispatch; no native route
 * is installed by this study. These operations WOULD write/select real flash. */
ag_ot_response_t ag_ota_transfer_adapter_handle(ag_ota_transfer_adapter_t *,uint64_t,
                                               const uint8_t *,size_t);
ag_ot_response_t ag_ota_transfer_adapter_poll(ag_ota_transfer_adapter_t *,uint64_t);
#endif
