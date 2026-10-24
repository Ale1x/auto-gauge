#ifndef AG_OTA_FRAMED_ADAPTER_H
#define AG_OTA_FRAMED_ADAPTER_H
#include "transfer_adapter.h"
#include "ota_framed_endpoint.h"
/* Static zero-initialized exclusive platform session. Trust/lease must originate
 * locally; no content-based RPC-to-OTA routing or radio initialization occurs. */
typedef struct {
    ag_ota_transfer_adapter_t payload;
    ag_ot_endpoint_t endpoint;
    bool initialized;
} ag_ota_framed_adapter_t;
bool ag_ota_framed_adapter_init(ag_ota_framed_adapter_t *,const ag_manifest_trust_t *,
    const ag_ota_transfer_platform_t *,uint64_t lease,uint32_t frame_timeout_ms,uint32_t request_timeout_ms);
ag_ot_ep_result_t ag_ota_framed_adapter_feed(ag_ota_framed_adapter_t *,uint64_t,const uint8_t *,size_t);
ag_ot_ep_result_t ag_ota_framed_adapter_step(ag_ota_framed_adapter_t *,uint64_t);
ag_ot_ep_result_t ag_ota_framed_adapter_take_output(ag_ota_framed_adapter_t *,uint64_t,uint8_t *,size_t,size_t *);
ag_ot_ep_result_t ag_ota_framed_adapter_poll(ag_ota_framed_adapter_t *,uint64_t);
ag_ot_ep_result_t ag_ota_framed_adapter_close(ag_ota_framed_adapter_t *,uint64_t);
#endif
