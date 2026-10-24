#ifndef AG_FRAME_H
#define AG_FRAME_H
#include <stddef.h>
#include <stdint.h>

#define AG_FRAME_MAX_PAYLOAD 4096u
#define AG_FRAME_MAX_SIZE (AG_FRAME_MAX_PAYLOAD + 16u)
typedef enum { AG_FRAME_OK = 0, AG_FRAME_INVALID, AG_FRAME_NO_SPACE } ag_frame_result_t;
/* Borrowed payload view: valid only while the input packet remains unchanged. */
typedef struct { uint8_t kind; uint32_t request_id; const uint8_t *payload; size_t payload_length; } ag_frame_view_t;
ag_frame_result_t ag_frame_decode(const uint8_t *packet, size_t size, ag_frame_view_t *view);
/* Output must not alias the payload. No CRC or frame field authenticates a user. */
ag_frame_result_t ag_frame_encode(uint8_t kind, uint32_t request_id,
    const uint8_t *payload, size_t length, uint8_t *output, size_t capacity, size_t *written);
#endif
