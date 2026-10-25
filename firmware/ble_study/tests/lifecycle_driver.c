/* Compile the UNMODIFIED production translation unit with test SDK doubles.
 * Static state is reset only to simulate a new boot between independent cases.
 * No SDK implementation, task, controller, NVS or radio executes here. */
#include <assert.h>
#include <stdio.h>
#include "ag_sdk_shim.h"
#include "../main/nimble_transport.c"

struct test_hs_cfg ble_hs_cfg;
static struct {
    int64_t us;
    int init_rc,deinit_rc,count_rc,add_rc,callout_init_rc,timer_rc;
    int ensure_rc,infer_rc,terminate_rc,set_cb_rc,secure_rc,store_rc,indicate_rc;
    int adv_fields_rc,adv_start_rc,adv_stop_rc;
    BaseType_t task_rc;
    bool present,adv,authorized,allow_adv,accept_connect,accept_receive,tick_alive;
    bool mbuf_fail,mbuf_busy,disappear_on_terminate,synchronous_status;
    uint16_t mtu;
    struct ble_gap_conn_desc desc;
    struct ble_store_value_sec key;
    struct os_mbuf outbound;
    test_gap_fn conn_callback;
    void *conn_argument;
    unsigned faults[7],init_calls,deinit_calls,task_calls,task_delete_calls;
    unsigned timer_init_calls,timer_stop_calls,timer_deinit_calls,timer_reset_calls;
    unsigned terminate_calls,connect_calls,disconnect_calls,clear_calls;
    unsigned receive_calls,transmit_ok,transmit_fail,indicate_calls,adv_stop_calls,inject_calls,adv_fields_calls,adv_start_calls;
    size_t lengths[256],sent_length;
    uint8_t sent[AG_GATT_FRAME_BYTES];
} mock;

int64_t esp_timer_get_time(void){return mock.us;}
int ble_gap_conn_find(uint16_t handle,struct ble_gap_conn_desc *out){
    if(!mock.present||handle!=mock.desc.conn_handle)return BLE_HS_ENOTCONN;
    *out=mock.desc;return 0;
}
int ble_store_read_peer_sec(const struct ble_store_key_sec *key,struct ble_store_value_sec *out){
    assert(ble_addr_cmp(&key->peer_addr,&mock.desc.peer_id_addr)==0);
    *out=mock.key;return mock.store_rc;
}
int ble_gap_terminate(uint16_t handle,int reason){
    assert(handle==mock.desc.conn_handle);assert(reason==BLE_ERR_REM_USER_CONN_TERM);
    ++mock.terminate_calls;
    if(mock.disappear_on_terminate)mock.present=false;
    return mock.terminate_rc;
}
int ble_npl_callout_reset(struct ble_npl_callout *timer,uint32_t ticks){
    assert(timer->initialized);assert(ticks==100);++mock.timer_reset_calls;
    timer->armed=mock.timer_rc==0;return mock.timer_rc;
}
uint32_t ble_npl_time_ms_to_ticks32(uint32_t ms){return ms;}
int ble_npl_callout_init(struct ble_npl_callout *timer,void *queue,void (*fn)(struct ble_npl_event *),void *arg){
    assert(queue!=NULL);assert(arg==NULL);++mock.timer_init_calls;
    if(mock.callout_init_rc!=0)return mock.callout_init_rc;
    timer->initialized=true;timer->callback=fn;return 0;
}
void ble_npl_callout_stop(struct ble_npl_callout *timer){assert(timer->initialized);timer->armed=false;++mock.timer_stop_calls;}
void ble_npl_callout_deinit(struct ble_npl_callout *timer){assert(timer->initialized);timer->initialized=false;++mock.timer_deinit_calls;}
uint16_t ble_att_mtu(uint16_t handle){assert(handle==mock.desc.conn_handle);return mock.mtu;}
struct os_mbuf *ble_hs_mbuf_from_flat(const void *bytes,uint16_t length){
    if(mock.mbuf_fail)return NULL;
    assert(!mock.mbuf_busy);assert(length<=512);mock.mbuf_busy=true;
    mock.outbound.length=length;memcpy(mock.outbound.bytes,bytes,length);return &mock.outbound;
}
static void notify(int status,void *argument){
    struct ble_gap_event event={.type=BLE_GAP_EVENT_NOTIFY_TX};
    event.notify_tx.conn_handle=mock.desc.conn_handle;
    event.notify_tx.attr_handle=server.response_handle;
    event.notify_tx.indication=true;event.notify_tx.status=status;
    assert(mock.conn_callback!=NULL);assert(mock.conn_callback(&event,argument)==0);
}
int ble_gatts_indicate_custom(uint16_t handle,uint16_t attribute,struct os_mbuf *buffer){
    assert(handle==mock.desc.conn_handle);assert(attribute==server.response_handle);
    assert(buffer==&mock.outbound && mock.mbuf_busy);
    assert(mock.indicate_calls<256);mock.lengths[mock.indicate_calls++]=buffer->length;
    assert(mock.sent_length+buffer->length<=sizeof(mock.sent));
    memcpy(mock.sent+mock.sent_length,buffer->bytes,buffer->length);mock.sent_length+=buffer->length;
    mock.mbuf_busy=false; /* The SDK consumes the supplied mbuf on all paths. */
    if(mock.synchronous_status)notify(mock.indicate_rc,mock.conn_argument);
    return mock.indicate_rc;
}
int os_mbuf_copydata(struct os_mbuf *buffer,int offset,int length,void *out){
    assert(offset>=0&&length>=0);assert((size_t)offset+(size_t)length<=buffer->length);
    assert(buffer->length<=sizeof(buffer->bytes));memcpy(out,buffer->bytes+offset,(size_t)length);return 0;
}
bool ble_gap_adv_active(void){return mock.adv;}
int ble_gap_adv_stop(void){if(mock.adv_stop_rc==0)mock.adv=false;++mock.adv_stop_calls;return mock.adv_stop_rc;}
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *fields){assert(fields->num_uuids128==1);++mock.adv_fields_calls;return mock.adv_fields_rc;}
int ble_gap_adv_start(uint8_t type,const ble_addr_t *peer,int32_t timeout,const struct ble_gap_adv_params *params,test_gap_fn fn,void *arg){
    (void)type;assert(peer==NULL&&timeout==BLE_HS_FOREVER);assert(params->conn_mode==BLE_GAP_CONN_MODE_UND);
    assert(fn==gap_event&&arg==NULL);++mock.adv_start_calls;if(mock.adv_start_rc==0)mock.adv=true;return mock.adv_start_rc;
}
int ble_gap_set_event_cb(uint16_t handle,test_gap_fn fn,void *arg){
    assert(handle==mock.desc.conn_handle);
    if(mock.set_cb_rc)return mock.set_cb_rc;
    mock.conn_callback=fn;mock.conn_argument=arg;return 0;
}
int ble_gap_security_initiate(uint16_t handle){assert(handle==mock.desc.conn_handle);return mock.secure_rc;}
int ble_sm_inject_io(uint16_t handle,const struct ble_sm_io *io){assert(handle==mock.desc.conn_handle&&io->action==BLE_SM_IOACT_DISP);++mock.inject_calls;return 0;}
int ble_hs_util_ensure_addr(int random){assert(random==0);return mock.ensure_rc;}
int ble_hs_id_infer_auto(int privacy,uint8_t *type){assert(privacy==1);*type=1;return mock.infer_rc;}
esp_err_t nimble_port_init(void){++mock.init_calls;return mock.init_rc;}
esp_err_t nimble_port_deinit(void){++mock.deinit_calls;return mock.deinit_rc;}
void nimble_port_run(void){} /* An explicit call to host_task tests unexpected return only. */
void *nimble_port_get_dflt_eventq(void){return &mock;}
void ble_svc_gap_init(void){}
void ble_svc_gatt_init(void){}
void ble_store_config_init(void){}
int ble_gatts_count_cfg(const struct ble_gatt_svc_def *svcs){assert(svcs==services);return mock.count_rc;}
int ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs){
    assert(svcs==services);server.response_handle=18;return mock.add_rc;
}
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void *),const char *name,uint32_t stack,void *arg,unsigned priority,TaskHandle_t *task,int core){
    assert(fn==host_task&&strcmp(name,"ag_nimble")==0&&arg==NULL);
    assert(stack==NIMBLE_HS_STACK_SIZE&&priority==configMAX_PRIORITIES-4&&core==NIMBLE_CORE);
    ++mock.task_calls;if(mock.task_rc==pdPASS)*task=&mock;return mock.task_rc;
}
void vTaskDelete(TaskHandle_t task){assert(task==NULL);++mock.task_delete_calls;}
static void hook_fault(void *ctx,ag_nimble_fault_t reason){assert(ctx==&mock&&reason>=1&&reason<=6);++mock.faults[reason];}
static bool hook_adv(void *ctx){assert(ctx==&mock);return mock.allow_adv;}
static bool hook_connect(void *ctx,uint16_t handle,uint64_t gen,const uint8_t client[16]){
    assert(ctx==&mock&&handle==mock.desc.conn_handle&&gen==server.generation&&client[15]==1);
    ++mock.connect_calls;return mock.accept_connect;
}
static void hook_disconnect(void *ctx,uint64_t gen){assert(ctx==&mock&&gen==server.generation);++mock.disconnect_calls;}
static bool hook_passkey(void *ctx,uint64_t gen,char out[7]){assert(ctx==&mock&&gen==server.generation);memcpy(out,"123456",7);return true;}
static void hook_clear(void *ctx,uint64_t gen){assert(ctx==&mock&&gen==server.generation);++mock.clear_calls;}
static bool hook_security(void *ctx,uint64_t gen){assert(ctx==&mock&&gen==server.generation);return mock.authorized;}
static bool hook_receive(void *ctx,uint64_t gen,const uint8_t *bytes,size_t length,uint64_t now){
    assert(ctx==&mock&&gen==server.generation&&bytes!=NULL&&length>0&&length<=512&&now==now_ms());
    ++mock.receive_calls;return mock.accept_receive;
}
static bool hook_tick(void *ctx,uint64_t gen,uint64_t now){assert(ctx==&mock&&gen==server.generation&&now==now_ms());return mock.tick_alive;}
static void hook_transmitted(void *ctx,uint64_t gen,bool confirmed){assert(ctx==&mock&&gen==server.generation);if(confirmed)++mock.transmit_ok;else ++mock.transmit_fail;}
static ag_nimble_callbacks_t callbacks(void){
    return (ag_nimble_callbacks_t){.context=&mock,.fault=hook_fault,.advertise_allowed=hook_adv,
        .connect=hook_connect,.disconnect=hook_disconnect,.passkey_display=hook_passkey,
        .clear_display=hook_clear,.security_changed=hook_security,.authorize_private=hook_security,
        .receive=hook_receive,.tick=hook_tick,.transmitted=hook_transmitted};
}
static void boot(void){
    memset(&server,0,sizeof(server));memset(&mock,0,sizeof(mock));memset(&ble_hs_cfg,0,sizeof(ble_hs_cfg));
    mock.task_rc=pdPASS;mock.allow_adv=mock.authorized=mock.accept_connect=mock.accept_receive=mock.tick_alive=true;
    mock.mtu=23;mock.desc.conn_handle=4;mock.desc.peer_id_addr.type=1;mock.desc.peer_id_addr.val[0]=0x42;
    mock.desc.sec_state.encrypted=mock.desc.sec_state.authenticated=mock.desc.sec_state.bonded=true;
    mock.desc.sec_state.key_size=16;mock.key.peer_addr=mock.desc.peer_id_addr;
    mock.key.ltk_present=mock.key.sc=mock.key.authenticated=true;mock.key.key_size=16;
}
static void start(void){ag_nimble_callbacks_t hooks=callbacks();assert(ag_nimble_transport_start(&hooks)==ESP_OK);}
static void connect_link(void){
    mock.present=true;mock.adv=false;
    struct ble_gap_event event={.type=BLE_GAP_EVENT_CONNECT};event.connect.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&event,NULL)==0);assert(server.connected&&!server.closing);
    assert(mock.conn_argument==(void *)(uintptr_t)server.generation);
}
static void ready(void){boot();start();ble_hs_cfg.sync_cb();assert(server.synced&&mock.adv);connect_link();server.subscribed=true;}
static void advance(uint64_t ms){assert(ms<=INT64_MAX/1000);mock.us=(int64_t)(ms*1000);timer_event(NULL);}
static void disconnect_link(void *arg){
    mock.present=false;struct ble_gap_event event={.type=BLE_GAP_EVENT_DISCONNECT};event.disconnect.conn=mock.desc;
    assert(gap_event(&event,arg)==0);
}
static uint8_t frame[AG_GATT_FRAME_BYTES+1];
static void fill_frame(void){for(size_t i=0;i<sizeof(frame);++i)frame[i]=(uint8_t)(i*37u+11u);}
static int write_request(size_t length){
    struct os_mbuf input={.length=length};struct ble_gatt_access_ctxt ctx={.op=BLE_GATT_ACCESS_OP_WRITE_CHR,.om=&input};
    return access_request(mock.desc.conn_handle,0,&ctx,NULL);
}

static void test_start_failure(void){
    for(unsigned cleanup_failure=0;cleanup_failure<2;++cleanup_failure){
        boot();mock.task_rc=pdFAIL;mock.deinit_rc=cleanup_failure?ESP_FAIL:ESP_OK;
        ag_nimble_callbacks_t hooks=callbacks();
        assert(ag_nimble_transport_start(&hooks)==ESP_ERR_NO_MEM);
        assert(mock.task_calls==1&&mock.deinit_calls==1&&mock.timer_stop_calls==1&&mock.timer_deinit_calls==1);
        assert(!server.started&&server.faulted&&server.host_task==NULL&&!server.timer.initialized);
        assert(mock.faults[AG_NIMBLE_FAULT_START]==1);
        assert(ag_nimble_transport_start(&hooks)==ESP_ERR_INVALID_ARG&&mock.init_calls==1);
    }
    for(unsigned which=0;which<4;++which){
        boot();if(which==0)mock.init_rc=ESP_FAIL;else if(which==1)mock.count_rc=1;else if(which==2)mock.add_rc=1;else mock.callout_init_rc=1;
        ag_nimble_callbacks_t hooks=callbacks();assert(ag_nimble_transport_start(&hooks)==ESP_FAIL);
        assert(server.faulted&&!server.started&&mock.task_calls==0);
        assert(mock.deinit_calls==(which==0?0u:1u)&&mock.faults[AG_NIMBLE_FAULT_START]==1);
    }
    boot();ag_nimble_callbacks_t hooks=callbacks();hooks.fault=NULL;
    assert(ag_nimble_transport_start(&hooks)==ESP_ERR_INVALID_ARG&&mock.init_calls==0);
}
static void test_termination_errors(void){
    for(unsigned already=0;already<2;++already){
        ready();assert(ag_nimble_send(server.generation,frame,100));
        mock.terminate_rc=already?BLE_HS_EALREADY:99;ag_nimble_close(server.generation);
        assert(mock.terminate_calls==1&&mock.disconnect_calls==1&&mock.transmit_fail==1&&server.tx_length==0);
        advance(100);advance(200);assert(mock.terminate_calls==3&&mock.faults[AG_NIMBLE_FAULT_TERMINATE]==0);
        advance(300);assert(mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1&&server.faulted&&server.closing);
        advance(400);advance(500);assert(mock.terminate_calls==3&&mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1);
        assert(!ag_nimble_send(server.generation,frame,16)&&!ag_nimble_advertise());
        disconnect_link(mock.conn_argument);assert(!server.connected&&!server.restart_advertise);
        assert(mock.disconnect_calls==1&&mock.transmit_fail==1);
    }
    ready();mock.present=false;ag_nimble_close(server.generation);
    assert(!server.connected&&!server.closing&&server.restart_advertise&&mock.terminate_calls==0);
    assert(mock.disconnect_calls==1);advance(100);assert(mock.adv);
    ready();mock.terminate_rc=BLE_HS_ENOTCONN;mock.disappear_on_terminate=true;
    ag_nimble_close(server.generation);assert(!server.connected&&mock.terminate_calls==1&&!server.faulted);
}
static void test_successful_termination(void){
    ready();ag_nimble_close(server.generation);
    advance(100);advance(29999);assert(mock.terminate_calls==1&&!server.faulted&&server.connected);
    advance(30000);assert(mock.terminate_calls==1&&server.faulted&&mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1);
    advance(31000);assert(mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1);
    ready();ag_nimble_close(server.generation);void *tag=mock.conn_argument;
    disconnect_link(tag);assert(!server.connected&&!server.closing&&mock.disconnect_calls==1);
    advance(100);assert(mock.adv&&!server.faulted&&mock.terminate_calls==1);
}
static void test_timer_and_sync_faults(void){
    ready();assert(ag_nimble_send(server.generation,frame,16));mock.timer_rc=99;mock.adv=true;
    advance(100);assert(server.faulted&&server.closing&&!mock.adv&&mock.adv_stop_calls==1);
    assert(mock.faults[AG_NIMBLE_FAULT_TIMER]==1&&mock.transmit_fail==1&&!server.timer.armed);
    assert(!ag_nimble_send(server.generation,frame,16)&&write_request(1)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN&&mock.receive_calls==0);
    for(unsigned which=0;which<3;++which){
        boot();start();if(which==0)mock.ensure_rc=1;else if(which==1)mock.infer_rc=1;else mock.timer_rc=1;
        ble_hs_cfg.sync_cb();assert(server.faulted&&!mock.adv);
        assert(mock.faults[which==2?AG_NIMBLE_FAULT_TIMER:AG_NIMBLE_FAULT_START]==1);
        assert(!ag_nimble_advertise());
    }
}
static void test_stale_generation(void){
    ready();void *old_tag=mock.conn_argument;disconnect_link(old_tag);connect_link();server.subscribed=true;
    assert(server.generation==2&&old_tag!=mock.conn_argument);
    assert(ag_nimble_send(server.generation,frame,100));
    unsigned disconnected=mock.disconnect_calls;notify(BLE_HS_EDONE,old_tag);
    assert(server.tx_offset==0&&server.awaiting&&mock.indicate_calls==1);
    struct ble_gap_event event={.type=BLE_GAP_EVENT_DISCONNECT};event.disconnect.conn=mock.desc;
    assert(gap_event(&event,old_tag)==0&&server.connected&&server.tx_length==100&&mock.disconnect_calls==disconnected);
    event.type=BLE_GAP_EVENT_SUBSCRIBE;event.subscribe.conn_handle=mock.desc.conn_handle;event.subscribe.attr_handle=server.response_handle;
    event.subscribe.cur_indicate=false;assert(gap_event(&event,old_tag)==0&&server.subscribed);
    assert(!ag_nimble_send(1,frame,16));notify(BLE_HS_EDONE,mock.conn_argument);assert(server.tx_offset==20);
    ready();disconnect_link(mock.conn_argument);server.generation=UINT32_MAX;
    assert(!ag_nimble_advertise());
    mock.present=true;
    struct ble_gap_event connect={.type=BLE_GAP_EVENT_CONNECT};connect.connect.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&connect,NULL)==0&&server.generation==UINT32_MAX&&server.connected&&server.rejected&&mock.terminate_calls==1);
}
static void test_indication_chunks(void){
    const uint16_t mtus[]={23,515,517,3};
    for(size_t m=0;m<sizeof(mtus)/sizeof(mtus[0]);++m){
        ready();mock.mtu=mtus[m];mock.synchronous_status=true;
        if(mock.mtu<=3){assert(!ag_nimble_send(server.generation,frame,16)&&server.closing);continue;}
        assert(!ag_nimble_send(server.generation,frame,15));
        assert(!ag_nimble_send(server.generation,frame,sizeof(frame)));
        assert(ag_nimble_send(server.generation,frame,AG_GATT_FRAME_BYTES));
        assert(server.tx_offset==0&&server.awaiting&&mock.transmit_ok==0);
        assert(!ag_nimble_send(server.generation,frame,16)&&mock.indicate_calls==1);
        size_t chunk=mock.mtu-3;if(chunk>512)chunk=512;
        while(server.tx_length){
            size_t before=server.tx_offset;unsigned calls=mock.indicate_calls;
            notify(0,mock.conn_argument);assert(server.tx_offset==before&&mock.indicate_calls==calls);
            notify(BLE_HS_EDONE,mock.conn_argument);
        }
        assert(mock.transmit_ok==1&&mock.transmit_fail==0&&!server.awaiting&&mock.sent_length==AG_GATT_FRAME_BYTES);
        assert(memcmp(frame,mock.sent,AG_GATT_FRAME_BYTES)==0);
        assert(mock.indicate_calls==(AG_GATT_FRAME_BYTES+chunk-1)/chunk);
        for(unsigned i=0;i<mock.indicate_calls;++i)assert(mock.lengths[i]<=chunk&&mock.lengths[i]>0);
        for(size_t i=0;i<sizeof(server.tx);++i)assert(server.tx[i]==0);
    }
}
static void test_private_guards(void){
    ready();mock.authorized=false;
    assert(!ag_nimble_send(server.generation,frame,16)&&mock.indicate_calls==0);
    assert(write_request(1)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN&&mock.receive_calls==0&&server.closing);
    ready();server.subscribed=false;
    assert(!ag_nimble_send(server.generation,frame,16));
    assert(write_request(1)==BLE_ATT_ERR_UNLIKELY&&mock.receive_calls==0&&!server.closing);
    ready();assert(write_request(512)==0&&mock.receive_calls==1);
    assert(write_request(0)==BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);
    assert(write_request(513)==BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN&&mock.receive_calls==1);
    for(unsigned which=0;which<9;++which){
        ready();if(which==0)mock.desc.sec_state.encrypted=false;
        else if(which==1)mock.desc.sec_state.authenticated=false;
        else if(which==2)mock.desc.sec_state.bonded=false;
        else if(which==3)mock.key.sc=false;else if(which==4)mock.key.ltk_present=false;
        else if(which==5)mock.key.key_size=15;else if(which==6)mock.desc.sec_state.key_size=15;
        else if(which==7)mock.store_rc=99;else mock.key.peer_addr.val[0]^=1;
        ag_link_security_t evidence;assert(!ag_nimble_security(server.generation,&evidence));
        assert(!ag_nimble_send(server.generation,frame,16)&&mock.indicate_calls==0);
        assert(write_request(1)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN&&mock.receive_calls==0);
    }
    ready();assert(ag_nimble_send(server.generation,frame,100));mock.authorized=false;
    notify(BLE_HS_EDONE,mock.conn_argument);
    assert(server.closing&&mock.indicate_calls==1&&mock.transmit_fail==1&&mock.transmit_ok==0);
    ready();assert(ag_nimble_send(server.generation,frame,100));
    struct ble_gap_event event={.type=BLE_GAP_EVENT_SUBSCRIBE};
    event.subscribe.conn_handle=mock.desc.conn_handle;event.subscribe.attr_handle=server.response_handle;
    event.subscribe.cur_indicate=false;assert(gap_event(&event,mock.conn_argument)==0);
    assert(server.closing&&mock.transmit_fail==1&&mock.indicate_calls==1);
}
static void test_send_failure_and_timeouts(void){
    ready();mock.mbuf_fail=true;assert(!ag_nimble_send(server.generation,frame,16));
    assert(server.closing&&mock.transmit_fail==1&&mock.indicate_calls==0);
    ready();mock.indicate_rc=99;mock.synchronous_status=true;
    assert(!ag_nimble_send(server.generation,frame,16));
    assert(mock.transmit_fail==1&&mock.disconnect_calls==1&&mock.terminate_calls==1&&!mock.mbuf_busy);
    ready();assert(ag_nimble_send(server.generation,frame,100));notify(99,mock.conn_argument);
    assert(server.closing&&mock.transmit_fail==1&&mock.transmit_ok==0);
    ready();assert(ag_nimble_send(server.generation,frame,100));advance(9999);assert(!server.closing);
    advance(10000);assert(server.closing&&mock.transmit_fail==1);
    ready();mock.desc.sec_state.encrypted=false;advance(29999);assert(!server.closing);
    advance(30000);assert(server.closing);
    ready();assert(ag_nimble_send(server.generation,frame,100));mock.us=9000000;notify(BLE_HS_EDONE,mock.conn_argument);
    advance(10000);assert(server.closing); /* Progress does not extend the original deadline. */
}
static void test_host_and_store_faults(void){
    ready();assert(ag_nimble_send(server.generation,frame,100));host_task(NULL);
    assert(server.faulted&&!server.started&&!server.synced&&mock.task_delete_calls==1&&mock.timer_stop_calls==1);
    assert(mock.faults[AG_NIMBLE_FAULT_HOST_RESET]==1&&mock.transmit_fail==1&&mock.disconnect_calls==1);
    ag_nimble_callbacks_t hooks=callbacks();assert(ag_nimble_transport_start(&hooks)==ESP_ERR_INVALID_ARG&&mock.init_calls==1);
    ready();ble_hs_cfg.reset_cb(99);assert(server.faulted&&!server.synced&&server.closing);
    ready();assert(store_status(NULL,NULL)==BLE_HS_ESTORE_CAP&&server.faulted&&server.closing);
    assert(mock.faults[AG_NIMBLE_FAULT_STORE]==1&&!ag_nimble_advertise());
}
static void test_bond_lookup(void){
    ready();mock.desc.sec_state.encrypted=false;
    assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_RECOGNIZED);
    mock.store_rc=BLE_HS_ENOENT;assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_UNKNOWN);
    mock.store_rc=99;assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_LOOKUP_FAILED);
    for(unsigned which=0;which<5;++which){
        ready();if(which==0)mock.key.ltk_present=false;else if(which==1)mock.key.sc=false;
        else if(which==2)mock.key.authenticated=false;else if(which==3)mock.key.key_size=15;
        else mock.key.peer_addr.val[0]^=1;
        assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_LOOKUP_FAILED);
    }
    ready();assert(ag_nimble_lookup_bond(server.generation+1)==AG_BOND_LOOKUP_FAILED);
    mock.present=false;assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_LOOKUP_FAILED);
    ready();server.faulted=true;assert(ag_nimble_lookup_bond(server.generation)==AG_BOND_LOOKUP_FAILED);
}
static void rejected_connect(void){
    struct ble_gap_event event={.type=BLE_GAP_EVENT_CONNECT};event.connect.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&event,NULL)==0);
}
static void test_early_rejected_connection_teardown(void){
    boot();start();ble_hs_cfg.sync_cb();mock.adv=false;mock.allow_adv=false;mock.present=true;mock.terminate_rc=99;
    rejected_connect();assert(server.connected&&server.closing&&server.rejected&&server.generation==1);
    assert(mock.connect_calls==0&&mock.disconnect_calls==0&&mock.clear_calls==0&&mock.terminate_calls==1);
    assert(!ag_nimble_send(server.generation,frame,16)&&write_request(1)==BLE_ATT_ERR_INSUFFICIENT_AUTHEN&&mock.receive_calls==0);
    struct ble_gap_event stale={.type=BLE_GAP_EVENT_DISCONNECT};stale.disconnect.conn=mock.desc;
    assert(gap_event(&stale,NULL)==0&&server.connected&&server.rejected);
    assert(gap_event(&stale,(void *)(uintptr_t)1)==0&&server.connected&&server.rejected);
    advance(100);advance(200);advance(300);advance(400);
    assert(mock.terminate_calls==3&&server.faulted&&mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1);
    assert(mock.connect_calls==0&&mock.disconnect_calls==0&&mock.clear_calls==0);
    mock.present=false;advance(500);assert(!server.connected&&!server.rejected&&!server.restart_advertise);

    /* A successful terminate is awaited once, and a callback with an untrusted
     * old cookie cannot clear the rejected descriptor early. */
    boot();start();ble_hs_cfg.sync_cb();mock.adv=false;mock.allow_adv=false;mock.present=true;
    rejected_connect();advance(29999);assert(mock.terminate_calls==1&&server.connected&&!server.faulted);
    assert(gap_event(&stale,NULL)==0&&server.connected);
    advance(30000);assert(server.faulted&&mock.terminate_calls==1&&mock.faults[AG_NIMBLE_FAULT_TERMINATE]==1);
    assert(mock.connect_calls==0&&mock.disconnect_calls==0);

    boot();start();ble_hs_cfg.sync_cb();mock.adv=false;mock.allow_adv=false;mock.present=true;
    mock.terminate_rc=BLE_HS_ENOTCONN;mock.disappear_on_terminate=true;rejected_connect();
    assert(!server.connected&&!server.rejected&&!server.faulted&&mock.terminate_calls==1);
    assert(mock.connect_calls==0&&mock.disconnect_calls==0);

    /* Exhaustion never wraps its immutable 32-bit cookie. Cleanup relies on
     * descriptor absence and cannot resume advertising at UINT32_MAX. */
    boot();start();ble_hs_cfg.sync_cb();mock.adv=false;mock.present=true;server.generation=UINT32_MAX;
    rejected_connect();assert(server.connected&&server.rejected&&server.generation==UINT32_MAX);
    assert(gap_event(&stale,(void *)(uintptr_t)UINT32_MAX)==0&&server.connected);
    mock.present=false;advance(100);assert(!server.connected&&!server.rejected&&!mock.adv&&!ag_nimble_advertise());
    assert(mock.connect_calls==0&&mock.disconnect_calls==0);

    boot();start();ble_hs_cfg.sync_cb();mock.adv=false;mock.present=true;server.faulted=true;
    rejected_connect();assert(server.rejected&&server.closing&&mock.connect_calls==0);
    mock.present=false;advance(100);assert(!server.connected&&!mock.adv&&server.faulted);
}
static void test_advertising_failures(void){
    for(unsigned which=0;which<2;++which){
        boot();start();if(which==0)mock.adv_fields_rc=99;else mock.adv_start_rc=99;
        ble_hs_cfg.sync_cb();assert(server.synced&&server.faulted&&!mock.adv&&mock.faults[AG_NIMBLE_FAULT_START]==1);
        assert(mock.adv_fields_calls==1&&mock.adv_start_calls==(which==0?0u:1u));
        assert(!ag_nimble_advertise());advance(100);assert(mock.adv_fields_calls==1&&mock.faults[AG_NIMBLE_FAULT_START]==1);
        ready();disconnect_link(mock.conn_argument);
        if(which==0)mock.adv_fields_rc=99;else mock.adv_start_rc=99;
        advance(100);assert(!server.restart_advertise&&server.faulted&&!mock.adv&&mock.faults[AG_NIMBLE_FAULT_START]==1);
        advance(200);assert(mock.faults[AG_NIMBLE_FAULT_START]==1&&mock.adv_fields_calls==2);
    }
    boot();start();mock.allow_adv=false;ble_hs_cfg.sync_cb();
    assert(!server.faulted&&!mock.adv&&mock.adv_fields_calls==0&&mock.adv_start_calls==0);
    assert(!ag_nimble_advertise());mock.allow_adv=true;assert(ag_nimble_advertise()&&mock.adv);
    mock.allow_adv=false;advance(100);assert(!mock.adv&&!server.faulted&&mock.adv_stop_calls==1);
    advance(200);assert(mock.adv_stop_calls==1);

    boot();start();ble_hs_cfg.sync_cb();mock.allow_adv=false;mock.adv_stop_rc=99;
    advance(100);assert(mock.adv&&server.adv_stop_failed&&server.faulted&&mock.faults[AG_NIMBLE_FAULT_START]==1);
    advance(200);advance(300);assert(mock.adv_stop_calls==1&&mock.faults[AG_NIMBLE_FAULT_START]==1);
    assert(!ag_nimble_advertise()); /* Error does not prove controller stopped. */

    boot();start();ble_hs_cfg.sync_cb();mock.timer_rc=99;mock.adv_stop_rc=99;
    timer_event(NULL);assert(server.adv_stop_failed&&server.faulted&&mock.adv);
    assert(mock.faults[AG_NIMBLE_FAULT_TIMER]==1&&mock.faults[AG_NIMBLE_FAULT_START]==0&&mock.adv_stop_calls==1);
}
static void test_permanent_quiesce(void){
    boot();assert(!ag_nimble_transport_quiesce());start();assert(!ag_nimble_transport_quiesce());
    ble_hs_cfg.sync_cb();assert(mock.adv&&ag_nimble_transport_quiesce());
    assert(server.quiescing&&!mock.adv&&!ag_nimble_advertise());
    advance(100);assert(!mock.adv&&!server.faulted);
    ready();void *old=mock.conn_argument;
    assert(!ag_nimble_transport_quiesce()&&server.closing&&mock.disconnect_calls==1);
    assert(!ag_nimble_send(server.generation,frame,sizeof(frame)));
    assert(!ag_nimble_transport_quiesce()&&mock.terminate_calls==1);
    disconnect_link(old);assert(ag_nimble_transport_quiesce());advance(100);assert(!mock.adv);
    /* Even unsolicited late successful connects are tracked and closed. */
    unsigned calls=mock.connect_calls;mock.present=true;
    struct ble_gap_event event={.type=BLE_GAP_EVENT_CONNECT};event.connect.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&event,NULL)==0&&server.rejected&&server.closing&&mock.connect_calls==calls);
    boot();start();ble_hs_cfg.sync_cb();mock.adv_stop_rc=99;
    assert(!ag_nimble_transport_quiesce()&&server.faulted&&mock.adv&&server.adv_stop_failed);
    assert(!ag_nimble_transport_quiesce()&&mock.adv_stop_calls==1);
}
int main(void){
    fill_frame();test_start_failure();test_termination_errors();test_successful_termination();
    test_timer_and_sync_faults();test_stale_generation();test_indication_chunks();
    test_private_guards();test_send_failure_and_timeouts();test_host_and_store_faults();test_bond_lookup();
    test_early_rejected_connection_teardown();test_advertising_failures();test_permanent_quiesce();
    puts("PASS: 13 NimBLE lifecycle groups (behavioral SDK doubles; no radio/ABI qualification)");return 0;
}
