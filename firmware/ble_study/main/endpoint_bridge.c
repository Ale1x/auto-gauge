#include "endpoint_bridge.h"
#include "esp_timer.h"
#include <string.h>
static uint64_t clock_ms(void){return (uint64_t)esp_timer_get_time()/1000u;}
static ag_admission_t *admission(ag_nimble_bridge_t *b){return b->admission_ref?b->admission_ref:&b->admission;}
static ag_device_endpoint_t *endpoint(ag_nimble_bridge_t *b){return b->endpoint_ref?b->endpoint_ref:&b->endpoint;}
static bool overlaps(const void *p,size_t n,const void *q,size_t m){
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
    return a>UINTPTR_MAX-n || b>UINTPTR_MAX-m || (a<b+m && b<a+n);
}
static bool mapped(ag_nimble_bridge_t *b,uint64_t generation){return b->initialized && b->has_lease && b->generation==generation;}
static bool ota_allowed(void *p,uint64_t lease){
    ag_nimble_bridge_t *b=p;
    return b->initialized && b->has_lease && b->ota_active && b->lease.counter==lease
        && b->platform.awake_allowed(b->platform.context)
        && b->platform.route_allowed(b->platform.context,true)
        && ag_admission_private_allowed(admission(b),&b->lease,clock_ms())
        && b->ota_options.allowed(b->ota_options.context,&b->lease);
}
static uint64_t ota_clock(void *p){(void)p;return clock_ms();}
static void ota_close(ag_nimble_bridge_t *b){
    if(!b->ota_active || b->ota_close_reported)return;
    if(b->ota_endpoint.initialized)(void)ag_ot_endpoint_close(&b->ota_endpoint,b->lease.counter);
    b->ota_close_reported=true;
    b->ota_options.closed(b->ota_options.context,b->ota_owner->phase,
        b->ota_owner->cleanup_unknown,b->ota_owner->selection_may_have_changed);
}
static bool random_word(void *p,uint32_t *out){ag_nimble_bridge_t *b=p;return b->platform.random_u32(b->platform.context,out);}
static ag_bond_lookup_t lookup(void *p,const ag_client_id_t *client){
    ag_nimble_bridge_t *b=p;
    if(!b->has_lease || memcmp(client->bytes,b->connection->client.bytes,16)==0){
        /* During admission.connect the connection has not copied client yet.
         * The SDK generation/handle mapping, not wire identity, is authoritative. */
        return ag_nimble_lookup_bond(b->generation);
    }
    return AG_BOND_LOOKUP_FAILED;
}
static bool security(void *p,const ag_client_id_t *client,const ag_connection_lease_t *lease,ag_link_security_t *out){
    ag_nimble_bridge_t *b=p;
    if(!b->has_lease || memcmp(client->bytes,b->connection->client.bytes,16)!=0
        ||lease->counter!=b->lease.counter || memcmp(lease->boot_id,b->lease.boot_id,16)!=0)return false;
    if(!ag_nimble_security(b->generation,out))return false;
    out->current_display_passkey_used=b->displayed;return true;
}
static bool bond(void *p,const ag_client_id_t *client,const ag_connection_lease_t *lease){
    ag_link_security_t out;
    return security(p,client,lease,&out) && out.bonded;
}
static bool route_ready(ag_nimble_bridge_t *b){
    ag_device_endpoint_t *e=endpoint(b);
    return e->initialized && e->admission==admission(b) && b->platform.route_allowed(b->platform.context,b->ota_armed);
}
static bool advertise(void *p){ag_nimble_bridge_t *b=p;return b->initialized && route_ready(b) && b->platform.awake_allowed(b->platform.context);}
static void fault(void *p,ag_nimble_fault_t reason){ag_nimble_bridge_t *b=p;b->platform.fault(b->platform.context,reason);}
static bool owner_tick(void *p){
    ag_nimble_bridge_t *b=p;
    return b->initialized && (b->platform.owner_tick==NULL || b->platform.owner_tick(b->platform.context));
}
static bool connect(void *p,uint16_t handle,uint64_t generation,const uint8_t client[16]){
    (void)handle;ag_nimble_bridge_t *b=p;
    if(!b->initialized || !route_ready(b) || b->has_lease || generation<=b->generation)return false;
    b->generation=generation;b->displayed=b->awaiting=false;
    b->ota_active=b->ota_armed;b->ota_close_reported=false;
    ag_client_id_t id;memcpy(id.bytes,client,16);
    ag_admission_result_t result=ag_admission_connect(admission(b),&id,clock_ms(),&b->lease);
    b->has_lease=result==AG_ADMISSION_OK;return b->has_lease;
}
static void disconnected(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation))return;
    ota_close(b);
    uint64_t now=clock_ms();
    if(endpoint(b)->attached)(void)ag_device_endpoint_disconnect(endpoint(b),&b->lease,now);
    else (void)ag_admission_disconnect(admission(b),&b->lease,now);
    b->has_lease=b->displayed=b->awaiting=false;b->ota_active=false;
    memset(b->packet,0,sizeof(b->packet));
}
static bool passkey(void *p,uint64_t generation,char display[7]){
    ag_nimble_bridge_t *b=p;
    if(!mapped(b,generation) || ag_admission_passkey(admission(b),&b->lease,clock_ms(),display)!=AG_ADMISSION_OK)return false;
    b->displayed=b->platform.display_passkey(b->platform.context,display);return b->displayed;
}
static void clear_display(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(b->generation!=generation)return;
    b->displayed=false;b->platform.clear_passkey(b->platform.context);
}
static bool security_changed(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation) || !b->platform.route_allowed(b->platform.context,b->ota_active))return false;
    uint64_t now=clock_ms();
    if(b->connection->state==AG_CONNECTION_VERIFIED){
        if(!ag_admission_private_allowed(admission(b),&b->lease,now))return false;
    }else if(ag_admission_security_changed(admission(b),&b->lease,now)!=AG_ADMISSION_OK)return false;
    if(b->ota_active){
        if(!ota_allowed(b,b->lease.counter))return false;
        if(b->ota_endpoint.initialized)return !b->ota_endpoint.closed;
        ag_ot_ep_hooks_t hooks={.context=b,.allowed=ota_allowed,.clock_ms=ota_clock,
            .frame_timeout_ms=b->ota_options.frame_timeout_ms,.request_timeout_ms=b->ota_options.request_timeout_ms};
        return ag_ot_endpoint_init(&b->ota_endpoint,b->ota_owner,b->lease.counter,&hooks);
    }
    return endpoint(b)->attached || ag_device_endpoint_attach(endpoint(b),&b->lease,clock_ms())==AG_DEVICE_OK;
}
static bool authorize(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;return mapped(b,generation) && b->platform.awake_allowed(b->platform.context)
        && b->platform.route_allowed(b->platform.context,b->ota_active)
        && ag_admission_private_allowed(admission(b),&b->lease,clock_ms())
        && (!b->ota_active || ota_allowed(b,b->lease.counter));
}
static bool receive(void *p,uint64_t generation,const uint8_t *bytes,size_t length,uint64_t now){
    (void)now;ag_nimble_bridge_t *b=p;
    if(!authorize(b,generation))return false;
    if(b->ota_active)return ag_ot_endpoint_feed(&b->ota_endpoint,b->lease.counter,bytes,length)==AG_OT_EP_OK;
    return ag_device_endpoint_feed(endpoint(b),&b->lease,bytes,length,clock_ms())==AG_DEVICE_OK;
}
static void transmitted(void *p,uint64_t generation,bool confirmed){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation))return;
    b->awaiting=false;memset(b->packet,0,sizeof(b->packet));
    if(!confirmed)disconnected(b,generation);
}
static bool tick(void *p,uint64_t generation,uint64_t now){
    (void)now;ag_nimble_bridge_t *b=p;
    if(!mapped(b,generation) || !b->platform.awake_allowed(b->platform.context) || ag_admission_tick(admission(b),clock_ms())!=AG_ADMISSION_OK)return false;
    if(b->connection->state==AG_CONNECTION_UNVERIFIED)return admission(b)->link_open;
    if(!authorize(b,generation))return false;
    if(b->ota_active){
        /* Poll deadlines and authorization even while an indication awaits
         * confirmation. No next request is dispatched until final EDONE. */
        if(ag_ot_endpoint_poll(&b->ota_endpoint,b->lease.counter)!=AG_OT_EP_OK)return false;
        if(b->awaiting)return true;
        ag_ot_ep_result_t result=ag_ot_endpoint_step(&b->ota_endpoint,b->lease.counter);
        if(result!=AG_OT_EP_OK && result!=AG_OT_EP_BUSY && result!=AG_OT_EP_NO_OUTPUT)return false;
        size_t written=0;
        result=ag_ot_endpoint_take_output(&b->ota_endpoint,b->lease.counter,b->packet,sizeof(b->packet),&written);
        if(result==AG_OT_EP_NO_OUTPUT)return true;
        if(result!=AG_OT_EP_OK)return false;
        b->awaiting=true;
        if(!ag_nimble_send(generation,b->packet,written)){b->awaiting=false;return false;}
        return true;
    }
    if(b->awaiting)return true;
    ag_device_result_t result=ag_device_endpoint_step(endpoint(b),clock_ms());
    if(result!=AG_DEVICE_OK && result!=AG_DEVICE_BUSY && result!=AG_DEVICE_NO_OUTPUT)return false;
    size_t written=0;
    /* Refresh after synchronous IO; never reuse a pre-commit timestamp. */
    result=ag_device_endpoint_take_output(endpoint(b),&b->lease,clock_ms(),b->packet,sizeof(b->packet),&written);
    if(result==AG_DEVICE_NO_OUTPUT)return true;
    if(result!=AG_DEVICE_OK)return false;
    b->awaiting=true; /* Set before SDK call in case a callback is immediate. */
    if(!ag_nimble_send(generation,b->packet,written)){b->awaiting=false;return false;}
    return true;
}
bool ag_nimble_bridge_init(ag_nimble_bridge_t *b,ag_connection_t *connection,ag_config_store_t *store,
    const ag_device_endpoint_options_t *options,const ag_admission_config_t *config,const ag_nimble_platform_t *platform){
    if(b==NULL || connection==NULL || store==NULL || options==NULL || config==NULL || platform==NULL
        || !connection->initialized || !connection->boot_journal_loaded || connection->maintenance_blocked
        || connection->state!=AG_CONNECTION_IDLE || connection->verifier!=NULL || !store->initialized
        ||platform->awake_allowed==NULL||platform->route_allowed==NULL||platform->random_u32==NULL||platform->display_passkey==NULL
        ||platform->clear_passkey==NULL||platform->fault==NULL)return false;
    const ag_nimble_platform_t platform_copy=*platform;
    const ag_device_endpoint_options_t options_copy=*options;
    const ag_admission_config_t config_copy=*config;
    memset(b,0,sizeof(*b));b->connection=connection;b->platform=platform_copy;
    ag_admission_hooks_t hooks={.context=b,.random_u32=random_word,.lookup=lookup,.read_security=security,.confirm_bond=bond};
    if(!ag_admission_init(admission(b),connection,&config_copy,&hooks))return false;
    if(!ag_device_endpoint_init(endpoint(b),admission(b),store,&options_copy)){
        connection->verifier=NULL;connection->verifier_context=NULL;memset(b,0,sizeof(*b));return false;
    }
    b->initialized=true;return true;
}
bool ag_nimble_bridge_init_bound(ag_nimble_bridge_t *b,ag_connection_t *connection,ag_admission_t *a,
    ag_device_endpoint_t *e,const ag_nimble_platform_t *platform){
    if(b==NULL || connection==NULL || a==NULL || e==NULL || platform==NULL
        || overlaps(b,sizeof(*b),a,sizeof(*a)) || overlaps(b,sizeof(*b),e,sizeof(*e))
        || overlaps(b,sizeof(*b),connection,sizeof(*connection))
        || !connection->initialized || !connection->boot_journal_loaded || connection->maintenance_blocked
        || connection->state!=AG_CONNECTION_IDLE || connection->verifier==NULL || connection->verifier_context!=a
        || !a->initialized || a->connection!=connection || a->link_open || a->window_open || a->verification_in_progress
        || (e->initialized && e->admission!=a) || e->attached || e->has_lease || e->stepping
        || (!e->initialized && (e->admission!=NULL || e->configuration!=NULL))
        || platform->awake_allowed==NULL || platform->route_allowed==NULL || platform->random_u32==NULL
        || platform->display_passkey==NULL || platform->clear_passkey==NULL || platform->fault==NULL)return false;
    const ag_nimble_platform_t platform_copy=*platform;
    memset(b,0,sizeof(*b));b->connection=connection;b->platform=platform_copy;
    b->admission_ref=a;b->endpoint_ref=e;
    a->hooks=(ag_admission_hooks_t){.context=b,.random_u32=random_word,.lookup=lookup,
        .read_security=security,.confirm_bond=bond};
    b->initialized=true;return true;
}
bool ag_nimble_bridge_prepare_ota(ag_nimble_bridge_t *b,ag_ot_owner_t *owner,const ag_nimble_ota_options_t *options){
    if(b==NULL || !b->initialized || b->has_lease || b->connection->state!=AG_CONNECTION_IDLE
        || endpoint(b)->attached || b->ota_endpoint.initialized || owner==NULL || !owner->initialized
        || owner->busy || owner->phase!=AG_OT_IDLE || options==NULL || options->allowed==NULL || options->closed==NULL
        || options->frame_timeout_ms==0 || options->request_timeout_ms==0
        || options->frame_timeout_ms>900000u || options->request_timeout_ms>900000u)return false;
    b->ota_owner=owner;b->ota_options=*options;b->ota_armed=true;return true;
}
bool ag_nimble_bridge_prepare_rpc(ag_nimble_bridge_t *b){
    if(b==NULL || !b->initialized || b->has_lease || b->connection->state!=AG_CONNECTION_IDLE
        || endpoint(b)->attached || (b->ota_owner!=NULL && (b->ota_owner->busy
            || b->ota_owner->cleanup_unknown || b->ota_owner->selection_may_have_changed)))return false;
    b->ota_armed=false;return true;
}
esp_err_t ag_nimble_bridge_start(ag_nimble_bridge_t *b){
    if(b==NULL || !b->initialized)return ESP_ERR_INVALID_ARG;
    ag_nimble_callbacks_t hooks={.context=b,.fault=fault,.advertise_allowed=advertise,.connect=connect,
        .disconnect=disconnected,.passkey_display=passkey,.clear_display=clear_display,.security_changed=security_changed,
        .authorize_private=authorize,.receive=receive,.tick=tick,.transmitted=transmitted,.owner_tick=owner_tick};
    return ag_nimble_transport_start(&hooks);
}
ag_admission_result_t ag_nimble_bridge_pairing_window(ag_nimble_bridge_t *b,bool local,uint64_t now){
    if(b==NULL || !b->initialized)return AG_ADMISSION_INVALID;
    return ag_admission_open_window(admission(b),local,now);
}
