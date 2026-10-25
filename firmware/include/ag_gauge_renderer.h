#ifndef AG_GAUGE_RENDERER_H
#define AG_GAUGE_RENDERER_H
#include "ag_gauge_ui.h"
#define AG_GAUGE_RENDER_MAX_SIDE 466u
#define AG_GAUGE_RENDER_ROWS_PER_STEP_MAX 16u
/* Synchronous row sink. Consume/copy words before returning. CPU RGB565 words:
 * adapter owns bus byte order, panel offset/protocol and electrical policy.
 * A false return may mean partial hardware output: no automatic replay. */
typedef struct {
    void *context;
    bool (*write_row)(void *,uint16_t y,const uint16_t *rgb565,size_t width);
    uint16_t width,height;
} ag_gauge_renderer_options_t;
typedef struct {
    ag_gauge_scene_t scene;
    ag_gauge_renderer_options_t options;
    uint16_t row[AG_GAUGE_RENDER_MAX_SIDE];
    char labels[AG_MAX_WIDGETS][8],values[AG_MAX_WIDGETS][16],units[AG_MAX_WIDGETS][8];
    uint16_t next_y;
    bool initialized,active,busy,faulted;
} ag_gauge_renderer_t;
typedef enum {AG_RENDER_OK=0,AG_RENDER_DONE,AG_RENDER_INVALID,AG_RENDER_BUSY,AG_RENDER_FAULT} ag_gauge_render_result_t;
/* One serialized owner. Objects/scene/sink must not overlap renderer; no hook
 * reentry. Init only fresh/quiesced storage, never a blind fault recovery. */
bool ag_gauge_renderer_init(ag_gauge_renderer_t *,const ag_gauge_renderer_options_t *);
/* Validates/snapshots complete scene before any output. If busy it leaves the
 * in-flight snapshot intact; caller may submit latest scene after DONE. */
ag_gauge_render_result_t ag_gauge_renderer_begin(ag_gauge_renderer_t *,const ag_gauge_scene_t *);
/* Local owner may discard an unfinished snapshot before submitting a safe
 * scene. No IO/rollback: already emitted rows remain until rewritten or the
 * physical power/display adapter blanks them. Never clears a fault latch. */
ag_gauge_render_result_t ag_gauge_renderer_cancel_pending(ag_gauge_renderer_t *);
/* At most1..16 rows/call, bounded466pixels scratch. Full row clears old pixels;
 * outside inscribed circle is black. No heap, DMA, panel/touch/brightness IO. */
ag_gauge_render_result_t ag_gauge_renderer_step(ag_gauge_renderer_t *,size_t row_budget);
#endif
