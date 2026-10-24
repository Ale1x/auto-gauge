#ifndef AG_STREAM_H
#define AG_STREAM_H
#include <stdbool.h>
#include "ag_frame.h"
#define AG_STREAM_MAX_CHUNK 512u

typedef enum {
    AG_STREAM_OK=0, AG_STREAM_ARGUMENT, AG_STREAM_INVALID_CHUNK,
    AG_STREAM_INVALID_FRAME, AG_STREAM_TIMEOUT, AG_STREAM_CLOCK_FAULT,
    AG_STREAM_HANDLER_REJECTED, AG_STREAM_BUSY
} ag_stream_result_t;
typedef struct {
    uint8_t bytes[AG_FRAME_MAX_SIZE];
    size_t used, expected;
    uint64_t started_ms, last_ms;
    uint32_t timeout_ms;
    bool initialized, has_time, clock_fault, feeding;
} ag_stream_t;
/* Borrowed frame view lasts only during callback. Handler must copy anything
 * retained, authorize the current connection itself, and never reenter stream.
 * Input chunk/output count must not alias the stream or each other.
 * Each accepted frame is independent: prior callback side effects are NOT
 * rolled back if a later frame in the same chunk is malformed. */
typedef bool (*ag_stream_handler_t)(void *context, const ag_frame_view_t *frame);
bool ag_stream_init(ag_stream_t *, uint32_t timeout_ms);
/* Drop partial bytes, retaining monotonic clock/fault. Fresh link: init again;
 * the adapter must separately reject old leases/callbacks. Not thread-safe. */
bool ag_stream_reset(ag_stream_t *);
ag_stream_result_t ag_stream_tick(ag_stream_t *, uint64_t now_ms);
/* Chunk1..512, no resync. On error drops partial input. accepted counts already
 * successful callbacks, even when a later frame in the chunk fails. */
ag_stream_result_t ag_stream_feed(ag_stream_t *, const uint8_t *chunk, size_t length,
    uint64_t now_ms, ag_stream_handler_t, void *context, size_t *accepted);
#endif
