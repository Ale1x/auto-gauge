#ifndef AG_OTA_TRANSFER_H
#define AG_OTA_TRANSFER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AG_OTA_TRANSFER_MANIFEST_MAX 4096u
#define AG_OTA_TRANSFER_CHUNK_MAX 1024u
#define AG_OTA_TRANSFER_IMAGE_MAX (16u * 1024u * 1024u)
#define AG_OTA_TRANSFER_REQUEST_HEADER 12u
#define AG_OTA_TRANSFER_REQUEST_MAX 1036u
#define AG_OTA_TRANSFER_RESPONSE_BYTES 16u

typedef enum { AG_OT_START=1, AG_OT_MANIFEST, AG_OT_BEGIN, AG_OT_IMAGE,
    AG_OT_FINISH, AG_OT_SELECT, AG_OT_CANCEL } ag_ot_opcode_t;
typedef enum { AG_OT_OK=0, AG_OT_INVALID, AG_OT_DENIED, AG_OT_EXPIRED,
    AG_OT_BACKEND_FAILURE, AG_OT_BUSY } ag_ot_status_t;
typedef enum { AG_OT_IDLE=0, AG_OT_MANIFEST_PHASE, AG_OT_WRITING,
    AG_OT_READY, AG_OT_SELECTED, AG_OT_FAULT } ag_ot_phase_t;
typedef struct {
    ag_ot_opcode_t opcode;
    uint32_t transfer_id,offset;
    const uint8_t *body; /* borrowed until caller changes payload */
    size_t body_length;
} ag_ot_request_t;
typedef struct {
    ag_ot_status_t status;
    ag_ot_phase_t phase;
    bool selection_may_have_changed;
    uint32_t transfer_id,manifest_offset,image_offset;
} ag_ot_response_t;
/* Strict complete payload codecs; output untouched on failure. Encode output
 * must not overlap request body. These bytes do not authenticate a lease. */
bool ag_ot_request_decode(const uint8_t *,size_t,ag_ot_request_t *);
bool ag_ot_request_encode(const ag_ot_request_t *,uint8_t *,size_t,size_t *);
bool ag_ot_response_decode(const uint8_t *,size_t,ag_ot_response_t *);
bool ag_ot_response_encode(const ag_ot_response_t *,uint8_t *,size_t);

typedef struct {
    void *context;
    bool (*allowed)(void *,uint64_t lease);
    uint64_t (*clock_ms)(void *);
    /* begin must cryptographically verify manifest using local trust and call
     * installer begin, returning only the verified plan's image length. */
    bool (*begin)(void *,uint64_t lease,const uint8_t *,size_t,uint32_t *image_bytes);
    bool (*write)(void *,uint64_t lease,uint32_t offset,const uint8_t *,size_t);
    bool (*finish)(void *,uint64_t lease);
    bool (*select)(void *,uint64_t lease);
    bool (*poll)(void *,uint64_t lease);
    bool (*cancel)(void *,uint64_t lease);
    uint32_t idle_timeout_ms,total_timeout_ms;
} ag_ot_hooks_t;
typedef struct {
    ag_ot_hooks_t hooks;
    uint8_t manifest[AG_OTA_TRANSFER_MANIFEST_MAX];
    uint64_t lease,started_ms,last_ms,activity_ms;
    uint32_t transfer_id,manifest_bytes,manifest_offset,image_bytes,image_offset;
    ag_ot_phase_t phase;
    bool initialized,busy,begin_attempted,cancel_attempted;
    bool selection_may_have_changed,cleanup_unknown;
} ag_ot_owner_t;
/* Static ZERO initialized object, one init and transfer/boot. One serialized
 * owner; hooks and context remain valid. No native endpoint activation here. */
bool ag_ot_owner_init(ag_ot_owner_t *,const ag_ot_hooks_t *);
/* Request parse/ordering rejections leave progress unchanged. A stale lease is
 * denied without touching another owner's state. ACK reports current progress;
 * failure after callback is terminal and may have side effects. */
ag_ot_response_t ag_ot_owner_handle(ag_ot_owner_t *,uint64_t lease,const uint8_t *,size_t);
/* Owner must poll even without incoming packets. Poll never refreshes idle. */
ag_ot_response_t ag_ot_owner_poll(ag_ot_owner_t *,uint64_t lease);
#endif
