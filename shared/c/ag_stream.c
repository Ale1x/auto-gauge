#include "ag_stream.h"
#include <string.h>

static void clear_partial(ag_stream_t *s) {
    memset(s->bytes, 0, sizeof(s->bytes));
    s->used=s->expected=0; s->started_ms=0;
}
bool ag_stream_init(ag_stream_t *s, uint32_t timeout) {
    if(s==NULL) return false;
    memset(s,0,sizeof(*s));
    if(timeout==0) return false;
    s->timeout_ms=timeout;s->initialized=true;return true;
}
bool ag_stream_reset(ag_stream_t *s) {
    if(s==NULL || !s->initialized || s->feeding) return false;
    clear_partial(s);return true;
}
static ag_stream_result_t observe(ag_stream_t *s,uint64_t now) {
    if(s->clock_fault) return AG_STREAM_CLOCK_FAULT;
    if(s->has_time && now<s->last_ms) {
        clear_partial(s);s->clock_fault=true;return AG_STREAM_CLOCK_FAULT;
    }
    s->has_time=true;s->last_ms=now;
    if(s->used!=0 && now-s->started_ms>=s->timeout_ms) {
        clear_partial(s);return AG_STREAM_TIMEOUT;
    }
    return AG_STREAM_OK;
}
ag_stream_result_t ag_stream_tick(ag_stream_t *s,uint64_t now) {
    if(s==NULL || !s->initialized) return AG_STREAM_ARGUMENT;
    if(s->feeding) return AG_STREAM_BUSY;
    return observe(s,now);
}
static bool header(ag_stream_t *s) {
    const uint8_t *b=s->bytes;
    const uint32_t id=(uint32_t)b[4]|(uint32_t)b[5]<<8|(uint32_t)b[6]<<16|(uint32_t)b[7]<<24;
    const size_t length=(size_t)b[8]|(size_t)b[9]<<8;
    if(b[0]!='A'||b[1]!='G'||b[2]!=1||b[3]<1||b[3]>3
        || ((id==0)!=(b[3]==3)) || b[10]!=0||b[11]!=0||length>AG_FRAME_MAX_PAYLOAD) return false;
    s->expected=length+16;return true;
}
ag_stream_result_t ag_stream_feed(ag_stream_t *s,const uint8_t *chunk,size_t length,
    uint64_t now,ag_stream_handler_t handler,void *context,size_t *accepted) {
    if(accepted!=NULL) *accepted=0;
    if(s==NULL || !s->initialized) return AG_STREAM_ARGUMENT;
    if(s->feeding) return AG_STREAM_BUSY;
    if(accepted==NULL || handler==NULL || chunk==NULL) {
        clear_partial(s);return AG_STREAM_ARGUMENT;
    }
    ag_stream_result_t result=observe(s,now);
    if(result!=AG_STREAM_OK) return result;
    if(length==0 || length>AG_STREAM_MAX_CHUNK) {
        clear_partial(s);return AG_STREAM_INVALID_CHUNK;
    }
    s->feeding=true;
    size_t position=0;
    while(position<length) {
        if(s->used==0) s->started_ms=now;
        const size_t target=s->expected==0?12:s->expected;
        if(s->used>=target || target>sizeof(s->bytes)) {result=AG_STREAM_INVALID_FRAME;break;}
        size_t count=target-s->used;
        if(count>length-position) count=length-position;
        memcpy(s->bytes+s->used,chunk+position,count);s->used+=count;position+=count;
        if(s->expected==0 && s->used==12 && !header(s)) {result=AG_STREAM_INVALID_FRAME;break;}
        if(s->expected!=0 && s->used==s->expected) {
            ag_frame_view_t frame;
            if(ag_frame_decode(s->bytes,s->used,&frame)!=AG_FRAME_OK) {result=AG_STREAM_INVALID_FRAME;break;}
            if(!handler(context,&frame)) {result=AG_STREAM_HANDLER_REJECTED;break;}
            ++*accepted;clear_partial(s);
        }
    }
    s->feeding=false;
    if(result!=AG_STREAM_OK) clear_partial(s);
    return result;
}
