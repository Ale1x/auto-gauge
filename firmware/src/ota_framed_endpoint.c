#include "ota_framed_endpoint.h"
#include <string.h>

static bool owner_active(const ag_ot_owner_t *o){
    return o->phase==AG_OT_MANIFEST_PHASE||o->phase==AG_OT_WRITING||o->phase==AG_OT_READY;
}
static void seal(ag_ot_endpoint_t *e){
    e->closed=true;e->staged=false;e->has_output=false;
    memset(e->staged_payload,0,sizeof(e->staged_payload));memset(e->output,0,sizeof(e->output));
    if(e->close_attempted)return;
    e->close_attempted=true;
    ag_ot_owner_t *o=e->owner;
    if(o->lease==e->lease&&owner_active(o)){
        uint8_t payload[12];size_t written=0;
        ag_ot_request_t cancel={AG_OT_CANCEL,o->transfer_id,0,NULL,0};
        if(ag_ot_request_encode(&cancel,payload,sizeof(payload),&written))
            (void)ag_ot_owner_handle(o,e->lease,payload,written);
    }
}
static ag_ot_ep_result_t enter(ag_ot_endpoint_t *e,uint64_t lease){
    if(e==NULL||!e->initialized)return AG_OT_EP_INVALID;
    if(lease==0||lease!=e->lease)return AG_OT_EP_DENIED;
    if(e->busy)return AG_OT_EP_BUSY;
    if(e->closed)return AG_OT_EP_CLOSED;
    e->busy=true;return AG_OT_EP_OK;
}
static ag_ot_ep_result_t leave(ag_ot_endpoint_t *e,ag_ot_ep_result_t result){e->busy=false;return result;}
static ag_ot_ep_result_t check(ag_ot_endpoint_t *e){
    bool allowed=e->hooks.allowed(e->hooks.context,e->lease);
    uint64_t now=e->hooks.clock_ms(e->hooks.context);
    if((e->has_time&&now<e->last_ms)||((e->staged||e->has_output)&&now-e->pending_ms>=e->hooks.request_timeout_ms)){
        seal(e);return AG_OT_EP_TIMEOUT;
    }
    e->has_time=true;e->last_ms=now;
    if(!allowed){seal(e);return AG_OT_EP_DENIED;}
    ag_stream_result_t stream=ag_stream_tick(&e->stream,now);
    if(stream!=AG_STREAM_OK){seal(e);return stream==AG_STREAM_TIMEOUT||stream==AG_STREAM_CLOCK_FAULT?AG_OT_EP_TIMEOUT:AG_OT_EP_INVALID;}
    return AG_OT_EP_OK;
}
bool ag_ot_endpoint_init(ag_ot_endpoint_t *e,ag_ot_owner_t *owner,uint64_t lease,const ag_ot_ep_hooks_t *hooks){
    if(e==NULL||owner==NULL||hooks==NULL||e->initialized||e->busy||!owner->initialized||owner->busy||lease==0
        ||hooks->allowed==NULL||hooks->clock_ms==NULL||hooks->frame_timeout_ms==0||hooks->request_timeout_ms==0
        ||hooks->frame_timeout_ms>900000u||hooks->request_timeout_ms>900000u)return false;
    ag_ot_ep_hooks_t copied=*hooks;
    memset(e,0,sizeof(*e));e->owner=owner;e->lease=lease;e->hooks=copied;e->next_request_id=1;
    if(!ag_stream_init(&e->stream,copied.frame_timeout_ms))return false;
    e->initialized=true;return true;
}
static bool stage(void *context,const ag_frame_view_t *frame){
    ag_ot_endpoint_t *e=context;
    if(e->staged||frame->kind!=1||e->next_request_id>UINT32_MAX||frame->request_id!=e->next_request_id
        ||frame->payload_length>sizeof(e->staged_payload))return false;
    if(frame->payload_length)memcpy(e->staged_payload,frame->payload,frame->payload_length);
    e->staged_length=frame->payload_length;e->staged_id=frame->request_id;e->pending_ms=e->last_ms;e->staged=true;
    return true;
}
ag_ot_ep_result_t ag_ot_endpoint_feed(ag_ot_endpoint_t *e,uint64_t lease,const uint8_t *bytes,size_t length){
    ag_ot_ep_result_t result=enter(e,lease);if(result!=AG_OT_EP_OK)return result;
    if(e->staged||e->has_output)return leave(e,AG_OT_EP_BUSY);
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    size_t accepted=0;
    ag_stream_result_t stream=ag_stream_feed(&e->stream,bytes,length,e->last_ms,stage,e,&accepted);
    if(stream!=AG_STREAM_OK||accepted>1||e->stream.expected>AG_OTA_TRANSFER_REQUEST_MAX+16u
        ||(e->stream.used>=12&&e->stream.bytes[3]!=1)||(e->staged&&e->stream.used!=0)){
        seal(e);return leave(e,stream==AG_STREAM_TIMEOUT||stream==AG_STREAM_CLOCK_FAULT?AG_OT_EP_TIMEOUT:AG_OT_EP_INVALID);
    }
    result=check(e);return leave(e,result);
}
ag_ot_ep_result_t ag_ot_endpoint_step(ag_ot_endpoint_t *e,uint64_t lease){
    ag_ot_ep_result_t result=enter(e,lease);if(result!=AG_OT_EP_OK)return result;
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    if(e->has_output)return leave(e,AG_OT_EP_BUSY);
    if(!e->staged)return leave(e,AG_OT_EP_NO_OUTPUT);
    /* Consume the sequence before dispatch: a side-effecting failure cannot be
     * replayed with this ID, and no wrap/reset is possible in this endpoint. */
    ++e->next_request_id;
    ag_ot_response_t response=ag_ot_owner_handle(e->owner,lease,e->staged_payload,e->staged_length);
    uint8_t payload[AG_OTA_TRANSFER_RESPONSE_BYTES];
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    size_t written=0;
    if(!ag_ot_response_encode(&response,payload,sizeof(payload))||ag_frame_encode(2,e->staged_id,payload,sizeof(payload),e->output,sizeof(e->output),&written)!=AG_FRAME_OK||written!=sizeof(e->output)){
        seal(e);return leave(e,AG_OT_EP_BACKEND_FAILURE);
    }
    e->staged=false;e->has_output=true;
    memset(e->staged_payload,0,sizeof(e->staged_payload));
    return leave(e,AG_OT_EP_OK);
}
ag_ot_ep_result_t ag_ot_endpoint_take_output(ag_ot_endpoint_t *e,uint64_t lease,uint8_t *output,size_t capacity,size_t *written){
    ag_ot_ep_result_t result=enter(e,lease);if(result!=AG_OT_EP_OK)return result;
    if(written==NULL||output==NULL)return leave(e,AG_OT_EP_INVALID);
    *written=0;
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    if(!e->has_output)return leave(e,AG_OT_EP_NO_OUTPUT);
    if(capacity<sizeof(e->output))return leave(e,AG_OT_EP_OUTPUT_SHORT);
    memcpy(output,e->output,sizeof(e->output));*written=sizeof(e->output);
    e->has_output=false;memset(e->output,0,sizeof(e->output));
    return leave(e,AG_OT_EP_OK);
}
ag_ot_ep_result_t ag_ot_endpoint_poll(ag_ot_endpoint_t *e,uint64_t lease){
    ag_ot_ep_result_t result=enter(e,lease);if(result!=AG_OT_EP_OK)return result;
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    ag_ot_response_t response=ag_ot_owner_poll(e->owner,lease);
    result=check(e);if(result!=AG_OT_EP_OK)return leave(e,result);
    if(response.status!=AG_OT_OK){seal(e);return leave(e,AG_OT_EP_BACKEND_FAILURE);}
    return leave(e,AG_OT_EP_OK);
}
ag_ot_ep_result_t ag_ot_endpoint_close(ag_ot_endpoint_t *e,uint64_t lease){
    ag_ot_ep_result_t result=enter(e,lease);if(result!=AG_OT_EP_OK)return result;
    seal(e);return leave(e,AG_OT_EP_OK);
}
