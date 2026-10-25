#include "nimble_transport.h"
#include "gatt_profile.h"
#include <string.h>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_hs_id.h"
#include "host/util/util.h"
#include "host/ble_hs_pvcy.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#if CONFIG_BT_NIMBLE_MAX_CONNECTIONS != 1 || CONFIG_BT_NIMBLE_SM_SC_ONLY != 1 || CONFIG_BT_NIMBLE_SM_LVL != 3
#error "Study requires one connection, SC-only and authenticated 128-bit SC security level"
#endif
#if !CONFIG_BT_NIMBLE_NVS_PERSIST
#error "Study bridge requires the SDK persistent bond store"
#endif
#if CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS || CONFIG_BT_NIMBLE_HANDLE_REPEAT_PAIRING_DELETION
#error "Debug keys and automatic repeat-pairing bond deletion are forbidden"
#endif

void ble_store_config_init(void);
static const ble_uuid128_t service_uuid=AG_GATT_SERVICE_UUID;
static const ble_uuid128_t request_uuid=AG_GATT_REQUEST_UUID;
static const ble_uuid128_t response_uuid=AG_GATT_RESPONSE_UUID;
static struct {
    ag_nimble_callbacks_t hooks;
    struct ble_npl_callout timer;
    uint8_t tx[AG_GATT_FRAME_BYTES];
    size_t tx_length,tx_offset,inflight;
    uint64_t generation,connected_ms,tx_started_ms,close_started_ms;
    TaskHandle_t host_task;
    uint8_t close_attempts;
    uint16_t handle,response_handle;
    uint8_t address_type;
    bool started,synced,connected,closing,rejected,subscribed,awaiting,faulted,adv_stop_failed,termination_fault_reported,restart_advertise,terminate_requested,quiescing;
} server;

static uint64_t now_ms(void){return (uint64_t)esp_timer_get_time()/1000u;}
static void wipe(void *p,size_t n){volatile uint8_t *b=p;while(n--)*b++=0;}
static bool current(uint64_t generation){return server.started && server.connected && !server.closing && generation==server.generation;}
static int gap_event(struct ble_gap_event *,void *);

ag_bond_lookup_t ag_nimble_lookup_bond(uint64_t generation){
    if(!current(generation)||server.faulted)return AG_BOND_LOOKUP_FAILED;
    struct ble_gap_conn_desc desc;
    if(ble_gap_conn_find(server.handle,&desc)!=0)return AG_BOND_LOOKUP_FAILED;
    struct ble_store_key_sec key={0};key.peer_addr=desc.peer_id_addr;
    struct ble_store_value_sec value={0};int result=ble_store_read_peer_sec(&key,&value);
    ag_bond_lookup_t answer=AG_BOND_LOOKUP_FAILED;
    if(result==BLE_HS_ENOENT)answer=AG_BOND_UNKNOWN;
    else if(result==0 && value.ltk_present && value.authenticated && value.sc
        &&value.key_size==16 &&ble_addr_cmp(&value.peer_addr,&desc.peer_id_addr)==0)answer=AG_BOND_RECOGNIZED;
    wipe(&value,sizeof(value));return answer;
}
bool ag_nimble_security(uint64_t generation,ag_link_security_t *out){
    if(out==NULL)return false;
    memset(out,0,sizeof(*out));
    if(!current(generation)||server.faulted)return false;
    struct ble_gap_conn_desc desc;
    if(ble_gap_conn_find(server.handle,&desc)!=0)return false;
    struct ble_store_key_sec key={0};key.peer_addr=desc.peer_id_addr;
    struct ble_store_value_sec value={0};
    int result=ble_store_read_peer_sec(&key,&value);
    out->encrypted=desc.sec_state.encrypted;
    out->mitm_authenticated=desc.sec_state.authenticated;
    out->bonded=desc.sec_state.bonded && result==0 && value.ltk_present;
    out->secure_connections=result==0 && value.sc;
    out->key_bits=desc.sec_state.key_size==16 && value.key_size==16?128:0;
    out->identity_resolved=result==0 && ble_addr_cmp(&value.peer_addr,&desc.peer_id_addr)==0;
    /* SMP method/local admission evidence belongs to the caller's per-lease
     * controller; this helper never invents current_display_passkey_used. */
    wipe(&value,sizeof(value));
    return result==0 && out->encrypted && out->mitm_authenticated && out->bonded
        && out->secure_connections && out->identity_resolved && out->key_bits==128;
}
static bool private_allowed(void){
    ag_link_security_t evidence;
    return !server.quiescing && current(server.generation) && ag_nimble_security(server.generation,&evidence)
        && server.hooks.authorize_private(server.hooks.context,server.generation);
}
static void clear_tx(bool notify){
    bool pending=server.tx_length!=0;
    server.tx_length=server.tx_offset=server.inflight=0;server.awaiting=false;
    wipe(server.tx,sizeof(server.tx));
    if(notify && pending)server.hooks.transmitted(server.hooks.context,server.generation,false);
}
static void fault(ag_nimble_fault_t reason){
    server.faulted=true;
    server.hooks.fault(server.hooks.context,reason);
}
static void stop_advertising(void){
    if(!ble_gap_adv_active() || server.adv_stop_failed)return;
    if(ble_gap_adv_stop()!=0){
        server.adv_stop_failed=true;
        if(!server.faulted)fault(AG_NIMBLE_FAULT_START);
        /* Controller may still advertise: explicit owner/reboot handling only.
         * No tight retry loop or assertion that the radio has stopped. */
    }
}
static void finish_disconnect(void){
    clear_tx(false);server.connected=false;server.closing=false;server.rejected=false;
    server.subscribed=false;server.handle=BLE_HS_CONN_HANDLE_NONE;
    server.restart_advertise=!server.faulted;
}
static void try_terminate(void){
    struct ble_gap_conn_desc description;
    int found=ble_gap_conn_find(server.handle,&description);
    if(found==BLE_HS_ENOTCONN){finish_disconnect();return;}
    if(server.terminate_requested){
        if(now_ms()-server.close_started_ms>=AG_GATT_REQUEST_MS && !server.termination_fault_reported){
            server.termination_fault_reported=true;fault(AG_NIMBLE_FAULT_TERMINATE);
        }
        return;
    }
    if(server.close_attempts>=3){
        if(!server.termination_fault_reported){server.termination_fault_reported=true;fault(AG_NIMBLE_FAULT_TERMINATE);}
        return;
    }
    ++server.close_attempts;
    /* A nonzero return never means a live peer is safely gone. Timer retries
     * boundedly; absent descriptor completes cleanup, other errors fail closed. */
    int result=ble_gap_terminate(server.handle,BLE_ERR_REM_USER_CONN_TERM);
    if(result==0)server.terminate_requested=true;
    else if(result==BLE_HS_ENOTCONN && ble_gap_conn_find(server.handle,&description)==BLE_HS_ENOTCONN)finish_disconnect();
}
static void reject_connection(uint16_t handle){
    /* A successful CONNECT owns a controller slot even when admission is denied.
     * Track its teardown without opening a logical client or invoking its hooks.
     * No callback cookie can safely be reused at generation exhaustion: cleanup
     * of rejected links is therefore driven only by the descriptor/timer. */
    if(server.connected){
        fault(AG_NIMBLE_FAULT_TERMINATE); /* Violates configured max_connections=1. */
        ag_nimble_close(server.generation);
        if(handle!=server.handle)(void)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    server.connected=true;server.closing=true;server.rejected=true;server.subscribed=false;
    server.handle=handle;if(server.generation<UINT32_MAX)++server.generation;
    server.close_attempts=0;server.termination_fault_reported=false;
    server.terminate_requested=false;server.close_started_ms=now_ms();
    try_terminate();
}
void ag_nimble_close(uint64_t generation){
    if(!current(generation))return;
    server.closing=true;server.close_attempts=0;server.termination_fault_reported=false;
    server.terminate_requested=false;server.close_started_ms=now_ms();
    clear_tx(true);server.hooks.clear_display(server.hooks.context,generation);
    server.hooks.disconnect(server.hooks.context,generation);
    try_terminate();
}
bool ag_nimble_transport_quiesce(void){
    if(!server.started || !server.synced || server.faulted)return false;
    server.quiescing=true;server.restart_advertise=false;
    stop_advertising();
    if(server.connected && !server.closing)ag_nimble_close(server.generation);
    if(server.connected && server.closing)try_terminate();
    return !server.faulted && !server.connected && !ble_gap_adv_active();
}
static bool arm_timer(void){
    if(ble_npl_callout_reset(&server.timer,ble_npl_time_ms_to_ticks32(100))==0)return true;
    fault(AG_NIMBLE_FAULT_TIMER);
    if(server.connected)ag_nimble_close(server.generation);
    stop_advertising();
    return false;
}
static bool send_next(void){
    if(server.awaiting || server.tx_length==0)return false;
    if(!server.subscribed || !private_allowed()){ag_nimble_close(server.generation);return false;}
    uint16_t mtu=ble_att_mtu(server.handle);
    if(mtu<=3){ag_nimble_close(server.generation);return false;}
    size_t length=server.tx_length-server.tx_offset;
    if(length>(size_t)(mtu-3))length=mtu-3;
    if(length>AG_GATT_CHUNK_BYTES)length=AG_GATT_CHUNK_BYTES;
    struct os_mbuf *buffer=ble_hs_mbuf_from_flat(server.tx+server.tx_offset,(uint16_t)length);
    if(buffer==NULL){ag_nimble_close(server.generation);return false;}
    server.inflight=length;server.awaiting=true;
    /* SDK consumes buffer on success AND failure. Only EDONE advances offset. */
    if(ble_gatts_indicate_custom(server.handle,server.response_handle,buffer)!=0){
        ag_nimble_close(server.generation);return false;
    }
    return true;
}
bool ag_nimble_send(uint64_t generation,const uint8_t *bytes,size_t length){
    if(!current(generation)||bytes==NULL||length<16||length>sizeof(server.tx)
        ||server.tx_length!=0||!server.subscribed||!private_allowed())return false;
    memcpy(server.tx,bytes,length);server.tx_length=length;server.tx_offset=0;
    server.tx_started_ms=now_ms();return send_next();
}
static int access_request(uint16_t handle,uint16_t attribute,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)attribute;(void)arg;
    if(handle!=server.handle || !server.connected || server.closing)return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    if(!private_allowed()){ag_nimble_close(server.generation);return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;}
    if(ctx->op!=BLE_GATT_ACCESS_OP_WRITE_CHR)return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    if(!server.subscribed)return BLE_ATT_ERR_UNLIKELY;
    uint16_t length=OS_MBUF_PKTLEN(ctx->om);
    if(length==0 || length>AG_GATT_CHUNK_BYTES)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t chunk[AG_GATT_CHUNK_BYTES];
    if(os_mbuf_copydata(ctx->om,0,length,chunk)!=0)return BLE_ATT_ERR_UNLIKELY;
    bool accepted=server.hooks.receive(server.hooks.context,server.generation,chunk,length,now_ms());
    wipe(chunk,sizeof(chunk));
    if(!accepted){ag_nimble_close(server.generation);return BLE_ATT_ERR_UNLIKELY;}
    return 0;
}
static int access_response(uint16_t handle,uint16_t attribute,struct ble_gatt_access_ctxt *ctx,void *arg){
    (void)handle;(void)attribute;(void)ctx;(void)arg;
    return BLE_ATT_ERR_READ_NOT_PERMITTED; /* All private output uses guarded indications. */
}
static const struct ble_gatt_svc_def services[]={
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&service_uuid.u,.characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=&request_uuid.u,.access_cb=access_request,
         .flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_ENC|BLE_GATT_CHR_F_WRITE_AUTHEN},
        {.uuid=&response_uuid.u,.access_cb=access_response,.val_handle=&server.response_handle,
         .flags=BLE_GATT_CHR_F_INDICATE},
        {0}}}, {0}};

bool ag_nimble_advertise(void){
    if(!server.started||!server.synced||server.connected||server.faulted||server.quiescing||server.generation==UINT32_MAX
        ||!server.hooks.advertise_allowed(server.hooks.context))return false;
    if(ble_gap_adv_active())return true;
    struct ble_hs_adv_fields fields={0};
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128=(ble_uuid128_t *)&service_uuid;fields.num_uuids128=1;fields.uuids128_is_complete=1;
    if(ble_gap_adv_set_fields(&fields)!=0){fault(AG_NIMBLE_FAULT_START);return false;}
    struct ble_gap_adv_params parameters={0};
    parameters.conn_mode=BLE_GAP_CONN_MODE_UND;parameters.disc_mode=BLE_GAP_DISC_MODE_GEN;
    if(ble_gap_adv_start(server.address_type,NULL,BLE_HS_FOREVER,&parameters,gap_event,NULL)!=0){
        fault(AG_NIMBLE_FAULT_START);return false;
    }
    return true;
}
static void timer_event(struct ble_npl_event *event){
    (void)event;
    if(!server.faulted && server.hooks.owner_tick!=NULL && !server.hooks.owner_tick(server.hooks.context)){
        fault(AG_NIMBLE_FAULT_OWNER);
        if(server.connected && !server.closing)ag_nimble_close(server.generation);
    }
    if(server.connected && server.closing)try_terminate();
    if(!server.connected && !server.faulted && (server.restart_advertise || !ble_gap_adv_active())){
        server.restart_advertise=false;(void)ag_nimble_advertise();
    }
    if(server.connected && !server.closing){
        uint64_t now=now_ms();
        bool alive=server.hooks.tick(server.hooks.context,server.generation,now);
        now=now_ms(); /* application/installer IO may have consumed time */
        ag_link_security_t evidence;
        if(!alive || (!ag_nimble_security(server.generation,&evidence)
            && now-server.connected_ms>=AG_GATT_REQUEST_MS)
            || (server.tx_length!=0 && now-server.tx_started_ms>=AG_GATT_FRAME_MS)){
            ag_nimble_close(server.generation);
        }
    }
    if(server.faulted || server.quiescing || !server.hooks.advertise_allowed(server.hooks.context))stop_advertising();
    if(server.started)(void)arm_timer();
}
static int gap_event(struct ble_gap_event *event,void *arg){
    /* A connection callback carries an immutable scalar tag, not a pointer to
     * mutable global generation. Old callbacks cannot act on a reused handle. */
    if(event->type==BLE_GAP_EVENT_CONNECT && arg!=NULL)return 0;
    if(event->type!=BLE_GAP_EVENT_CONNECT && server.rejected)return 0;
    if(event->type!=BLE_GAP_EVENT_CONNECT && (!server.connected
        || (uint64_t)(uintptr_t)arg!=server.generation))
        return event->type==BLE_GAP_EVENT_REPEAT_PAIRING?BLE_GAP_REPEAT_PAIRING_IGNORE:0;
    switch(event->type){
    case BLE_GAP_EVENT_CONNECT:
        if(event->connect.status!=0){(void)ag_nimble_advertise();return 0;}
        if(server.faulted || server.quiescing || !server.hooks.advertise_allowed(server.hooks.context) || server.connected || server.generation==UINT32_MAX){reject_connection(event->connect.conn_handle);return 0;}
        server.connected=true;server.closing=false;server.rejected=false;server.subscribed=false;
        server.handle=event->connect.conn_handle;++server.generation;server.connected_ms=now_ms();
        if(ble_gap_set_event_cb(server.handle,gap_event,(void *)(uintptr_t)(uint32_t)server.generation)!=0){ag_nimble_close(server.generation);return 0;}
        {struct ble_gap_conn_desc desc;uint8_t client[16]={0};
        if(ble_gap_conn_find(server.handle,&desc)!=0){ag_nimble_close(server.generation);return 0;}
        /* Per-link trusted mapping; descriptor identity can resolve during SMP.
         * Caller must bind this provisional client to this handle/generation. */
        client[0]=desc.peer_id_addr.type;memcpy(client+1,desc.peer_id_addr.val,6);client[15]=1;
        if(!server.hooks.connect(server.hooks.context,server.handle,server.generation,client)){
            ag_nimble_close(server.generation);return 0;
        }}
        if(ble_gap_security_initiate(server.handle)!=0)ag_nimble_close(server.generation);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        if(!server.connected || event->disconnect.conn.conn_handle!=server.handle)return 0;
        if(!server.closing){clear_tx(true);server.hooks.clear_display(server.hooks.context,server.generation);server.hooks.disconnect(server.hooks.context,server.generation);}
        else clear_tx(false);
        finish_disconnect();return 0;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if(event->passkey.conn_handle!=server.handle || !current(server.generation))return 0;
        if(event->passkey.params.action!=BLE_SM_IOACT_DISP){ag_nimble_close(server.generation);return 0;}
        {char display[7]={0};struct ble_sm_io io={0};bool valid=server.hooks.passkey_display(server.hooks.context,server.generation,display);
        io.action=BLE_SM_IOACT_DISP;
        if(display[6]!=0)valid=false;
        for(unsigned i=0;i<6;++i){if(display[i]<'0'||display[i]>'9')valid=false;else io.passkey=io.passkey*10+(uint32_t)(display[i]-'0');}
        if(!valid || ble_sm_inject_io(server.handle,&io)!=0)ag_nimble_close(server.generation);
        wipe(display,sizeof(display));wipe(&io,sizeof(io));return 0;}
    case BLE_GAP_EVENT_ENC_CHANGE:
        if(event->enc_change.conn_handle!=server.handle || !current(server.generation))return 0;
        {ag_link_security_t evidence;
        if(event->enc_change.status!=0 || !ag_nimble_security(server.generation,&evidence)
            ||!server.hooks.security_changed(server.hooks.context,server.generation))ag_nimble_close(server.generation);
        server.hooks.clear_display(server.hooks.context,server.generation);return 0;}
    case BLE_GAP_EVENT_SUBSCRIBE:
        if(event->subscribe.conn_handle==server.handle && event->subscribe.attr_handle==server.response_handle){
            server.subscribed=event->subscribe.cur_indicate!=0;
            if(!server.subscribed && server.tx_length!=0)ag_nimble_close(server.generation);
        }return 0;
    case BLE_GAP_EVENT_NOTIFY_TX:
        if(event->notify_tx.conn_handle!=server.handle || event->notify_tx.attr_handle!=server.response_handle
            ||!event->notify_tx.indication ||!current(server.generation))return 0;
        if(event->notify_tx.status==0)return 0; /* Enqueue/send is NOT confirmation. */
        if(event->notify_tx.status!=BLE_HS_EDONE || !server.awaiting){ag_nimble_close(server.generation);return 0;}
        server.tx_offset+=server.inflight;server.inflight=0;server.awaiting=false;
        if(server.tx_offset==server.tx_length){clear_tx(false);server.hooks.transmitted(server.hooks.context,server.generation,true);}
        else (void)send_next();
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        if(event->repeat_pairing.conn_handle==server.handle)ag_nimble_close(server.generation);
        return BLE_GAP_REPEAT_PAIRING_IGNORE; /* Never delete a bond on remote request. */
    default:return 0;
    }
}
static void synced(void){
    if(ble_hs_util_ensure_addr(0)!=0){fault(AG_NIMBLE_FAULT_START);return;}
    /* ESP32-S3 uses controller privacy via RPA own-address type; the
     * host-based rpa_config helper is ESP32-only in this pinned SDK. */
    if(ble_hs_id_infer_auto(1,&server.address_type)!=0){fault(AG_NIMBLE_FAULT_START);return;}
    server.synced=true;
    if(!arm_timer())return;
    (void)ag_nimble_advertise();
}
static void reset_host(int reason){
    (void)reason;server.synced=false;
    if(server.connected)ag_nimble_close(server.generation);
    fault(AG_NIMBLE_FAULT_HOST_RESET); /* Explicit fault, no silent restart. */
}
static int store_status(struct ble_store_status_event *event,void *arg){
    (void)event;(void)arg;fault(AG_NIMBLE_FAULT_STORE);
    if(server.connected)ag_nimble_close(server.generation);
    return BLE_HS_ESTORE_CAP; /* No round-robin bond deletion. */
}
static void host_task(void *argument){
    (void)argument;nimble_port_run();
    /* No public stop/restart lifecycle exists in this study. Unexpected loop
     * exit is explicit failure, never permission to reinitialize over SDK state. */
    fault(AG_NIMBLE_FAULT_HOST_RESET);
    if(server.connected)ag_nimble_close(server.generation);
    ble_npl_callout_stop(&server.timer);server.synced=false;
    server.host_task=NULL;server.started=false;vTaskDelete(NULL);
}
esp_err_t ag_nimble_transport_start(const ag_nimble_callbacks_t *hooks){
    if(server.started || server.faulted || hooks==NULL || hooks->fault==NULL || hooks->advertise_allowed==NULL || hooks->connect==NULL
        ||hooks->disconnect==NULL||hooks->passkey_display==NULL||hooks->clear_display==NULL
        ||hooks->security_changed==NULL||hooks->authorize_private==NULL||hooks->receive==NULL
        ||hooks->tick==NULL||hooks->transmitted==NULL)return ESP_ERR_INVALID_ARG;
    server.hooks=*hooks;server.handle=BLE_HS_CONN_HANDLE_NONE;
    esp_err_t result=nimble_port_init();if(result!=ESP_OK){fault(AG_NIMBLE_FAULT_START);return result;}
    ble_hs_cfg.reset_cb=reset_host;ble_hs_cfg.sync_cb=synced;
    ble_hs_cfg.store_status_cb=store_status;
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_ONLY;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;
    ble_hs_cfg.sm_sc=1;ble_hs_cfg.sm_sc_only=1;ble_hs_cfg.sm_oob_data_flag=0;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();ble_svc_gatt_init();ble_store_config_init();
    if(ble_gatts_count_cfg(services)!=0 || ble_gatts_add_svcs(services)!=0){fault(AG_NIMBLE_FAULT_START);(void)nimble_port_deinit();return ESP_FAIL;}
    if(ble_npl_callout_init(&server.timer,nimble_port_get_dflt_eventq(),timer_event,NULL)!=0){fault(AG_NIMBLE_FAULT_START);(void)nimble_port_deinit();return ESP_FAIL;}
    server.started=true;
    /* The pinned SDK void wrapper ignores allocation failure. Own the task and
     * check creation instead; SDK stack/core/priority settings are retained. */
    BaseType_t created=xTaskCreatePinnedToCore(host_task,"ag_nimble",NIMBLE_HS_STACK_SIZE,NULL,
        configMAX_PRIORITIES-4,&server.host_task,NIMBLE_CORE);
    if(created!=pdPASS){
        server.started=false;server.host_task=NULL;
        ble_npl_callout_stop(&server.timer);ble_npl_callout_deinit(&server.timer);
        fault(AG_NIMBLE_FAULT_START);(void)nimble_port_deinit();return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
