#include "native_profile.h"
#include "boot_storage.h"
#include "endpoint_bridge.h"
#include "maintenance_store.h"
#include "transfer_adapter.h"
#include "esp_timer.h"
#include <string.h>

static struct {
    ag_native_board_profile_t profile;
    ag_application_t app;
    ag_connection_t connection;
    ag_admission_t admission;
    ag_config_store_t configuration;
    ag_journal_history_t journal;
    ag_partition_store_t partitions;
    ag_factory_reset_t reset;
    ag_maintenance_store_t maintenance;
    ag_boot_health_t boot;
    ag_ota_boot_storage_t boot_storage;
    ag_twai_port_t can;
    ag_nimble_bridge_t bridge;
    ag_ota_transfer_adapter_t ota;
    ag_gauge_renderer_t renderer;
    uint8_t sid[16];
    bool attempted,failed,ota_closed,ota_cleanup_unknown,ota_selection_changed,radio_quiesced,recovery_active;
} native;

__attribute__((weak)) const ag_native_board_profile_t *ag_native_board_profile(void) { return NULL; }
static uint64_t now_ms(void) { int64_t us=esp_timer_get_time(); return us<0?UINT64_MAX:(uint64_t)us/1000u; }
static bool clock_ms(void *unused,uint64_t *out) { (void)unused; *out=now_ms(); return *out!=UINT64_MAX; }
static uint64_t ota_clock(void *unused) { (void)unused; return now_ms(); }
static bool policy(void *unused,ag_can_policy_t *out) {
    (void)unused; ag_can_policy_init(out); return native.profile.read_policy(native.profile.context,out);
}
static bool good_can_result(ag_twai_result_t r) {
    return r==AG_TWAI_OK || r==AG_TWAI_NO_WORK || r==AG_TWAI_QUEUED
        || r==AG_TWAI_COMPLETED || r==AG_TWAI_BUSY;
}
static bool request_pending(void *unused) {
    (void)unused; return native.can.pending && native.can.tx_bytes[0]==2 && native.can.tx_bytes[1]==1;
}
static bool quiesce(void *unused) {
    (void)unused;
    if(!native.can.initialized || native.can.phase==AG_TWAI_INITIALIZED || native.can.phase==AG_TWAI_SUSPENDED) return true;
    return ag_twai_port_suspend(&native.can)==AG_TWAI_OK;
}
static bool can_step(void *unused,ag_discovery_t *discovery,ag_runtime_t *runtime) {
    (void)unused; ag_twai_result_t result;
    if(native.can.phase==AG_TWAI_INITIALIZED) result=ag_twai_port_start(&native.can);
    else if(native.can.phase==AG_TWAI_SUSPENDED) result=ag_twai_port_resume(&native.can);
    else result=ag_twai_port_poll(&native.can);
    if(!good_can_result(result)) return false;
    if(discovery) return good_can_result(ag_twai_port_send_discovery(&native.can,discovery));
    if(runtime==NULL) return false;
    result=ag_twai_port_send_flow_control(&native.can,runtime);
    if(!good_can_result(result)) return false;
    if(result==AG_TWAI_QUEUED || result==AG_TWAI_BUSY) return true;
    return good_can_result(ag_twai_port_send_runtime(&native.can,runtime));
}
static bool row(void *unused,uint16_t y,const uint16_t *pixels,size_t width) {
    (void)unused; return native.profile.write_row(native.profile.context,y,pixels,width);
}
static bool render(void *unused,const ag_gauge_scene_t *scene) {
    (void)unused;
    if(!native.profile.display_power(native.profile.context,scene->power,scene->brightness,scene->brightness_percent)) return false;
    if(scene->power!=AG_GAUGE_POWER_ON)
        return ag_gauge_renderer_cancel_pending(&native.renderer)==AG_RENDER_OK;
    if(ag_gauge_renderer_begin(&native.renderer,scene)!=AG_RENDER_OK) return false;
    const uint64_t started=now_ms();
    if(started==UINT64_MAX) return false;
    /* Complete this projection before returning to acquisition: no multi-tick
     * old snapshot may overwrite a newer dashboard or unavailable quality.
     * The board's synchronous sink must obey its qualified per-row bound. */
    while(native.renderer.active) {
        uint64_t now=now_ms();
        if(now<started || now-started>=native.profile.maximum_frame_ms) return false;
        ag_gauge_render_result_t r=ag_gauge_renderer_step(&native.renderer,AG_GAUGE_RENDER_ROWS_PER_STEP_MAX);
        if(r!=AG_RENDER_OK && r!=AG_RENDER_DONE) return false;
    }
    uint64_t finished=now_ms();
    return finished>=started && finished-started<native.profile.maximum_frame_ms;
}
static bool als(void *unused,uint8_t *percent) {
    (void)unused; return native.profile.als_percent!=NULL && native.profile.als_percent(native.profile.context,percent);
}
static uint32_t health(void *unused) {
    (void)unused;
    /* Display health cannot be claimed before at least one complete frame. */
    if(native.renderer.active || native.renderer.faulted || native.renderer.next_y!=native.profile.panel_height) return 0;
    return native.profile.health_mask(native.profile.context);
}
static bool awake(void *unused) { (void)unused; return !native.failed && native.profile.radio_allowed(native.profile.context); }
static bool route(void *unused,bool ota) { (void)unused; return !native.failed && ag_application_route_allowed(&native.app,ota); }
static bool entropy(void *unused,uint32_t *out) { (void)unused; return native.profile.random_u32(native.profile.context,out); }
static bool display_passkey(void *unused,const char digits[7]) {
    (void)unused; return native.profile.display_passkey(native.profile.context,digits);
}
static void clear_passkey(void *unused) { (void)unused; native.profile.clear_passkey(native.profile.context); }
static bool startup_failed(void) {
    if(native.failed) return false;
    native.failed=true;
    if(native.can.initialized) (void)ag_twai_port_close(&native.can);
    (void)native.profile.display_power(native.profile.context,AG_GAUGE_POWER_UNAVAILABLE,AG_GAUGE_BRIGHTNESS_UNAVAILABLE,0);
    return false;
}
static void native_fault(void *unused,ag_nimble_fault_t reason) {
    (void)unused; (void)reason; (void)startup_failed();
}
/* Until bridge binds actual stack hooks, all admission evidence is denied. */
static ag_bond_lookup_t deny_lookup(void *c,const ag_client_id_t *id) { (void)c;(void)id;return AG_BOND_LOOKUP_FAILED; }
static bool deny_security(void *c,const ag_client_id_t *id,const ag_connection_lease_t *l,ag_link_security_t *o) {
    (void)c;(void)id;(void)l;(void)o;return false;
}
static bool deny_bond(void *c,const ag_client_id_t *id,const ag_connection_lease_t *l) { (void)c;(void)id;(void)l;return false; }
static bool credentials(void *unused) {
    (void)unused;
    return native.radio_quiesced && native.profile.erase_credentials!=NULL && native.connection.state==AG_CONNECTION_IDLE
        && native.profile.erase_credentials(native.profile.context);
}
static bool ota_allowed(void *unused,uint64_t lease) {
    (void)unused;
    return !native.failed && native.app.phase==AG_APP_OTA && native.bridge.has_lease
        && native.bridge.lease.counter==lease && awake(NULL)
        && ag_admission_private_allowed(&native.admission,&native.bridge.lease,now_ms())
        && native.profile.ota_allowed(native.profile.context,&native.bridge.lease);
}
static bool lease_allowed(void *unused,const ag_connection_lease_t *lease) {
    (void)unused;
    return lease!=NULL && memcmp(lease->boot_id,native.sid,16)==0 && ota_allowed(NULL,lease->counter);
}
static void ota_closed(void *unused,ag_ot_phase_t phase,bool cleanup_unknown,bool selection_changed) {
    (void)unused;(void)phase; native.ota_closed=true;
    native.ota_cleanup_unknown=cleanup_unknown; native.ota_selection_changed=selection_changed;
}
static bool enter_ota(void) {
    if(native.ota.initialized || !ag_application_enter_exclusive(&native.app,true,true)) return false;
    ag_manifest_trust_t trust=native.profile.trust;
    if(!ag_boot_health_floor(&native.boot,&trust.accepted_release_floor)) return false;
    ag_ota_transfer_platform_t platform={.context=&native,.maintenance_allowed=ota_allowed,.clock_ms=ota_clock,
        .idle_timeout_ms=native.profile.ota_idle_timeout_ms,.total_timeout_ms=native.profile.ota_total_timeout_ms,
        .boot_context=&native.boot,.prepare_selection=ag_ota_boot_prepare_selection,
        .allow_factory_bootstrap=native.profile.allow_factory_bootstrap};
    if(!ag_ota_transfer_adapter_init(&native.ota,&trust,&platform)) return false;
    ag_nimble_ota_options_t options={.context=&native,.allowed=lease_allowed,.closed=ota_closed,
        .frame_timeout_ms=2000,.request_timeout_ms=10000};
    return ag_nimble_bridge_prepare_ota(&native.bridge,&native.ota.transfer,&options);
}
static bool local_event(void) {
    ag_native_local_event_t event={0};
    if(!native.profile.local_event(native.profile.context,&event) || event.kind==AG_LOCAL_NONE) return true;
    if(!event.physically_qualified) return true;
    switch(event.kind) {
        case AG_LOCAL_TAP: (void)ag_application_tap(&native.app,true); return true;
        case AG_LOCAL_PAIR: (void)ag_nimble_bridge_pairing_window(&native.bridge,true,now_ms()); return true;
        case AG_LOCAL_OTA:
            /* Local rejection (busy/not-idle) is not a transfer retry. */
            if(native.connection.state!=AG_CONNECTION_IDLE || native.app.runtime.count || native.ota.initialized
                ||native.boot.pending || (native.app.phase!=AG_APP_RUNNING && native.app.phase!=AG_APP_CONFIG_REQUIRED)) return true;
            return enter_ota();
        case AG_LOCAL_RESET_ARM: {
            if(native.profile.erase_credentials==NULL || native.connection.state!=AG_CONNECTION_IDLE
                || native.app.runtime.count || !native.profile.usb_power_valid(native.profile.context)) return true;
            if(native.app.phase!=AG_APP_MAINTENANCE && !ag_application_enter_exclusive(&native.app,true,false)) return true;
            native.radio_quiesced=ag_nimble_transport_quiesce();
            if(!native.radio_quiesced) return true;
            ag_local_reset_event_t reset={AG_RESET_ORIGIN_LOCAL_PHYSICAL,true,
                native.profile.usb_power_valid(native.profile.context),event.confirmed};
            return ag_factory_reset_arm(&native.reset,&reset)==AG_RESET_OK;
        }
        case AG_LOCAL_RESET_CONFIRM: {
            ag_local_reset_event_t reset={AG_RESET_ORIGIN_LOCAL_PHYSICAL,true,
                native.profile.usb_power_valid(native.profile.context),event.confirmed};
            if(ag_factory_reset_confirm(&native.reset,&reset)==AG_RESET_OK) {
                native.configuration.ready=native.configuration.writable=false;
                native.journal.ready=native.journal.writable=false;
            }
            return true;
        }
        default: return true;
    }
}
static bool owner_tick(void *unused) {
    (void)unused;
    if(native.failed) return false;
    /* Pump a queued physical TX before any operation can reconfigure its core.
     * No application transition is made until completion is observed. */
    if(native.can.phase==AG_TWAI_RUNNING && native.can.pending) {
        if(!good_can_result(ag_twai_port_poll(&native.can))) return false;
        if(native.can.pending) {
            /* FC completion must not suspend the original response deadline.
             * Avoid reconfiguration/local mode changes until physical TX ends. */
            if(!request_pending(NULL) && native.app.runtime_started) {
                ag_can_policy_t p;
                if(!policy(NULL,&p) || !ag_runtime_tick(&native.app.runtime,&p,now_ms())) return false;
            }
            return true;
        }
    }
    if(!local_event()) return false;
    if(native.reset.state==AG_RESET_IN_PROGRESS) {
        if(ag_factory_reset_step(&native.reset,native.profile.usb_power_valid(native.profile.context))!=AG_RESET_OK) return false;
        return true; /* Erased stores require explicit commissioning, no defaults. */
    }
    if(native.ota_closed && native.connection.state==AG_CONNECTION_IDLE) {
        native.ota_closed=false;
        if(!ag_application_leave_exclusive(&native.app,!native.ota_cleanup_unknown,!native.ota_selection_changed)) return false;
        if(!ag_nimble_bridge_prepare_rpc(&native.bridge)) return false;
    }
    if(!ag_application_tick(&native.app)) return false;
    if(native.boot.pending && native.app.phase==AG_APP_RUNNING) {
        ag_boot_result_t result=ag_application_confirm_health(&native.app);
        if(result!=AG_BOOT_OK && result!=AG_BOOT_POLICY && result!=AG_BOOT_NEEDS_HEALTH) return false;
    }
    return true;
}
static bool valid_profile(const ag_native_board_profile_t *p) {
    if(p==NULL || p->device_id==NULL || p->nvs_label==NULL || p->prepare==NULL || p->read_policy==NULL
        ||p->radio_allowed==NULL || p->usb_power_valid==NULL || p->random_u32==NULL || p->local_event==NULL
        ||p->display_power==NULL || p->write_row==NULL || p->display_passkey==NULL || p->clear_passkey==NULL
        ||p->health_mask==NULL || p->ota_allowed==NULL || p->trust.lookup_key==NULL
        ||!p->can.hardware_profile_qualified || p->ecu_count==0 || p->ecu_count>AG_DISCOVERY_ECU_MAX
        ||p->panel_width<32 || p->panel_height<32 || p->panel_width>466 || p->panel_height>466
        ||p->maximum_frame_ms==0 || p->maximum_frame_ms>1000
        ||(p->required_health&AG_BOOT_HEALTH_REQUIRED_MIN)!=AG_BOOT_HEALTH_REQUIRED_MIN
        ||(p->required_health&~AG_BOOT_HEALTH_KNOWN)!=0) return false;
    for(size_t i=0;i<p->ecu_count;++i)
        if(!ag_can_profile_valid(&p->ecus[i]) || p->ecus[i].bitrate!=p->can.bitrate || p->ecus[i].id_format!=p->can.id_format) return false;
    return true;
}
bool ag_native_profile_recovery_required(void) { return native.recovery_active && !native.failed; }
bool ag_native_profile_recovery_tick(void) {
    if(!ag_native_profile_recovery_required()) return false;
    ag_native_local_event_t event={0};
    if(native.profile.local_event(native.profile.context,&event) && event.physically_qualified
        && native.profile.usb_power_valid(native.profile.context)) {
        ag_local_reset_event_t reset={AG_RESET_ORIGIN_LOCAL_PHYSICAL,true,true,event.confirmed};
        if(event.kind==AG_LOCAL_RESET_ARM && native.reset.state==AG_RESET_FAILED) {
            if(ag_factory_reset_arm(&native.reset,&reset)!=AG_RESET_OK) return startup_failed();
        } else if(event.kind==AG_LOCAL_RESET_CONFIRM && native.reset.state==AG_RESET_ARMED && event.confirmed) {
            if(ag_factory_reset_confirm(&native.reset,&reset)!=AG_RESET_OK) return startup_failed();
        }
    }
    if(native.reset.state==AG_RESET_IN_PROGRESS
        && ag_factory_reset_step(&native.reset,native.profile.usb_power_valid(native.profile.context))!=AG_RESET_OK)
        return startup_failed();
    if(native.reset.state==AG_RESET_IDLE) native.recovery_active=false;
    return true;
}
bool ag_native_profile_start(void) {
    if(native.attempted) return false;
    native.attempted=true;
    const ag_native_board_profile_t *profile=ag_native_board_profile();
    if(!valid_profile(profile)) return false;
    native.profile=*profile;
    if(!profile->prepare(profile->context)) return startup_failed();
    for(unsigned word=0;word<4;++word) {
        uint32_t value;
        if(!profile->random_u32(profile->context,&value)) return startup_failed();
        for(unsigned byte=0;byte<4;++byte)native.sid[4*word+byte]=(uint8_t)(value>>(8*byte));
    }
    bool sid=false;for(size_t i=0;i<16;++i)sid|=native.sid[i]!=0;if(!sid) return startup_failed();
    if(!ag_partition_store_bind(&native.partitions,&profile->partitions)) return startup_failed();
    ag_pid_mask_t supported={0};for(unsigned pid=0;pid<256;++pid)if(ag_obd_pid_implemented((uint8_t)pid))ag_pid_mask_set(&supported,(uint8_t)pid);
    (void)ag_config_store_open(&native.configuration,&native.partitions.config_io,&supported);
    (void)ag_journal_open(&native.journal,&native.partitions.history_io,profile->partitions.history_sectors);
    if(!ag_connection_init(&native.connection,native.sid,NULL,NULL)) return startup_failed();
    ag_reset_snapshot_t snapshot;ag_reset_io_t reset_io;
    if(!ag_maintenance_store_bind(&native.maintenance,profile->nvs_label,&native.partitions,&native,
        credentials,&snapshot,&reset_io) || !ag_factory_reset_restore(&native.reset,&native.connection,&reset_io,true,&snapshot)) return startup_failed();
    /* An interrupted reset never reaches radio/OTA/runtime. The bootstrap
     * task may poll the separate local-only recovery API until completion. */
    if(native.connection.maintenance_blocked) {
        if(profile->erase_credentials==NULL) return startup_failed();
        native.radio_quiesced=true; /* No radio API was invoked in this boot. */
        native.recovery_active=true;
        if(!profile->display_power(profile->context,AG_GAUGE_POWER_OFF,AG_GAUGE_BRIGHTNESS_UNAVAILABLE,0)) return startup_failed();
        return false;
    }
    ag_boot_hooks_t boot_hooks;
    if(!ag_ota_boot_storage_bind(&native.boot_storage,profile->nvs_label,&boot_hooks)) return startup_failed();
    boot_hooks.allow_factory_bootstrap=profile->allow_factory_bootstrap;
    ag_boot_result_t boot=ag_boot_health_open(&native.boot,&boot_hooks,profile->required_health);
    if(boot!=AG_BOOT_OK && boot!=AG_BOOT_NEEDS_HEALTH && boot!=AG_BOOT_RECOVERED_FALLBACK) return startup_failed();
    ag_admission_hooks_t admission={&native,entropy,deny_lookup,deny_security,deny_bond};
    if(!ag_admission_init(&native.admission,&native.connection,&profile->admission,&admission)) return startup_failed();
    ag_twai_port_config_t can=profile->can;can.policy_context=&native;can.read_policy=policy;
    if(!ag_twai_port_init(&native.can,&can)) return startup_failed();
    ag_gauge_renderer_options_t renderer={&native,row,profile->panel_width,profile->panel_height};
    if(!ag_gauge_renderer_init(&native.renderer,&renderer)) return startup_failed();
    ag_application_options_t options={.configuration=&native.configuration,.journal=&native.journal,
        .admission=&native.admission,.boot=&native.boot,.device_id=profile->device_id,.profile_count=profile->ecu_count,
        .response_timeout_ms=profile->response_timeout_ms,.poll_interval_ms=profile->poll_interval_ms,
        .stale_after_ms=profile->stale_after_ms,.ui_options=profile->ui,
        .hooks={.context=&native,.clock_ms=clock_ms,.read_policy=policy,.can_step=can_step,.can_quiesce=quiesce,
            .render=render,.als_percent=als,.health_mask=health,.can_request_pending=request_pending},.allow_degraded_logging=profile->allow_degraded_logging,
        .rpc_externally_pumped=true};
    memcpy(options.session_id,native.sid,16);memcpy(options.profiles,profile->ecus,profile->ecu_count*sizeof(profile->ecus[0]));
    if(!ag_application_init(&native.app,&options)) return startup_failed();
    ag_nimble_platform_t platform={.context=&native,.awake_allowed=awake,.route_allowed=route,.random_u32=entropy,
        .display_passkey=display_passkey,.clear_passkey=clear_passkey,.fault=native_fault,.owner_tick=owner_tick};
    if(!ag_nimble_bridge_init_bound(&native.bridge,&native.connection,&native.admission,&native.app.rpc,&platform)) return startup_failed();
    if(ag_nimble_bridge_start(&native.bridge)!=ESP_OK) return startup_failed();
    return true;
}
