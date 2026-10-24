#include "ag_device_endpoint.h"
#include <stdio.h>
#include <string.h>
static bool same_lease(const ag_connection_lease_t *a,const ag_connection_lease_t *b){
    return a!=NULL && b!=NULL && a->counter!=0 && a->counter==b->counter && memcmp(a->boot_id,b->boot_id,16)==0;
}
static void clear_work(ag_device_endpoint_t *e){
    e->output_length=0;e->phase=AG_DEVICE_IDLE;e->pending_id=0;memset(&e->pending,0,sizeof(e->pending));
    (void)ag_stream_reset(&e->stream);
}
static void detach(ag_device_endpoint_t *e){e->attached=false;clear_work(e);}
static bool admission_lease_current(const ag_device_endpoint_t *e,const ag_connection_lease_t *lease){
    const ag_connection_t *current=e->admission->connection;
    return e->admission->link_open && current!=NULL && current->initialized && current->state!=AG_CONNECTION_IDLE
        && same_lease(lease,&e->admission->lease) && lease->counter==current->lease_counter
        && memcmp(lease->boot_id,current->boot_id,16)==0;
}
static bool authorized(ag_device_endpoint_t *e,uint64_t now){
    if(!e->attached || !admission_lease_current(e,&e->lease)
        || !ag_admission_private_allowed(e->admission,&e->lease,now)){detach(e);return false;}
    return true;
}
static bool ascii_id(const char *s){
    if(s==NULL)return false;
    for(size_t n=0;n<=128;++n){unsigned c=(unsigned char)s[n];if(c==0)return n>0;if(c<32 || c>126)return false;}
    return false;
}
bool ag_device_endpoint_init(ag_device_endpoint_t *e,ag_admission_t *admission,ag_config_store_t *store,
                             const ag_device_endpoint_options_t *options){
    if(e==NULL || admission==NULL || store==NULL || options==NULL || !admission->initialized || !store->initialized
        || options->history_read==NULL || options->frame_timeout_ms==0 || options->request_timeout_ms==0 || !ascii_id(options->device_id)
        || (options->config_persistence!=AG_RPC_VOLATILE && options->config_persistence!=AG_RPC_DURABLE))return false;
    ag_device_endpoint_options_t copy=*options;memset(e,0,sizeof(*e));e->admission=admission;e->configuration=store;e->options=copy;
    e->initialized=ag_stream_init(&e->stream,copy.frame_timeout_ms);return e->initialized;
}
ag_device_result_t ag_device_endpoint_attach(ag_device_endpoint_t *e,const ag_connection_lease_t *lease,uint64_t now){
    if(e==NULL || !e->initialized || lease==NULL)return AG_DEVICE_INVALID;
    /* Reject an old callback BEFORE admission's clock-observing authorization:
     * a stale timestamp/lease must never advance or regress the new peer clock. */
    if(!admission_lease_current(e,lease))return AG_DEVICE_STALE;
    if(e->has_lease && same_lease(&e->lease,lease))return AG_DEVICE_REPLAY;
    if(!ag_admission_private_allowed(e->admission,lease,now))return AG_DEVICE_AUTH_REQUIRED;
    clear_work(e);e->lease=*lease;e->last_request_id=0;e->now_ms=now;e->attached=e->has_lease=true;
    if(!ag_stream_init(&e->stream,e->options.frame_timeout_ms)){detach(e);return AG_DEVICE_INVALID;}return AG_DEVICE_OK;
}
ag_device_result_t ag_device_endpoint_disconnect(ag_device_endpoint_t *e,const ag_connection_lease_t *lease,uint64_t now){
    if(e==NULL || !e->initialized)return AG_DEVICE_INVALID;
    if(!e->attached || !same_lease(&e->lease,lease))return AG_DEVICE_STALE;
    detach(e);(void)ag_admission_disconnect(e->admission,lease,now);return AG_DEVICE_OK;
}
static bool timely(ag_device_endpoint_t *e,uint64_t now){
    if(ag_stream_tick(&e->stream,now)!=AG_STREAM_OK
        || (e->phase!=AG_DEVICE_IDLE && now-e->request_started_ms>=e->options.request_timeout_ms)){
        (void)ag_device_endpoint_disconnect(e,&e->lease,now);return false;
    }
    return true;
}
static ag_device_result_t queue_payload(ag_device_endpoint_t *e,const uint8_t *payload,size_t length){
    if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
    size_t written=0;
    if(e->output_length!=0 || ag_frame_encode(2,e->pending_id,payload,length,e->output,sizeof(e->output),&written)!=AG_FRAME_OK){detach(e);return AG_DEVICE_BACKEND_ERROR;}
    e->output_length=written;return AG_DEVICE_OK;
}
static ag_device_result_t queue_reply(ag_device_endpoint_t *e,ag_rpc_reply_t *reply){
    uint8_t payload[AG_RPC_MAX_PAYLOAD+1];size_t length=0;reply->simulation=e->options.simulation;
    if(ag_rpc_reply_encode(reply,payload,sizeof(payload),&length)!=AG_RPC_OK){detach(e);return AG_DEVICE_BACKEND_ERROR;}
    return queue_payload(e,payload,length);
}
static ag_device_result_t reject(ag_device_endpoint_t *e,ag_rpc_wire_error_t error){
    ag_rpc_reply_t reply={.kind=AG_RPC_REPLY_REJECTED,.error=error};e->phase=AG_DEVICE_FINAL;return queue_reply(e,&reply);
}
static ag_device_result_t special(ag_device_endpoint_t *e,const char *phase,const char *error,const char *state){
    char payload[256];int n=snprintf(payload,sizeof(payload),"{\"simulation\":%s,\"phase\":\"%s\",\"error\":\"%s\",\"store_state\":\"%s\"}",
        e->options.simulation?"true":"false",phase,error,state);
    if(n<0 || (size_t)n>=sizeof(payload)){detach(e);return AG_DEVICE_BACKEND_ERROR;}
    e->phase=AG_DEVICE_FINAL;return queue_payload(e,(const uint8_t *)payload,(size_t)n);
}
static const char *store_state(ag_config_store_result_t r){
    switch(r){case AG_CONFIG_STORE_READ_ONLY:return "read_only";case AG_CONFIG_STORE_CAPABILITY_MISMATCH:return "capability_mismatch";
    case AG_CONFIG_STORE_NO_VALID_CONFIG:return "no_valid_configuration";case AG_CONFIG_STORE_CONFLICT:return "conflict";default:return "io_or_integrity_error";}
}
static bool on_frame(void *v,const ag_frame_view_t *frame){
    ag_device_endpoint_t *e=v;
    if(e->phase!=AG_DEVICE_IDLE || e->output_length!=0){e->handler_result=AG_DEVICE_BUSY;return false;}
    if(!authorized(e,e->now_ms)){e->handler_result=AG_DEVICE_AUTH_REQUIRED;return false;}
    if(frame->kind!=1){e->handler_result=AG_DEVICE_PROTOCOL_ERROR;return false;}
    if(frame->request_id<=e->last_request_id){e->handler_result=AG_DEVICE_REPLAY;return false;}
    e->last_request_id=e->pending_id=frame->request_id;
    e->request_started_ms=e->stream.started_ms;
    ag_rpc_error_t detail;
    ag_rpc_result_t parsed=ag_rpc_request_parse(frame->payload,frame->payload_length,&e->options.supported,&e->pending,&detail);
    if(parsed!=AG_RPC_OK){
        e->handler_result=reject(e,parsed==AG_RPC_CONFIG?AG_RPC_ERROR_INVALID_CONFIGURATION:AG_RPC_ERROR_INVALID_REQUEST);
        return e->handler_result==AG_DEVICE_OK;
    }
    e->phase=AG_DEVICE_PENDING;e->handler_result=AG_DEVICE_OK;return true;
}
ag_device_result_t ag_device_endpoint_feed(ag_device_endpoint_t *e,const ag_connection_lease_t *lease,
                                           const uint8_t *bytes,size_t length,uint64_t now){
    if(e==NULL || !e->initialized)return AG_DEVICE_INVALID;
    if(!e->attached || !same_lease(&e->lease,lease))return AG_DEVICE_STALE;
    if(!authorized(e,now))return AG_DEVICE_AUTH_REQUIRED;
    if(!timely(e,now))return AG_DEVICE_PROTOCOL_ERROR;
    if(e->phase!=AG_DEVICE_IDLE || e->output_length!=0)return AG_DEVICE_BUSY;
    e->now_ms=now;e->handler_result=AG_DEVICE_OK;size_t accepted=0;
    ag_stream_result_t result=ag_stream_feed(&e->stream,bytes,length,now,on_frame,e,&accepted);
    if(result!=AG_STREAM_OK){ag_device_result_t cause=e->handler_result==AG_DEVICE_OK?AG_DEVICE_PROTOCOL_ERROR:e->handler_result;
        (void)ag_device_endpoint_disconnect(e,lease,now);return cause;}
    return AG_DEVICE_OK;
}
static bool exact_readback(ag_config_store_t *s,const ag_config_t *candidate){
    if(ag_config_store_restore(s)!=AG_CONFIG_STORE_OK || !s->ready || !s->writable || !s->compatible)return false;
    uint8_t a[AG_CONFIG_RECORD_MAX],b[AG_CONFIG_RECORD_MAX];size_t na=0,nb=0;
    return ag_config_record_encode(candidate,&s->supported,s->generation,a,sizeof(a),&na)==AG_CONFIG_STORE_OK
        && ag_config_record_encode(&s->active,&s->supported,s->generation,b,sizeof(b),&nb)==AG_CONFIG_STORE_OK
        && na==nb && memcmp(a,b,na)==0;
}
static ag_device_result_t perform(ag_device_endpoint_t *e){
    ag_config_store_t *store=e->configuration;
    if(e->phase==AG_DEVICE_APPLY_COMMIT){
        ag_config_store_result_t restored=ag_config_store_restore(store);
        if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
        if(restored!=AG_CONFIG_STORE_OK && restored!=AG_CONFIG_STORE_CAPABILITY_MISMATCH)
            return special(e,"diagnostic","configuration_not_operational",store_state(restored));
        if(store->active.revision!=e->pending.expected_revision)return reject(e,AG_RPC_ERROR_REVISION_CONFLICT);
        if(e->pending.expected_revision==INT32_MAX || e->pending.configuration.revision!=(int64_t)e->pending.expected_revision+1
            || ag_config_validate(&e->pending.configuration,&e->options.supported)!=AG_CONFIG_OK)return reject(e,AG_RPC_ERROR_INVALID_CONFIGURATION);
        /* No await between final authorization and entering the synchronous commit.
         * An IO hook can model a disconnect during commit; never undo durable data. */
        if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
        ag_config_store_result_t result=ag_config_store_commit(store,(uint32_t)e->pending.expected_revision,&e->pending.configuration);
        if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
        if(result!=AG_CONFIG_STORE_OK || !exact_readback(store,&e->pending.configuration))
            return special(e,"outcome_unknown","outcome_unknown","commit_or_readback_failed");
        if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
        ag_rpc_reply_t reply={.kind=AG_RPC_REPLY_APPLIED,.applied_revision=(int32_t)e->pending.configuration.revision,
            .persistence=e->options.config_persistence};e->phase=AG_DEVICE_FINAL;return queue_reply(e,&reply);
    }
    if(e->phase!=AG_DEVICE_PENDING)return AG_DEVICE_NO_OUTPUT;
    ag_rpc_reply_t reply={0};
    switch(e->pending.operation){
    case AG_RPC_HELLO:
        reply.kind=AG_RPC_REPLY_HELLO;reply.device_id=e->options.device_id;reply.supported=&e->options.supported;reply.authenticated=true;break;
    case AG_RPC_CONFIG_GET:{
        ag_config_store_result_t restored=ag_config_store_restore(store);
        if(restored!=AG_CONFIG_STORE_OK)return special(e,"diagnostic","configuration_not_operational",store_state(restored));
        reply.kind=AG_RPC_REPLY_CURRENT;reply.configuration=&store->active;reply.persistence=e->options.config_persistence;break;}
    case AG_RPC_CONFIG_VALIDATE:reply.kind=AG_RPC_REPLY_VALIDATED;break;
    case AG_RPC_CONFIG_APPLY:reply.kind=AG_RPC_REPLY_RECEIVED;e->phase=AG_DEVICE_APPLY_COMMIT;return queue_reply(e,&reply);
    case AG_RPC_LOGS_READ:case AG_RPC_LOGS_ACK:{
        ag_history_snapshot_t snapshot;
        ag_device_history_result_t result=e->options.history_read(e->options.history_context,e->pending.session_id,e->pending.sequence,&snapshot);
        if(!authorized(e,e->now_ms))return AG_DEVICE_AUTH_REQUIRED;
        if(result==AG_DEVICE_HISTORY_READ_ONLY)return special(e,"diagnostic","history_not_operational","read_only");
        if(result!=AG_DEVICE_HISTORY_OK)return reject(e,result==AG_DEVICE_HISTORY_SESSION_MISMATCH?AG_RPC_ERROR_SESSION_MISMATCH:
            (result==AG_DEVICE_HISTORY_CURSOR_AHEAD?AG_RPC_ERROR_INVALID_HISTORY_REQUEST:AG_RPC_ERROR_HISTORY_UNAVAILABLE));
        if(!ag_history_snapshot_valid(&snapshot) || memcmp(snapshot.session_id,e->pending.session_id,16)!=0)return reject(e,AG_RPC_ERROR_HISTORY_UNAVAILABLE);
        uint64_t latest=snapshot.exhausted?UINT64_MAX:snapshot.next_sequence-1;
        if(e->pending.sequence>latest)return reject(e,AG_RPC_ERROR_INVALID_HISTORY_REQUEST);
        memcpy(reply.session_id,e->pending.session_id,16);
        if(e->pending.operation==AG_RPC_LOGS_ACK){reply.kind=AG_RPC_REPLY_LOGS_ACK;reply.acknowledged_through=e->pending.sequence;}
        else {reply.kind=AG_RPC_REPLY_LOGS_READ;reply.history=&snapshot;reply.after_sequence=e->pending.sequence;reply.limit=e->pending.limit;}
        e->phase=AG_DEVICE_FINAL;return queue_reply(e,&reply);}
    default:return reject(e,AG_RPC_ERROR_INVALID_REQUEST);
    }
    e->phase=AG_DEVICE_FINAL;return queue_reply(e,&reply);
}
ag_device_result_t ag_device_endpoint_step(ag_device_endpoint_t *e,uint64_t now){
    if(e==NULL || !e->initialized)return AG_DEVICE_INVALID;
    if(e->stepping)return AG_DEVICE_BUSY;
    if(!authorized(e,now))return AG_DEVICE_AUTH_REQUIRED;
    if(!timely(e,now))return AG_DEVICE_PROTOCOL_ERROR;
    if(e->output_length!=0)return AG_DEVICE_BUSY;
    e->now_ms=now;e->stepping=true;ag_device_result_t result=perform(e);e->stepping=false;return result;
}
ag_device_result_t ag_device_endpoint_take_output(ag_device_endpoint_t *e,const ag_connection_lease_t *lease,
    uint64_t now,uint8_t *out,size_t capacity,size_t *written){
    if(written!=NULL)*written=0;
    if(e==NULL || !e->initialized || out==NULL || written==NULL)return AG_DEVICE_INVALID;
    if(!e->attached || !same_lease(&e->lease,lease))return AG_DEVICE_STALE;
    if(!authorized(e,now))return AG_DEVICE_AUTH_REQUIRED;
    if(!timely(e,now))return AG_DEVICE_PROTOCOL_ERROR;
    if(e->output_length==0)return AG_DEVICE_NO_OUTPUT;
    if(capacity<e->output_length)return AG_DEVICE_OUTPUT_SHORT;
    memcpy(out,e->output,e->output_length);*written=e->output_length;e->output_length=0;
    if(e->phase==AG_DEVICE_FINAL)e->phase=AG_DEVICE_IDLE;
    return AG_DEVICE_OK;
}
