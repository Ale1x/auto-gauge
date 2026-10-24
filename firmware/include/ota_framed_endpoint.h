#ifndef AG_OTA_FRAMED_ENDPOINT_H
#define AG_OTA_FRAMED_ENDPOINT_H
#include "ota_transfer.h"
#include "ag_stream.h"

typedef enum {
    AG_OT_EP_OK=0, AG_OT_EP_INVALID, AG_OT_EP_DENIED, AG_OT_EP_BUSY,
    AG_OT_EP_CLOSED, AG_OT_EP_NO_OUTPUT, AG_OT_EP_OUTPUT_SHORT,
    AG_OT_EP_TIMEOUT, AG_OT_EP_BACKEND_FAILURE
} ag_ot_ep_result_t;
typedef struct {
    void *context;
    bool (*allowed)(void *,uint64_t lease);
    uint64_t (*clock_ms)(void *);
    uint32_t frame_timeout_ms,request_timeout_ms;
} ag_ot_ep_hooks_t;
typedef struct {
    ag_ot_owner_t *owner;
    ag_ot_ep_hooks_t hooks;
    ag_stream_t stream;
    uint8_t staged_payload[AG_OTA_TRANSFER_REQUEST_MAX];
    uint8_t output[AG_OTA_TRANSFER_RESPONSE_BYTES+16u];
    size_t staged_length;
    uint64_t lease,next_request_id,last_ms,pending_ms;
    uint32_t staged_id;
    bool initialized,busy,closed,has_time,staged,has_output,close_attempted;
} ag_ot_endpoint_t;
/* Static zero initialized endpoint, one init/session; owner is not reset.
 * One serialized exclusive OTA connection and owner, trusted callbacks and
 * lease supplied locally. Lifetimes exceed endpoint. No BLE route is installed. */
bool ag_ot_endpoint_init(ag_ot_endpoint_t *,ag_ot_owner_t *,uint64_t lease,const ag_ot_ep_hooks_t *);
/* Feed stages a sole complete AG request, with no trailing bytes. It never
 * dispatches owner mutation. Caller runs step then takes the complete output. */
ag_ot_ep_result_t ag_ot_endpoint_feed(ag_ot_endpoint_t *,uint64_t,const uint8_t *,size_t);
ag_ot_ep_result_t ag_ot_endpoint_step(ag_ot_endpoint_t *,uint64_t);
/* Output/written and input chunks must not overlap endpoint or owner storage. */
ag_ot_ep_result_t ag_ot_endpoint_take_output(ag_ot_endpoint_t *,uint64_t,uint8_t *,size_t,size_t *written);
/* Periodic even without traffic. Poll never refreshes frame/pending deadlines. */
ag_ot_ep_result_t ag_ot_endpoint_poll(ag_ot_endpoint_t *,uint64_t);
/* Matching close seals terminal and cancels own active transfer at most once.
 * Stale leases/reentry leave current state and owner unchanged. */
ag_ot_ep_result_t ag_ot_endpoint_close(ag_ot_endpoint_t *,uint64_t);
#endif
