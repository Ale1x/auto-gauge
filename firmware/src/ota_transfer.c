#include "ota_transfer.h"
#include <string.h>

static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static bool request_valid(const ag_ot_request_t *r){
    if(r==NULL||r->transfer_id==0||(r->body_length&&r->body==NULL))return false;
    switch(r->opcode){
        case AG_OT_START:return r->body_length==0&&r->offset>0&&r->offset<=AG_OTA_TRANSFER_MANIFEST_MAX;
        case AG_OT_MANIFEST:return r->body_length>0&&r->body_length<=AG_OTA_TRANSFER_CHUNK_MAX&&r->offset<=AG_OTA_TRANSFER_MANIFEST_MAX&&r->body_length<=AG_OTA_TRANSFER_MANIFEST_MAX-r->offset;
        case AG_OT_IMAGE:return r->body_length>0&&r->body_length<=AG_OTA_TRANSFER_CHUNK_MAX&&r->offset<=AG_OTA_TRANSFER_IMAGE_MAX&&r->body_length<=AG_OTA_TRANSFER_IMAGE_MAX-r->offset;
        case AG_OT_BEGIN:case AG_OT_FINISH:case AG_OT_SELECT:case AG_OT_CANCEL:return r->offset==0&&r->body_length==0;
        default:return false;
    }
}
bool ag_ot_request_decode(const uint8_t *p,size_t n,ag_ot_request_t *out){
    if(p==NULL||out==NULL||n<AG_OTA_TRANSFER_REQUEST_HEADER||n>AG_OTA_TRANSFER_REQUEST_MAX||p[0]!=1||p[2]!=0||p[3]!=0)return false;
    ag_ot_request_t r={(ag_ot_opcode_t)p[1],get32(p+4),get32(p+8),p+12,n-12};
    if(!request_valid(&r))return false;
    *out=r;return true;
}
bool ag_ot_request_encode(const ag_ot_request_t *r,uint8_t *out,size_t capacity,size_t *written){
    if(out==NULL||written==NULL||!request_valid(r)||capacity<12+r->body_length)return false;
    out[0]=1;out[1]=(uint8_t)r->opcode;out[2]=0;out[3]=0;put32(out+4,r->transfer_id);put32(out+8,r->offset);
    if(r->body_length)memcpy(out+12,r->body,r->body_length);
    *written=12+r->body_length;return true;
}
static bool response_valid(const ag_ot_response_t *r){return r!=NULL&&(unsigned)r->status<=AG_OT_BUSY&&(unsigned)r->phase<=AG_OT_FAULT&&r->manifest_offset<=AG_OTA_TRANSFER_MANIFEST_MAX&&r->image_offset<=AG_OTA_TRANSFER_IMAGE_MAX;}
bool ag_ot_response_decode(const uint8_t *p,size_t n,ag_ot_response_t *out){
    if(p==NULL||out==NULL||n!=16||p[0]!=1||p[1]>AG_OT_BUSY||p[2]>AG_OT_FAULT||(p[3]&~1u))return false;
    ag_ot_response_t r={(ag_ot_status_t)p[1],(ag_ot_phase_t)p[2],(p[3]&1u)!=0,get32(p+4),get32(p+8),get32(p+12)};
    if(!response_valid(&r))return false;
    *out=r;return true;
}
bool ag_ot_response_encode(const ag_ot_response_t *r,uint8_t *p,size_t n){
    if(p==NULL||n<16||!response_valid(r))return false;
    p[0]=1;p[1]=(uint8_t)r->status;p[2]=(uint8_t)r->phase;p[3]=r->selection_may_have_changed?1:0;
    put32(p+4,r->transfer_id);put32(p+8,r->manifest_offset);put32(p+12,r->image_offset);return true;
}
static ag_ot_response_t reply(const ag_ot_owner_t *o,ag_ot_status_t status){
    ag_ot_response_t r={status,AG_OT_IDLE,false,0,0,0};
    if(o&&o->initialized){r.phase=o->phase;r.selection_may_have_changed=o->selection_may_have_changed;r.transfer_id=o->transfer_id;r.manifest_offset=o->manifest_offset;r.image_offset=o->image_offset;}
    return r;
}
bool ag_ot_owner_init(ag_ot_owner_t *o,const ag_ot_hooks_t *h){
    if(o==NULL||h==NULL||o->initialized||o->busy||h->allowed==NULL||h->clock_ms==NULL||h->begin==NULL||h->write==NULL||h->finish==NULL||h->select==NULL||h->poll==NULL||h->cancel==NULL||h->idle_timeout_ms==0||h->total_timeout_ms==0||h->total_timeout_ms>900000||h->idle_timeout_ms>h->total_timeout_ms)return false;
    memset(o,0,sizeof(*o));o->hooks=*h;o->initialized=true;return true;
}
static bool active(const ag_ot_owner_t *o){return o->phase==AG_OT_MANIFEST_PHASE||o->phase==AG_OT_WRITING||o->phase==AG_OT_READY;}
static void fail(ag_ot_owner_t *o){
    o->phase=AG_OT_FAULT;
    if(o->begin_attempted&&!o->selection_may_have_changed&&!o->cancel_attempted){
        o->cancel_attempted=true;
        if(!o->hooks.cancel(o->hooks.context,o->lease))o->cleanup_unknown=true;
    }
}
/* Authorization may itself take time, so sample the clock afterwards. No
 * deadline is refreshed by this check. last_ms is monotonic observation only. */
static ag_ot_status_t check(ag_ot_owner_t *o){
    bool allowed=o->hooks.allowed(o->hooks.context,o->lease);
    uint64_t now=o->hooks.clock_ms(o->hooks.context);
    if(now<o->last_ms||now-o->started_ms>=o->hooks.total_timeout_ms||now-o->activity_ms>=o->hooks.idle_timeout_ms){fail(o);return AG_OT_EXPIRED;}
    o->last_ms=now;
    if(!allowed){fail(o);return AG_OT_DENIED;}
    return AG_OT_OK;
}
static bool order_valid(const ag_ot_owner_t *o,const ag_ot_request_t *r){
    if(r->opcode==AG_OT_START)return o->phase==AG_OT_IDLE;
    if(!active(o)||r->transfer_id!=o->transfer_id)return false;
    switch(r->opcode){
        case AG_OT_MANIFEST:return o->phase==AG_OT_MANIFEST_PHASE&&r->offset==o->manifest_offset&&r->body_length<=o->manifest_bytes-o->manifest_offset;
        case AG_OT_BEGIN:return o->phase==AG_OT_MANIFEST_PHASE&&o->manifest_offset==o->manifest_bytes;
        case AG_OT_IMAGE:return o->phase==AG_OT_WRITING&&r->offset==o->image_offset&&r->body_length<=o->image_bytes-o->image_offset;
        case AG_OT_FINISH:return o->phase==AG_OT_WRITING&&o->image_offset==o->image_bytes;
        case AG_OT_SELECT:return o->phase==AG_OT_READY;
        case AG_OT_CANCEL:return true;
        default:return false;
    }
}
ag_ot_response_t ag_ot_owner_handle(ag_ot_owner_t *o,uint64_t lease,const uint8_t *p,size_t n){
    if(o==NULL||!o->initialized)return reply(NULL,AG_OT_INVALID);
    if(o->busy)return reply(o,AG_OT_BUSY);
    if(lease==0||(o->phase!=AG_OT_IDLE&&lease!=o->lease))return reply(o,AG_OT_DENIED);
    ag_ot_request_t r;if(!ag_ot_request_decode(p,n,&r)||!order_valid(o,&r))return reply(o,AG_OT_INVALID);
    o->busy=true;ag_ot_status_t status=AG_OT_OK;
    if(r.opcode==AG_OT_START){
        /* No pre-admission denial consumes the one transfer or binds a lease. */
        bool allowed=o->hooks.allowed(o->hooks.context,lease);
        uint64_t now=o->hooks.clock_ms(o->hooks.context);
        if(!allowed){status=AG_OT_DENIED;goto done;}
        o->lease=lease;o->transfer_id=r.transfer_id;o->manifest_bytes=r.offset;
        o->started_ms=now;o->last_ms=now;o->activity_ms=now;o->phase=AG_OT_MANIFEST_PHASE;
        status=check(o);goto done;
    }
    status=check(o);if(status!=AG_OT_OK)goto done;
    bool ok=true;
    switch(r.opcode){
        case AG_OT_MANIFEST:
            memmove(o->manifest+o->manifest_offset,r.body,r.body_length);
            o->manifest_offset+=(uint32_t)r.body_length;
            break;
        case AG_OT_BEGIN:{
            uint32_t bytes=0;o->begin_attempted=true;
            ok=o->hooks.begin(o->hooks.context,lease,o->manifest,o->manifest_bytes,&bytes);
            if(ok&&(bytes==0||bytes>AG_OTA_TRANSFER_IMAGE_MAX||bytes%16!=0))ok=false;
            if(ok){o->image_bytes=bytes;o->phase=AG_OT_WRITING;}
            break;
        }
        case AG_OT_IMAGE:
            ok=o->hooks.write(o->hooks.context,lease,r.offset,r.body,r.body_length);
            if(ok)o->image_offset+=(uint32_t)r.body_length;
            break;
        case AG_OT_FINISH:
            ok=o->hooks.finish(o->hooks.context,lease);
            if(ok){o->phase=AG_OT_READY;}
            break;
        case AG_OT_SELECT:
            o->selection_may_have_changed=true;ok=o->hooks.select(o->hooks.context,lease);
            if(ok){o->phase=AG_OT_SELECTED;}
            break;
        case AG_OT_CANCEL:
            fail(o);ok=!o->cleanup_unknown;
            break;
        default:
            ok=false;
            break;
    }
    /* Also checks after select/cancel: these callbacks can already have changed
     * persistent state before revocation/deadline becomes visible. */
    status=check(o);
    if(status!=AG_OT_OK)goto done;
    if(!ok){fail(o);status=AG_OT_BACKEND_FAILURE;goto done;}
    if(active(o))o->activity_ms=o->last_ms;
 done:
    o->busy=false;return reply(o,status);
}
ag_ot_response_t ag_ot_owner_poll(ag_ot_owner_t *o,uint64_t lease){
    if(o==NULL||!o->initialized)return reply(NULL,AG_OT_INVALID);
    if(o->busy)return reply(o,AG_OT_BUSY);
    if(lease==0||(o->phase!=AG_OT_IDLE&&lease!=o->lease))return reply(o,AG_OT_DENIED);
    if(!active(o))return reply(o,o->phase==AG_OT_IDLE||o->phase==AG_OT_SELECTED?AG_OT_OK:AG_OT_INVALID);
    o->busy=true;ag_ot_status_t status=check(o);
    if(status==AG_OT_OK&&o->begin_attempted){
        bool ok=o->hooks.poll(o->hooks.context,lease);status=check(o);
        if(status==AG_OT_OK&&!ok){fail(o);status=AG_OT_BACKEND_FAILURE;}
    }
    o->busy=false;return reply(o,status);
}
