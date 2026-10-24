#include "endpoint_bridge.h"
#include "esp_timer.h"
#include <string.h>
static uint64_t clock_ms(void){return (uint64_t)esp_timer_get_time()/1000u;}
static bool mapped(ag_nimble_bridge_t *b,uint64_t generation){return b->initialized && b->has_lease && b->generation==generation;}
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
static bool advertise(void *p){ag_nimble_bridge_t *b=p;return b->initialized && b->platform.awake_allowed(b->platform.context);}
static void fault(void *p,ag_nimble_fault_t reason){ag_nimble_bridge_t *b=p;b->platform.fault(b->platform.context,reason);}
static bool connect(void *p,uint16_t handle,uint64_t generation,const uint8_t client[16]){
    (void)handle;ag_nimble_bridge_t *b=p;
    if(!b->initialized || b->has_lease || generation<=b->generation)return false;
    b->generation=generation;b->displayed=b->awaiting=false;
    ag_client_id_t id;memcpy(id.bytes,client,16);
    ag_admission_result_t result=ag_admission_connect(&b->admission,&id,clock_ms(),&b->lease);
    b->has_lease=result==AG_ADMISSION_OK;return b->has_lease;
}
static void disconnected(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation))return;
    uint64_t now=clock_ms();
    if(b->endpoint.attached)(void)ag_device_endpoint_disconnect(&b->endpoint,&b->lease,now);
    else (void)ag_admission_disconnect(&b->admission,&b->lease,now);
    b->has_lease=b->displayed=b->awaiting=false;
    memset(b->packet,0,sizeof(b->packet));
}
static bool passkey(void *p,uint64_t generation,char display[7]){
    ag_nimble_bridge_t *b=p;
    if(!mapped(b,generation) || ag_admission_passkey(&b->admission,&b->lease,clock_ms(),display)!=AG_ADMISSION_OK)return false;
    b->displayed=b->platform.display_passkey(b->platform.context,display);return b->displayed;
}
static void clear_display(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(b->generation!=generation)return;
    b->displayed=false;b->platform.clear_passkey(b->platform.context);
}
static bool security_changed(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation))return false;
    uint64_t now=clock_ms();
    if(b->connection->state==AG_CONNECTION_VERIFIED){
        if(!ag_admission_private_allowed(&b->admission,&b->lease,now))return false;
    }else if(ag_admission_security_changed(&b->admission,&b->lease,now)!=AG_ADMISSION_OK)return false;
    return b->endpoint.attached || ag_device_endpoint_attach(&b->endpoint,&b->lease,clock_ms())==AG_DEVICE_OK;
}
static bool authorize(void *p,uint64_t generation){
    ag_nimble_bridge_t *b=p;return mapped(b,generation) && b->platform.awake_allowed(b->platform.context) && ag_admission_private_allowed(&b->admission,&b->lease,clock_ms());
}
static bool receive(void *p,uint64_t generation,const uint8_t *bytes,size_t length,uint64_t now){
    (void)now;ag_nimble_bridge_t *b=p;
    return mapped(b,generation) && ag_device_endpoint_feed(&b->endpoint,&b->lease,bytes,length,clock_ms())==AG_DEVICE_OK;
}
static void transmitted(void *p,uint64_t generation,bool confirmed){
    ag_nimble_bridge_t *b=p;if(!mapped(b,generation))return;
    b->awaiting=false;memset(b->packet,0,sizeof(b->packet));
    if(!confirmed)disconnected(b,generation);
}
static bool tick(void *p,uint64_t generation,uint64_t now){
    (void)now;ag_nimble_bridge_t *b=p;
    if(!mapped(b,generation) || !b->platform.awake_allowed(b->platform.context) || ag_admission_tick(&b->admission,clock_ms())!=AG_ADMISSION_OK)return false;
    if(b->connection->state==AG_CONNECTION_UNVERIFIED)return b->admission.link_open;
    if(!authorize(b,generation))return false;
    if(b->awaiting)return true;
    ag_device_result_t result=ag_device_endpoint_step(&b->endpoint,clock_ms());
    if(result!=AG_DEVICE_OK && result!=AG_DEVICE_BUSY && result!=AG_DEVICE_NO_OUTPUT)return false;
    size_t written=0;
    /* Refresh after synchronous IO; never reuse a pre-commit timestamp. */
    result=ag_device_endpoint_take_output(&b->endpoint,&b->lease,clock_ms(),b->packet,sizeof(b->packet),&written);
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
        ||platform->awake_allowed==NULL||platform->random_u32==NULL||platform->display_passkey==NULL
        ||platform->clear_passkey==NULL||platform->fault==NULL)return false;
    const ag_nimble_platform_t platform_copy=*platform;
    const ag_device_endpoint_options_t options_copy=*options;
    const ag_admission_config_t config_copy=*config;
    memset(b,0,sizeof(*b));b->connection=connection;b->platform=platform_copy;
    ag_admission_hooks_t hooks={.context=b,.random_u32=random_word,.lookup=lookup,.read_security=security,.confirm_bond=bond};
    if(!ag_admission_init(&b->admission,connection,&config_copy,&hooks))return false;
    if(!ag_device_endpoint_init(&b->endpoint,&b->admission,store,&options_copy)){
        connection->verifier=NULL;connection->verifier_context=NULL;memset(b,0,sizeof(*b));return false;
    }
    b->initialized=true;return true;
}
esp_err_t ag_nimble_bridge_start(ag_nimble_bridge_t *b){
    if(b==NULL || !b->initialized)return ESP_ERR_INVALID_ARG;
    ag_nimble_callbacks_t hooks={.context=b,.fault=fault,.advertise_allowed=advertise,.connect=connect,
        .disconnect=disconnected,.passkey_display=passkey,.clear_display=clear_display,.security_changed=security_changed,
        .authorize_private=authorize,.receive=receive,.tick=tick,.transmitted=transmitted};
    return ag_nimble_transport_start(&hooks);
}
ag_admission_result_t ag_nimble_bridge_pairing_window(ag_nimble_bridge_t *b,bool local,uint64_t now){
    if(b==NULL || !b->initialized)return AG_ADMISSION_INVALID;
    return ag_admission_open_window(&b->admission,local,now);
}
