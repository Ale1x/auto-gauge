/* Production native_profile.c, endpoint bridge and complete core are linked
 * separately. SDK-facing adapters below are behavioral LOCAL test doubles. */
#include "native_profile.h"
#include "boot_storage.h"
#include "endpoint_bridge.h"
#include "maintenance_store.h"
#include "transfer_adapter.h"
#include "ag_bench_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ag_native_board_profile_t profile;
static ag_bench_profile_t fixture;
static ag_application_t fixture_owner;
static ag_reset_snapshot_t durable_reset;
static ag_nimble_callbacks_t radio;
static ag_twai_port_t *can_port;
static ag_maintenance_store_t *maintenance;
static const char *scenario;
static ag_native_local_event_t next_event;
static unsigned prepares,entropy_calls,partitions,maintenance_binds,boot_binds,can_inits,radio_starts,rows,power_calls,quiesces;
static unsigned erased_credentials,erased_configuration,erased_logs,reset_writes,ota_inits;
static bool usb=true,zero_entropy,entropy_failure,prepare_failure,radio_quiesce_result=true;
static int64_t ticks_us;
static uint64_t row_delay_us;
static ag_gauge_power_t last_power;
static ag_manifest_trust_t captured_trust;
int64_t esp_timer_get_time(void){return ticks_us;}
static bool write_reset(void *unused,const ag_reset_snapshot_t *value){(void)unused;durable_reset=*value;++reset_writes;return true;}
static bool erase_credentials(void *unused){(void)unused;++erased_credentials;return strcmp(scenario,"interrupted-credentials-fail")!=0;}
static bool maintenance_credentials(void *unused){(void)unused;return maintenance->erase_credentials(maintenance->credentials_context);}
static bool erase_config(void *unused){(void)unused;++erased_configuration;for(unsigned i=0;i<2;++i)if(!fixture.configuration.io.erase(fixture.configuration.io.context,i*AG_CONFIG_SLOT_BYTES,AG_CONFIG_SLOT_BYTES))return false;return true;}
static bool erase_log(void *unused){(void)unused;++erased_logs;for(unsigned i=0;i<2;++i)if(!fixture.journal.io.erase_sector(fixture.journal.io.context,i*4096u))return false;return true;}
static bool prepare(void *unused){
    (void)unused;++prepares;if(prepare_failure)return false;
    uint8_t sid[16]={0x72};assert(ag_bench_profile_start(&fixture,&fixture_owner,sid));durable_reset=ag_reset_clean_snapshot();
    if(!strncmp(scenario,"interrupted",11)){
        ag_reset_io_t io={NULL,write_reset,erase_credentials,erase_config,erase_log};
        ag_connection_t connection;ag_factory_reset_t reset;
        assert(ag_connection_init(&connection,sid,NULL,NULL));
        assert(ag_factory_reset_restore(&reset,&connection,&io,true,&durable_reset));
        ag_local_reset_event_t event={AG_RESET_ORIGIN_LOCAL_PHYSICAL,true,true,true};
        assert(ag_factory_reset_arm(&reset,&event)==AG_RESET_OK);
        assert(ag_factory_reset_confirm(&reset,&event)==AG_RESET_OK);
    }
    if(!strcmp(scenario,"corrupt-boot"))fixture.boot_bytes[0]^=1;
    return true;
}
static bool random_word(void *unused,uint32_t *out){(void)unused;++entropy_calls;*out=zero_entropy?0:0x23456789u+entropy_calls;return !entropy_failure;}
static bool policy(void *unused,ag_can_policy_t *out){(void)unused;*out=fixture.policy;return true;}
static bool yes(void *unused){(void)unused;return true;}
static bool usb_valid(void *unused){(void)unused;return usb;}
static bool local(void *unused,ag_native_local_event_t *event){(void)unused;*event=next_event;memset(&next_event,0,sizeof(next_event));return event->kind!=AG_LOCAL_NONE;}
static bool display(void *unused,ag_gauge_power_t p,ag_gauge_brightness_t b,uint8_t percent){(void)unused;(void)b;(void)percent;++power_calls;last_power=p;return true;}
static bool write_row(void *unused,uint16_t y,const uint16_t *pixels,size_t width){(void)unused;(void)y;assert(pixels&&width==466);++rows;ticks_us+=(int64_t)row_delay_us;return true;}
static bool passkey(void *unused,const char digits[7]){(void)unused;assert(digits[6]==0);return true;}
static void clear(void *unused){(void)unused;}
static uint32_t health(void *unused){(void)unused;return 0;}
static bool allow_ota(void *unused,const ag_connection_lease_t *lease){(void)unused;(void)lease;return false;}
static size_t key(void *unused,const char *id,uint8_t *out,size_t cap){(void)unused;(void)id;(void)out;(void)cap;return 0;}
const ag_native_board_profile_t *ag_native_board_profile(void){return !strcmp(scenario,"absent")?NULL:&profile;}
bool ag_partition_store_bind(ag_partition_store_t *s,const ag_partition_layout_t *layout){
    ++partitions;assert(layout->history_sectors==2);memset(s,0,sizeof(*s));s->initialized=true;
    s->config_io=fixture.configuration.io;s->history_io=fixture.journal.io;s->history_identity.size=8192;return true;
}
bool ag_maintenance_store_bind(ag_maintenance_store_t *s,const char *label,ag_partition_store_t *p,void *ctx,ag_reset_erase_t erase,ag_reset_snapshot_t *out,ag_reset_io_t *io){
    ++maintenance_binds;assert(!strcmp(label,"nvs")&&p->initialized);memset(s,0,sizeof(*s));s->initialized=true;s->partitions=p;s->credentials_context=ctx;s->erase_credentials=erase;maintenance=s;
    *out=durable_reset;*io=(ag_reset_io_t){NULL,write_reset,maintenance_credentials,erase_config,erase_log};return true;
}
bool ag_ota_boot_storage_bind(ag_ota_boot_storage_t *s,const char *label,ag_boot_hooks_t *hooks){
    ++boot_binds;assert(!strcmp(label,"nvs"));s->initialized=true;*hooks=fixture.boot.hooks;return true;
}
bool ag_ota_boot_prepare_selection(void *ctx,const ag_manifest_verified_t *v,const esp_partition_t *p){(void)ctx;(void)v;(void)p;assert(!"No OTA selection in this harness");return false;}
bool ag_ota_transfer_adapter_init(ag_ota_transfer_adapter_t *s,const ag_manifest_trust_t *trust,const ag_ota_transfer_platform_t *platform){
    ++ota_inits;captured_trust=*trust;s->trust=*trust;s->platform=*platform;s->initialized=true;s->transfer.initialized=true;s->transfer.phase=AG_OT_IDLE;return true;
}
bool ag_twai_port_init(ag_twai_port_t *p,const ag_twai_port_config_t *c){++can_inits;memset(p,0,sizeof(*p));p->initialized=true;p->config=*c;can_port=p;return true;}
ag_twai_result_t ag_twai_port_start(ag_twai_port_t *p){p->phase=AG_TWAI_RUNNING;return AG_TWAI_OK;}
ag_twai_result_t ag_twai_port_resume(ag_twai_port_t *p){p->phase=AG_TWAI_RUNNING;return AG_TWAI_OK;}
ag_twai_result_t ag_twai_port_poll(ag_twai_port_t *p){(void)p;return AG_TWAI_OK;}
ag_twai_result_t ag_twai_port_suspend(ag_twai_port_t *p){assert(!p->pending);p->phase=AG_TWAI_SUSPENDED;p->discovery=NULL;p->runtime=NULL;return AG_TWAI_OK;}
ag_twai_result_t ag_twai_port_close(ag_twai_port_t *p){p->phase=AG_TWAI_CLOSED;return AG_TWAI_OK;}
ag_twai_result_t ag_twai_port_send_discovery(ag_twai_port_t *p,ag_discovery_t *d){
    p->discovery=d;ag_can_policy_t current;assert(p->config.read_policy(p->config.policy_context,&current));
    uint64_t now=(uint64_t)ticks_us/1000u;ag_simulated_dispatch_t dispatch;
    if(ag_discovery_prepare(d,&current,now)&&ag_discovery_dispatch(d,&current,now,&dispatch)){
        if(!strcmp(scenario,"queued")){p->pending=true;p->tx_bytes[0]=2;p->tx_bytes[1]=1;return AG_TWAI_QUEUED;}
        ag_can_frame_t frame={.id=dispatch.profile.response_id,.length=8,.data={6,0x41,0,0,0x18,0,0,0}};
        assert(ag_discovery_receive(d,&current,&frame,now)==AG_DISCOVERY_RX_ACCEPTED);
    }
    return AG_TWAI_OK;
}
ag_twai_result_t ag_twai_port_send_flow_control(ag_twai_port_t *p,ag_runtime_t *r){p->runtime=r;return AG_TWAI_NO_WORK;}
ag_twai_result_t ag_twai_port_send_runtime(ag_twai_port_t *p,ag_runtime_t *r){
    p->runtime=r;ag_can_policy_t current;assert(p->config.read_policy(p->config.policy_context,&current));uint64_t now=(uint64_t)ticks_us/1000u;
    ag_simulated_dispatch_t dispatch;
    if(ag_runtime_prepare(r,&current,now)&&ag_runtime_dispatch(r,&current,now,&dispatch)){
        uint8_t pid=dispatch.request.bytes[1];ag_can_frame_t frame={.id=dispatch.profile.response_id,.length=8,.data={4,0x41,pid,0x1f,0x40}};
        if(pid==13){frame.data[0]=3;frame.data[3]=42;}
        assert(ag_runtime_receive(r,&current,&frame,now)==AG_SCHED_RX_ACCEPTED);
    }
    return AG_TWAI_OK;
}
esp_err_t ag_nimble_transport_start(const ag_nimble_callbacks_t *hooks){++radio_starts;radio=*hooks;return ESP_OK;}
bool ag_nimble_transport_quiesce(void){++quiesces;return radio_quiesce_result;}
ag_bond_lookup_t ag_nimble_lookup_bond(uint64_t generation){(void)generation;return AG_BOND_LOOKUP_FAILED;}
bool ag_nimble_security(uint64_t generation,ag_link_security_t *out){(void)generation;(void)out;return false;}
bool ag_nimble_send(uint64_t generation,const uint8_t *bytes,size_t length){(void)generation;(void)bytes;(void)length;return false;}
static void setup(void){
    profile=(ag_native_board_profile_t){.device_id="NATIVE-COMPOSITION-HOST",.nvs_label="nvs",
        .partitions={.config_label="config",.history_label="logs",.config_subtype=0x40,.history_subtype=0x41,.history_sectors=2},
        .can={.tx_gpio=4,.rx_gpio=5,.bitrate=500000,.tx_timeout_ms=20,.maximum_rx_age_ms=100,.id_format=AG_CAN_ID_11,.hardware_profile_qualified=true},
        .ecus={{AG_CAN_ID_11,500000,0x7e0,0x7e8}},.ecu_count=1,
        .trust={.hardware_id="ag-host-fixture",.inactive_capacity=1024*1024,.lookup_key=key},
        .admission={60000,10000,1000,3},.required_health=AG_BOOT_HEALTH_REQUIRED_MIN,
        .response_timeout_ms=100,.poll_interval_ms=200,.stale_after_ms=2000,.ui={2000,100},
        .panel_width=466,.panel_height=466,.maximum_frame_ms=100,.ota_idle_timeout_ms=10000,.ota_total_timeout_ms=60000,
        .prepare=prepare,.read_policy=policy,.radio_allowed=yes,.usb_power_valid=usb_valid,.random_u32=random_word,
        .local_event=local,.display_power=display,.write_row=write_row,.display_passkey=passkey,.clear_passkey=clear,
        .health_mask=health,.ota_allowed=allow_ota,.erase_credentials=erase_credentials};
}
static void event(ag_native_event_kind_t kind,bool confirmed){next_event=(ag_native_local_event_t){kind,true,confirmed};}
static void tick(unsigned ms){ticks_us=(int64_t)ms*1000;if(!radio.owner_tick(radio.context)){fprintf(stderr,"tick failed at %u ms, reset phase %u\n",ms,(unsigned)durable_reset.phase);assert(false);}}
static ag_nimble_bridge_t *bridge(void){return radio.context;}
static void good(void){
    assert(ag_native_profile_start());assert(prepares==1&&entropy_calls==4&&partitions==1&&maintenance_binds==1&&boot_binds==1&&can_inits==1&&radio_starts==1);
    assert(!radio.advertise_allowed(radio.context));tick(0);tick(100);
    assert(bridge()->endpoint_ref->initialized&&radio.advertise_allowed(radio.context));assert(rows==932&&last_power==AG_GAUGE_POWER_ON);
    unsigned old_quiesces=quiesces;usb=false;event(AG_LOCAL_RESET_ARM,false);tick(101);
    assert(quiesces==old_quiesces&&durable_reset.phase==AG_RESET_CLEAN&&radio.advertise_allowed(radio.context));usb=true;
    event(AG_LOCAL_RESET_CONFIRM,true);tick(102);assert(durable_reset.phase==AG_RESET_CLEAN&&erased_credentials==0);
    event(AG_LOCAL_OTA,true);tick(103);assert(bridge()->ota_armed&&ota_inits==1&&captured_trust.accepted_release_floor==0);
    assert(!bridge()->ota_options.allowed(bridge()->ota_options.context,NULL));
    bridge()->ota_options.closed(bridge()->ota_options.context,AG_OT_FAULT,false,false);tick(104);assert(!bridge()->ota_armed&&radio.advertise_allowed(radio.context));
    event(AG_LOCAL_RESET_ARM,false);tick(105);assert(durable_reset.phase==AG_RESET_JOURNAL_ARMED&&quiesces==1);
    event(AG_LOCAL_RESET_CONFIRM,true);tick(106);assert(erased_credentials==1&&durable_reset.phase==AG_RESET_CREDENTIALS_ERASED);
    tick(107);tick(108);tick(109);assert(durable_reset.phase==AG_RESET_CLEAN&&erased_configuration==1&&erased_logs==1);
    assert(!radio.advertise_allowed(radio.context));
}
int main(int argc,char **argv){
    assert(argc==2);scenario=argv[1];setup();
    if(!strcmp(scenario,"invalid"))profile.maximum_frame_ms=0;
    if(!strcmp(scenario,"entropy-fail"))entropy_failure=true;
    if(!strcmp(scenario,"entropy-zero"))zero_entropy=true;
    if(!strcmp(scenario,"prepare-fail"))prepare_failure=true;
    if(!strcmp(scenario,"absent")||!strcmp(scenario,"invalid")){
        assert(!ag_native_profile_start());assert(prepares==0&&entropy_calls==0&&partitions==0&&radio_starts==0&&power_calls==0);
    }else if(entropy_failure||zero_entropy||prepare_failure){
        assert(!ag_native_profile_start());assert(prepares==1&&partitions==0&&maintenance_binds==0&&radio_starts==0&&can_inits==0);assert(last_power==AG_GAUGE_POWER_UNAVAILABLE);
    }else if(!strcmp(scenario,"corrupt-boot")){
        assert(!ag_native_profile_start());assert(boot_binds==1&&radio_starts==0&&can_inits==0&&last_power==AG_GAUGE_POWER_UNAVAILABLE);
    }else if(!strncmp(scenario,"interrupted",11)){
        assert(!ag_native_profile_start()&&ag_native_profile_recovery_required());assert(radio_starts==0&&can_inits==0&&boot_binds==0);
        assert(ag_native_profile_recovery_tick()&&erased_credentials==0); // No automatic resume.
        usb=false;event(AG_LOCAL_RESET_ARM,true);assert(ag_native_profile_recovery_tick());assert(erased_credentials==0);
        usb=true;event(AG_LOCAL_RESET_ARM,false);assert(ag_native_profile_recovery_tick());assert(erased_credentials==0);
        event(AG_LOCAL_RESET_CONFIRM,false);assert(ag_native_profile_recovery_tick());assert(erased_credentials==0);
        event(AG_LOCAL_RESET_CONFIRM,true);
        if(!strcmp(scenario,"interrupted-credentials-fail")){
            assert(!ag_native_profile_recovery_tick()&&!ag_native_profile_recovery_required());
            assert(erased_credentials==1&&erased_configuration==0&&erased_logs==0);
            assert(!ag_native_profile_recovery_tick()&&erased_credentials==1); // No blind retry.
        }else{
            assert(ag_native_profile_recovery_tick());assert(erased_credentials==1);
            for(unsigned i=0;i<3;++i)assert(ag_native_profile_recovery_tick());
            assert(!ag_native_profile_recovery_required()&&durable_reset.phase==AG_RESET_CLEAN);
        }
        assert(radio_starts==0&&can_inits==0);
    }else if(!strcmp(scenario,"reset-radio-busy")){
        assert(ag_native_profile_start());tick(0);tick(100);
        radio_quiesce_result=false;event(AG_LOCAL_RESET_ARM,true);tick(101);
        assert(quiesces==1&&durable_reset.phase==AG_RESET_CLEAN&&erased_credentials==0);
        event(AG_LOCAL_RESET_CONFIRM,true);tick(102);assert(erased_credentials==0);
        radio_quiesce_result=true;event(AG_LOCAL_RESET_ARM,true);tick(103);
        event(AG_LOCAL_RESET_CONFIRM,true);tick(104);
        assert(quiesces==2&&erased_credentials==1&&durable_reset.phase==AG_RESET_CREDENTIALS_ERASED);
    }else if(!strcmp(scenario,"queued")){
        assert(ag_native_profile_start());tick(0);assert(can_port->pending&&!bridge()->endpoint_ref->initialized);
        tick(1000);assert(can_port->pending&&can_port->discovery->phase==AG_DISCOVERY_WAITING&&!bridge()->endpoint_ref->initialized);
    }else if(!strcmp(scenario,"render-timeout")){
        assert(ag_native_profile_start());row_delay_us=1000;assert(!radio.owner_tick(radio.context));
        radio.fault(radio.context,AG_NIMBLE_FAULT_OWNER);assert(last_power==AG_GAUGE_POWER_UNAVAILABLE&&rows<466);
    }else{assert(!strcmp(scenario,"good"));good();}
    unsigned before=prepares;assert(!ag_native_profile_start()&&prepares==before);puts("PASS native composition scenario");return 0;
}
