/* Share explicit SDK doubles, but link endpoint_bridge.c and all core C as
 * separate, unmodified production translation units. No SDK/radio runs. */
#define main transport_regression_main
#include "lifecycle_driver.c"
#undef main
#include "endpoint_bridge.h"
#include "ag_flash_sim.h"

static struct {
    ag_nimble_bridge_t bridge;
    ag_connection_t connection;
    ag_factory_reset_t reset;
    ag_flash_sim_t flash;
    ag_config_flash_io_t memory;
    ag_config_store_t store;
    ag_pid_mask_t caps;
    bool entropy,render,awake,route;
    unsigned renders,clears,faults;
    uint64_t program_delay_ms;
    char display[7];
} device;
static bool platform_awake(void *p){assert(p==&device);return device.awake;}
static bool platform_route(void *p,bool ota){assert(p==&device);(void)ota;return device.route;}
static bool platform_random(void *p,uint32_t *out){assert(p==&device);*out=42;return device.entropy;}
static bool platform_display(void *p,const char digits[7]){
    assert(p==&device);++device.renders;memcpy(device.display,digits,7);return device.render;
}
static void platform_clear(void *p){assert(p==&device);++device.clears;memset(device.display,0,7);}
static void platform_fault(void *p,ag_nimble_fault_t reason){assert(p==&device&&reason>=1&&reason<=6);++device.faults;}
static size_t storage_read(void *p,uint32_t at,uint8_t *out,size_t n){assert(p==&device);return device.memory.read(&device.flash,at,out,n);}
static size_t storage_program(void *p,uint32_t at,const uint8_t *bytes,size_t n){
    assert(p==&device);mock.us+=(int64_t)(device.program_delay_ms*1000);device.program_delay_ms=0;
    return device.memory.program(&device.flash,at,bytes,n);
}
static bool storage_erase(void *p,uint32_t at,size_t n){assert(p==&device);return device.memory.erase(&device.flash,at,n);}
static ag_device_history_result_t unavailable_history(void *p,const uint8_t sid[16],uint64_t after,ag_history_snapshot_t *out){
    (void)p;(void)sid;(void)after;(void)out;return AG_DEVICE_HISTORY_UNAVAILABLE;
}
static ag_device_endpoint_options_t options(void){
    return (ag_device_endpoint_options_t){.history_read=unavailable_history,.device_id="HOST-SIM-001",.supported=device.caps,
        .config_persistence=AG_RPC_VOLATILE,.frame_timeout_ms=10000,.request_timeout_ms=30000,.simulation=true};
}
static ag_nimble_platform_t platform(void){
    return (ag_nimble_platform_t){.context=&device,.awake_allowed=platform_awake,.route_allowed=platform_route,.random_u32=platform_random,
        .display_passkey=platform_display,.clear_passkey=platform_clear,.fault=platform_fault};
}
static void device_init(void){
    boot();memset(&device,0,sizeof(device));device.entropy=device.render=device.awake=device.route=true;
    ag_pid_mask_set(&device.caps,12);
    ag_config_t config={.schema_version=1,.revision=1,.dashboard_count=1,.brightness_mode=AG_BRIGHTNESS_MANUAL,.manual_percent=50};
    config.dashboards[0]=(ag_dashboard_t){.id=1,.widget_count=1};
    config.dashboards[0].widgets[0]=(ag_widget_t){.id=1,.pid=12,.rect={0,0,10000,10000}};
    ag_flash_sim_init(&device.flash);device.memory=ag_flash_sim_io(&device.flash);
    uint8_t record[AG_CONFIG_RECORD_MAX];size_t written=0;
    assert(ag_config_record_encode(&config,&device.caps,1,record,sizeof(record),&written)==AG_CONFIG_STORE_OK);
    memcpy(device.flash.bytes,record,written);memcpy(device.flash.bytes+AG_CONFIG_COMMIT_OFFSET,ag_config_commit_marker,4);
    ag_config_flash_io_t io={&device,storage_read,storage_program,storage_erase};
    assert(ag_config_store_open(&device.store,&io,&device.caps)==AG_CONFIG_STORE_OK);
    uint8_t id[16]={1};assert(ag_connection_init(&device.connection,id,NULL,NULL));
    ag_reset_snapshot_t clean=ag_reset_clean_snapshot();assert(ag_factory_reset_restore(&device.reset,&device.connection,NULL,true,&clean));
    ag_admission_config_t cfg={10000,10000,1,3};ag_nimble_platform_t p=platform();ag_device_endpoint_options_t opts=options();
    assert(ag_nimble_bridge_init(&device.bridge,&device.connection,&device.store,&opts,&cfg,&p));
    assert(ag_nimble_bridge_start(&device.bridge)==ESP_OK);ble_hs_cfg.sync_cb();assert(mock.adv);
}
static void device_connect(uint8_t peer,bool known){
    mock.desc.peer_id_addr.val[0]=peer;mock.key.peer_addr=mock.desc.peer_id_addr;
    mock.store_rc=known?0:BLE_HS_ENOENT;mock.present=true;mock.adv=false;
    struct ble_gap_event event={.type=BLE_GAP_EVENT_CONNECT};event.connect.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&event,NULL)==0);
}
static void subscribe_link(void){
    struct ble_gap_event event={.type=BLE_GAP_EVENT_SUBSCRIBE};event.subscribe.conn_handle=mock.desc.conn_handle;
    event.subscribe.attr_handle=server.response_handle;event.subscribe.cur_indicate=true;
    assert(gap_event(&event,mock.conn_argument)==0&&server.subscribed);
}
static void passkey_event(void){
    struct ble_gap_event event={.type=BLE_GAP_EVENT_PASSKEY_ACTION};event.passkey.conn_handle=mock.desc.conn_handle;
    event.passkey.params.action=BLE_SM_IOACT_DISP;assert(gap_event(&event,mock.conn_argument)==0);
}
static void encrypted_event(void){
    mock.store_rc=0;struct ble_gap_event event={.type=BLE_GAP_EVENT_ENC_CHANGE};event.enc_change.conn_handle=mock.desc.conn_handle;
    assert(gap_event(&event,mock.conn_argument)==0);
}
static void admit(uint8_t peer,bool known){
    if(!known)assert(ag_nimble_bridge_pairing_window(&device.bridge,true,now_ms())==AG_ADMISSION_OK);
    device_connect(peer,known);assert(device.bridge.has_lease&&!server.closing);
    if(!known){passkey_event();assert(device.bridge.displayed&&strcmp(device.display,"000042")==0);}
    encrypted_event();assert(device.connection.state==AG_CONNECTION_VERIFIED&&device.bridge.endpoint.attached&&!server.closing);
    assert(!device.bridge.displayed&&device.display[0]==0);subscribe_link();
}
static void reset_wire(void){mock.sent_length=0;mock.indicate_calls=0;memset(mock.sent,0,sizeof(mock.sent));}
static void request(uint32_t id,const char *json){
    uint8_t bytes[AG_FRAME_MAX_SIZE];size_t length=0;
    assert(ag_frame_encode(1,id,(const uint8_t *)json,strlen(json),bytes,sizeof(bytes),&length)==AG_FRAME_OK);
    for(size_t at=0;at<length;){size_t chunk=length-at;if(chunk>512)chunk=512;
        struct os_mbuf input={.length=chunk};memcpy(input.bytes,bytes+at,chunk);
        struct ble_gatt_access_ctxt ctx={.op=BLE_GATT_ACCESS_OP_WRITE_CHR,.om=&input};
        assert(access_request(mock.desc.conn_handle,0,&ctx,NULL)==0);at+=chunk;
    }
}
static void acknowledge_frame(void){
    while(server.tx_length){notify(0,mock.conn_argument);assert(device.bridge.awaiting);notify(BLE_HS_EDONE,mock.conn_argument);}
    assert(!device.bridge.awaiting);
}
static void response(uint32_t id,const char *substring){
    ag_frame_view_t view;assert(ag_frame_decode(mock.sent,mock.sent_length,&view)==AG_FRAME_OK&&view.request_id==id);
    char json[4097];assert(view.payload_length<sizeof(json));memcpy(json,view.payload,view.payload_length);json[view.payload_length]=0;
    assert(strstr(json,substring)!=NULL);
}
static const char apply[]="{\"op\":\"config.apply\",\"expected_revision\":1,\"configuration\":{\"schema_version\":1,\"revision\":2,\"brightness\":{\"mode\":\"manual\",\"manual_percent\":50},\"dashboards\":[{\"id\":1,\"widgets\":[{\"id\":1,\"pid\":12,\"rect\":{\"x\":0,\"y\":0,\"width\":10000,\"height\":10000}}]}]}}";
static void test_pairing_and_hello(void){
    device_init();device_connect(0x41,false);
    assert(server.closing&&!device.bridge.has_lease&&device.connection.state==AG_CONNECTION_IDLE&&device.renders==0);
    disconnect_link(mock.conn_argument);admit(0x41,false);
    assert(device.renders==1&&mock.inject_calls==1&&device.bridge.admission.recognized);
    reset_wire();request(17,"{\"op\":\"hello\"}");timer_event(NULL);
    assert(device.bridge.awaiting&&server.tx_length>0);acknowledge_frame();response(17,"\"authenticated\":true");
}
static void test_received_confirmation_then_commit(void){
    device_init();admit(0x41,true);reset_wire();request(91,apply);timer_event(NULL);
    assert(device.bridge.endpoint.phase==AG_DEVICE_APPLY_COMMIT&&device.store.active.revision==1&&device.flash.programs==0);
    assert(device.bridge.awaiting);for(unsigned i=0;i<3;++i){notify(0,mock.conn_argument);timer_event(NULL);}
    assert(device.store.active.revision==1&&device.flash.programs==0&&device.bridge.awaiting);
    acknowledge_frame();response(91,"\"phase\":\"received\"");
    assert(device.store.active.revision==1&&device.flash.programs==0);
    reset_wire();timer_event(NULL);assert(device.store.active.revision==2&&device.flash.programs>0&&device.bridge.awaiting);
    acknowledge_frame();response(91,"\"phase\":\"applied\"");
    /* RAM simulator is deliberately labelled volatile, although model bytes
     * survive a simulated reboot and the real record codec verifies readback. */
    response(91,"\"persistence\":\"volatile\"");
    ag_flash_sim_reboot(&device.flash);assert(ag_config_store_restore(&device.store)==AG_CONFIG_STORE_OK&&device.store.active.revision==2);
}
static void test_switch_and_drop_before_commit(void){
    device_init();admit(0x41,true);void *a=mock.conn_argument;ag_connection_lease_t lease_a=device.bridge.lease;
    reset_wire();request(1,apply);timer_event(NULL);assert(device.bridge.awaiting);ag_nimble_close(server.generation);
    assert(!device.bridge.has_lease&&!device.bridge.endpoint.attached&&device.store.active.revision==1&&device.flash.programs==0);
    disconnect_link(a);admit(0x42,true);void *b=mock.conn_argument;assert(lease_a.counter!=device.bridge.lease.counter);
    reset_wire();request(1,"{\"op\":\"hello\"}");timer_event(NULL);notify(BLE_HS_EDONE,a);
    assert(server.tx_offset==0&&device.bridge.awaiting&&device.bridge.has_lease);
    struct ble_gap_event late={.type=BLE_GAP_EVENT_DISCONNECT};late.disconnect.conn=mock.desc;
    assert(gap_event(&late,a)==0&&device.bridge.has_lease&&device.bridge.endpoint.attached);
    acknowledge_frame();response(1,"\"authenticated\":true");disconnect_link(b);admit(0x41,true);
    assert(device.bridge.lease.counter!=lease_a.counter&&device.bridge.generation==3);
    reset_wire();request(1,"{\"op\":\"config.get\"}");timer_event(NULL);acknowledge_frame();response(1,"\"revision\":1");
    reset_wire();request(2,apply);timer_event(NULL);device.awake=false;timer_event(NULL);
    assert(server.closing&&!device.bridge.has_lease&&device.store.active.revision==1&&device.flash.programs==0);
}
static void test_platform_fail_closed(void){
    for(unsigned which=0;which<2;++which){
        device_init();assert(ag_nimble_bridge_pairing_window(&device.bridge,true,now_ms())==AG_ADMISSION_OK);
        device_connect(0x41,false);if(which==0)device.entropy=false;else device.render=false;
        passkey_event();assert(server.closing&&!device.bridge.has_lease&&!device.bridge.displayed&&!device.bridge.endpoint.attached);
        assert(device.connection.state==AG_CONNECTION_IDLE&&device.display[0]==0&&device.store.active.revision==1);
        assert(device.renders==(which==0?0u:1u)&&mock.inject_calls==0);
    }
    device_init();device.awake=false;assert(!ag_nimble_advertise());
    /* Platform pointers are mandatory; no entropy/display fallback exists. */
    device_init();device.connection.verifier=NULL;device.connection.verifier_context=NULL;
    ag_nimble_bridge_t other;ag_device_endpoint_options_t opts=options();ag_admission_config_t cfg={10000,10000,1,3};
    ag_nimble_platform_t p=platform();p.random_u32=NULL;
    assert(!ag_nimble_bridge_init(&other,&device.connection,&device.store,&opts,&cfg,&p));
    p=platform();p.display_passkey=NULL;assert(!ag_nimble_bridge_init(&other,&device.connection,&device.store,&opts,&cfg,&p));
    p=platform();p.route_allowed=NULL;assert(!ag_nimble_bridge_init(&other,&device.connection,&device.store,&opts,&cfg,&p));
    device_init();admit(0x41,true);reset_wire();request(1,apply);timer_event(NULL);
    device.route=false;timer_event(NULL);assert(server.closing&&!device.bridge.has_lease&&device.flash.programs==0);
}
static void test_fresh_time_after_io(void){
    device_init();admit(0x41,true);reset_wire();request(71,apply);timer_event(NULL);acknowledge_frame();
    reset_wire();device.program_delay_ms=40000;timer_event(NULL);
    assert(now_ms()==40000&&device.store.active.revision==2);
    assert(server.closing&&!device.bridge.has_lease&&mock.sent_length==0);
    /* Commit completed, but expired request does not emit a stale applied.
     * A later qualified connection must re-read the committed revision. */
    assert(ag_config_store_restore(&device.store)==AG_CONFIG_STORE_OK&&device.store.active.revision==2);
}
static struct {
    ag_ot_owner_t owner;
    bool local_allowed,backend_allowed,cancel_ok,select_ok;
    unsigned begins,writes,finishes,selects,cancels,closed;
    ag_ot_phase_t closed_phase;
    bool closed_unknown,closed_selection;
    uint64_t delay_ms;
} ota;
static bool ota_policy(void *p,const ag_connection_lease_t *lease){
    assert(p==&ota&&lease->counter==device.bridge.lease.counter);return ota.local_allowed;
}
static void ota_closed(void *p,ag_ot_phase_t phase,bool unknown,bool selection){
    assert(p==&ota);++ota.closed;ota.closed_phase=phase;ota.closed_unknown=unknown;ota.closed_selection=selection;
}
static bool ota_backend_allowed(void *p,uint64_t lease){assert(p==&ota&&lease!=0);return ota.backend_allowed;}
static uint64_t ota_backend_clock(void *p){assert(p==&ota);return now_ms();}
static bool ota_begin(void *p,uint64_t lease,const uint8_t *manifest,size_t n,uint32_t *bytes){
    assert(p==&ota&&lease!=0&&n==1&&manifest[0]==0x42);++ota.begins;*bytes=16;
    mock.us+=(int64_t)(ota.delay_ms*1000);ota.delay_ms=0;return true;
}
static bool ota_write(void *p,uint64_t lease,uint32_t at,const uint8_t *bytes,size_t n){
    assert(p==&ota&&lease!=0&&at==0&&bytes!=NULL&&n==16);++ota.writes;return true;
}
static bool ota_finish(void *p,uint64_t lease){assert(p==&ota&&lease!=0);++ota.finishes;return true;}
static bool ota_select(void *p,uint64_t lease){assert(p==&ota&&lease!=0);++ota.selects;return ota.select_ok;}
static bool ota_poll(void *p,uint64_t lease){assert(p==&ota&&lease!=0);return true;}
static bool ota_cancel(void *p,uint64_t lease){assert(p==&ota&&lease!=0);++ota.cancels;return ota.cancel_ok;}
static void arm_ota(void){
    memset(&ota,0,sizeof(ota));ota.local_allowed=ota.backend_allowed=ota.cancel_ok=ota.select_ok=true;
    ag_ot_hooks_t hooks={.context=&ota,.allowed=ota_backend_allowed,.clock_ms=ota_backend_clock,
        .begin=ota_begin,.write=ota_write,.finish=ota_finish,.select=ota_select,.poll=ota_poll,.cancel=ota_cancel,
        .idle_timeout_ms=10000,.total_timeout_ms=60000};
    assert(ag_ot_owner_init(&ota.owner,&hooks));
    ag_nimble_ota_options_t options={.context=&ota,.allowed=ota_policy,.closed=ota_closed,
        .frame_timeout_ms=1000,.request_timeout_ms=30000};
    assert(ag_nimble_bridge_prepare_ota(&device.bridge,&ota.owner,&options));
}
static void admit_ota(void){
    device_connect(0x41,true);assert(device.bridge.ota_active&&device.bridge.has_lease);
    encrypted_event();assert(device.connection.state==AG_CONNECTION_VERIFIED&&!server.closing);
    assert(device.bridge.ota_endpoint.initialized&&!device.bridge.endpoint.attached);subscribe_link();
}
static void binary_request(uint32_t id,ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t body_length){
    ag_ot_request_t request={op,0x1234,offset,body,body_length};uint8_t payload[AG_OTA_TRANSFER_REQUEST_MAX];
    uint8_t frame[AG_FRAME_MAX_SIZE];size_t n=0,length=0;
    assert(ag_ot_request_encode(&request,payload,sizeof(payload),&n));
    assert(ag_frame_encode(1,id,payload,n,frame,sizeof(frame),&length)==AG_FRAME_OK);
    assert(length<=512);struct os_mbuf input={.length=length};memcpy(input.bytes,frame,length);
    struct ble_gatt_access_ctxt ctx={.op=BLE_GATT_ACCESS_OP_WRITE_CHR,.om=&input};
    assert(access_request(mock.desc.conn_handle,0,&ctx,NULL)==0);
}
static ag_ot_response_t ota_response(uint32_t id){
    ag_frame_view_t frame;ag_ot_response_t response;
    assert(ag_frame_decode(mock.sent,mock.sent_length,&frame)==AG_FRAME_OK&&frame.request_id==id);
    assert(ag_ot_response_decode(frame.payload,frame.payload_length,&response));return response;
}
static void exchange(uint32_t id,ag_ot_opcode_t op,uint32_t offset,const uint8_t *body,size_t n){
    reset_wire();binary_request(id,op,offset,body,n);timer_event(NULL);
    assert(device.bridge.awaiting&&!server.closing);acknowledge_frame();assert(ota_response(id).status==AG_OT_OK);
}
static void begin_ota(void){
    uint8_t manifest=0x42;exchange(1,AG_OT_START,1,NULL,0);exchange(2,AG_OT_MANIFEST,0,&manifest,1);
    exchange(3,AG_OT_BEGIN,0,NULL,0);assert(ota.begins==1&&ota.owner.phase==AG_OT_WRITING);
}
static void test_ota_exclusive_local_route(void){
    device_init();arm_ota();admit_ota();
    assert(!ag_nimble_bridge_prepare_rpc(&device.bridge));
    assert(!ag_nimble_bridge_prepare_ota(&device.bridge,&ota.owner,&device.bridge.ota_options));
    /* JSON hello never selects RPC or mutates config in a locally armed OTA link. */
    reset_wire();request(1,"{\"op\":\"hello\"}");timer_event(NULL);acknowledge_frame();
    assert(ota_response(1).status==AG_OT_INVALID&&ota.owner.phase==AG_OT_IDLE&&!device.bridge.endpoint.attached);
    assert(device.store.active.revision==1&&device.flash.programs==0);
    /* Requests have a single sequence space; next request must be 2. */
    exchange(2,AG_OT_START,1,NULL,0);assert(ota.owner.phase==AG_OT_MANIFEST_PHASE);
    disconnect_link(mock.conn_argument);assert(ota.closed==1&&ota.closed_phase==AG_OT_FAULT&&ota.cancels==0);
    assert(ag_nimble_bridge_prepare_rpc(&device.bridge));admit(0x42,true);
    reset_wire();binary_request(1,AG_OT_START,1,NULL,0);timer_event(NULL);acknowledge_frame();
    assert(ota.owner.phase==AG_OT_FAULT&&ota.begins==0&&device.store.active.revision==1);
}
static void test_ota_confirmation_revocation_cleanup(void){
    device_init();arm_ota();admit_ota();begin_ota();void *old=mock.conn_argument;
    reset_wire();uint8_t bytes[16]={0};binary_request(4,AG_OT_IMAGE,0,bytes,sizeof(bytes));timer_event(NULL);
    assert(device.bridge.awaiting&&ota.writes==1);
    /* One subsequent request may be staged, but no finish before final EDONE. */
    binary_request(5,AG_OT_FINISH,0,NULL,0);timer_event(NULL);
    assert(ota.finishes==0&&device.bridge.ota_endpoint.staged);
    ota.local_allowed=false;timer_event(NULL);
    assert(server.closing&&!device.bridge.has_lease&&ota.cancels==1&&ota.closed==1);
    assert(!ota.closed_unknown&&!ota.closed_selection&&ota.finishes==0);
    server.hooks.transmitted(server.hooks.context,1,false);assert(ota.closed==1&&ota.cancels==1);
    disconnect_link(old);assert(ag_nimble_bridge_prepare_rpc(&device.bridge));admit(0x42,true);
    notify(BLE_HS_EDONE,old);assert(device.bridge.has_lease&&device.bridge.endpoint.attached);
}
static void test_ota_unknown_and_selection_are_latched(void){
    device_init();arm_ota();admit_ota();begin_ota();ota.cancel_ok=false;
    disconnect_link(mock.conn_argument);assert(ota.cancels==1&&ota.closed==1&&ota.closed_unknown);
    assert(!ag_nimble_bridge_prepare_rpc(&device.bridge));
    device_init();arm_ota();admit_ota();begin_ota();uint8_t bytes[16]={0};
    exchange(4,AG_OT_IMAGE,0,bytes,sizeof(bytes));exchange(5,AG_OT_FINISH,0,NULL,0);
    ota.select_ok=false;reset_wire();binary_request(6,AG_OT_SELECT,0,NULL,0);timer_event(NULL);
    acknowledge_frame();ag_ot_response_t response=ota_response(6);
    assert(response.status==AG_OT_BACKEND_FAILURE&&response.selection_may_have_changed&&ota.selects==1&&ota.cancels==0);
    disconnect_link(mock.conn_argument);assert(ota.closed_selection&&!ag_nimble_bridge_prepare_rpc(&device.bridge));
}
static void test_ota_local_denial_and_fresh_deadline(void){
    device_init();arm_ota();ota.local_allowed=false;device_connect(0x41,true);encrypted_event();
    assert(server.closing&&!device.bridge.has_lease&&!device.bridge.endpoint.attached&&ota.begins==0&&ota.closed==1);
    device_init();arm_ota();admit_ota();uint8_t manifest=0x42;
    exchange(1,AG_OT_START,1,NULL,0);exchange(2,AG_OT_MANIFEST,0,&manifest,1);
    ota.delay_ms=40000;reset_wire();binary_request(3,AG_OT_BEGIN,0,NULL,0);timer_event(NULL);
    assert(now_ms()==40000&&ota.begins==1&&ota.cancels==1&&ota.closed==1);
    assert(server.closing&&!device.bridge.has_lease&&mock.sent_length==0);
}
static void test_bound_application_endpoint_single_pump(void){
    device_init();ag_nimble_platform_t p=platform();
    assert(!ag_nimble_bridge_init_bound(&device.bridge,&device.connection,&device.bridge.admission,&device.bridge.endpoint,&p));
    ag_admission_t a={0};ag_device_endpoint_t e={0};
    ag_admission_hooks_t hooks=device.bridge.admission.hooks;ag_admission_config_t cfg=device.bridge.admission.config;
    device.connection.verifier=NULL;device.connection.verifier_context=NULL;
    assert(ag_admission_init(&a,&device.connection,&cfg,&hooks));
    ag_device_endpoint_options_t opts=options();assert(ag_device_endpoint_init(&e,&a,&device.store,&opts));
    /* Fixture boot resets only the SDK mock singleton; no hardware is running. */
    boot();assert(ag_nimble_bridge_init_bound(&device.bridge,&device.connection,&a,&e,&p));
    assert(a.hooks.context==&device.bridge&&device.bridge.admission_ref==&a&&device.bridge.endpoint_ref==&e);
    assert(ag_nimble_bridge_start(&device.bridge)==ESP_OK);ble_hs_cfg.sync_cb();
    device_connect(0x41,true);encrypted_event();subscribe_link();assert(e.attached&&!device.bridge.endpoint.attached);
    reset_wire();request(1,apply);timer_event(NULL);assert(e.phase==AG_DEVICE_APPLY_COMMIT&&device.flash.programs==0);
    for(unsigned i=0;i<3;++i)timer_event(NULL);
    assert(device.flash.programs==0&&device.bridge.awaiting);acknowledge_frame();
    reset_wire();timer_event(NULL);acknowledge_frame();assert(device.store.active.revision==2);response(1,"\"phase\":\"applied\"");
    disconnect_link(mock.conn_argument);assert(!a.link_open&&!e.attached&&!device.bridge.has_lease);
}
static struct {ag_admission_t a;ag_device_endpoint_t e;bool ready,ok;unsigned ticks;} deferred;
static bool application_tick(void *p){
    assert(p==&device);++deferred.ticks;
    if(deferred.ready&&!deferred.e.initialized){ag_device_endpoint_options_t opts=options();assert(ag_device_endpoint_init(&deferred.e,&deferred.a,&device.store,&opts));}
    return deferred.ok;
}
static void test_deferred_binding_and_owner_tick(void){
    device_init();memset(&deferred,0,sizeof(deferred));deferred.ok=true;
    ag_admission_hooks_t hooks=device.bridge.admission.hooks;ag_admission_config_t cfg=device.bridge.admission.config;
    device.connection.verifier=NULL;device.connection.verifier_context=NULL;
    assert(ag_admission_init(&deferred.a,&device.connection,&cfg,&hooks));
    ag_nimble_platform_t p=platform();p.owner_tick=application_tick;
    boot();assert(ag_nimble_bridge_init_bound(&device.bridge,&device.connection,&deferred.a,&deferred.e,&p));
    assert(ag_nimble_bridge_start(&device.bridge)==ESP_OK);ble_hs_cfg.sync_cb();
    assert(!mock.adv&&!deferred.e.initialized);timer_event(NULL);
    assert(deferred.ticks==1&&!mock.adv&&!server.faulted);
    deferred.ready=true;timer_event(NULL);
    assert(deferred.ticks==2&&deferred.e.initialized&&mock.adv&&!server.faulted);
    device_connect(0x41,true);encrypted_event();subscribe_link();assert(deferred.e.attached);
    deferred.ok=false;timer_event(NULL);
    assert(deferred.ticks==3&&server.faulted&&server.closing&&!device.bridge.has_lease&&!deferred.e.attached);
    assert(device.faults==1);timer_event(NULL);assert(deferred.ticks==3&&device.faults==1);
}
int main(void){
    fill_frame();test_pairing_and_hello();test_received_confirmation_then_commit();
    test_switch_and_drop_before_commit();test_platform_fail_closed();test_fresh_time_after_io();
    test_ota_exclusive_local_route();test_ota_confirmation_revocation_cleanup();
    test_ota_unknown_and_selection_are_latched();test_ota_local_denial_and_fresh_deadline();
    test_bound_application_endpoint_single_pump();test_deferred_binding_and_owner_tick();
    puts("PASS: 11 NimBLE bridge/core groups (real core, SDK doubles, no radio)");return 0;
}
