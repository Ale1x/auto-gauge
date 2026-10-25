#include "ag_bench_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ag_bench_profile_t b;
static ag_application_t a;
static void start(void){uint8_t sid[16]={7};memset(&a,0,sizeof(a));assert(ag_bench_profile_start(&b,&a,sid));}
static void tick(uint64_t now){b.now_ms=now;assert(ag_application_tick(&a));}
static void test_integrated_flow(void){
    start();assert(a.phase==AG_APP_WAIT_POWER&&!a.runtime_started);tick(0);
    assert(a.phase==AG_APP_RUNNING&&a.runtime_started&&a.binding_count==2&&a.rpc.initialized);
    assert(a.rpc.options.simulation&&a.rpc.options.config_persistence==AG_RPC_VOLATILE);
    assert(a.runtime.persisted==1&&b.renders==1&&b.rendered.widgets[0].has_value);
    assert(b.rendered.widgets[0].value==2000&&a.runtime.scheduler.count==2);
    tick(100);assert(a.runtime.persisted==2);tick(200);assert(a.runtime.rate_limited==1);
    ag_history_snapshot_t out;assert(ag_journal_endpoint_read(&b.journal,a.options.session_id,0,&out)==AG_DEVICE_HISTORY_OK&&out.count==2);
    ag_history_sessions_page_t page;assert(ag_journal_endpoint_sessions(&b.journal,NULL,16,&page)==AG_DEVICE_HISTORY_OK);
    assert(ag_application_tap(&a,false)==AG_GAUGE_UI_DENIED);assert(ag_application_tap(&a,true)==AG_GAUGE_UI_OK);
    tick(201);assert(b.rendered.dashboard_id==2&&b.rendered.widgets[0].value==42);
    assert(ag_application_confirm_health(&a)==AG_BOOT_POLICY);
}
static bool fail_quiesce(void *v){(void)v;return false;}
static bool fail_render(void *v,const ag_gauge_scene_t *s){(void)v;(void)s;return false;}
static void test_guards_and_faults(void){
    assert(!ag_application_init(&a,NULL)&&a.phase==AG_APP_BLOCKED);assert(!ag_application_tick(&a));
    start();b.policy.power=AG_POWER_USB_ONLY;tick(0);assert(a.phase==AG_APP_WAIT_POWER&&b.can_steps==0);
    b.policy.power=AG_POWER_OBD;tick(100);size_t n=b.can_steps;b.policy.ignition=AG_IGN_OFF;tick(101);
    assert(b.can_steps==n&&b.rendered.power==AG_GAUGE_POWER_OFF&&!b.rendered.widgets[0].has_value);
    assert(ag_application_tap(&a,true)==AG_GAUGE_UI_DENIED);
    b.now_ms=99;assert(!ag_application_tick(&a)&&a.phase==AG_APP_FAULT);b.now_ms=102;assert(!ag_application_tick(&a));
    start();tick(0);a.options.hooks.can_quiesce=fail_quiesce;b.policy.bus_off=true;b.now_ms=1;
    assert(!ag_application_tick(&a)&&a.phase==AG_APP_FAULT);
    start();a.options.hooks.render=fail_render;assert(!ag_application_tick(&a)&&a.phase==AG_APP_FAULT);
}
static void test_reconfiguration_and_throttle(void){
    start();tick(0);tick(100);uint64_t generation=b.journal.info.generation,sequence=b.journal.info.next_sequence;
    ag_config_t c=b.configuration.active;c.revision=2;c.dashboard_count=1;c.dashboards[0].widgets[0].pid=13;
    assert(ag_config_store_commit(&b.configuration,1,&c)==AG_CONFIG_STORE_OK);tick(101);
    assert(a.runtime.scheduler.count==1&&a.configuration.revision==2&&a.runtime.rate_limited==1);
    assert(b.journal.info.generation==generation&&b.journal.info.next_sequence==sequence);
    c.revision=3;c.dashboards[0].widgets[0].pid=12;assert(ag_config_store_commit(&b.configuration,2,&c)==AG_CONFIG_STORE_OK);tick(102);
    assert(a.runtime.rate_limited==2&&b.journal.info.next_sequence==sequence);
    tick(1102);assert(a.runtime.persisted==3&&b.journal.info.next_sequence==sequence+1);
    assert(!memcmp(a.runtime.session_id,a.options.session_id,16));
}
static void test_exclusive(void){
    start();tick(0);tick(100);size_t steps=b.can_steps;
    assert(!ag_application_enter_exclusive(&a,false,true));assert(ag_application_enter_exclusive(&a,true,true));
    assert(ag_application_route_allowed(&a,true)&&!ag_application_route_allowed(&a,false));
    tick(200);assert(b.can_steps==steps&&a.phase==AG_APP_OTA);
    assert(ag_application_leave_exclusive(&a,true,true));tick(201);assert(a.phase==AG_APP_RUNNING&&b.can_steps>steps);
    /* Drain is one per tick; no session restart on mode exit. */
    tick(301);assert(ag_application_enter_exclusive(&a,true,false));assert(a.phase==AG_APP_MAINTENANCE);
    assert(!ag_application_leave_exclusive(&a,false,true)&&a.phase==AG_APP_FAULT);
}
static bool reentry_render(void *v,const ag_gauge_scene_t *s){(void)v;(void)s;assert(!ag_application_tick(&a));assert(!ag_application_enter_exclusive(&a,true,true));return true;}
static void test_reentry_and_stale_storage(void){
    start();a.options.hooks.render=reentry_render;tick(0);
    start();b.configuration.writable=false;ag_application_options_t o=a.options;
    assert(ag_application_init(&a,&o)&&a.phase==AG_APP_MAINTENANCE);
    assert(!ag_application_route_allowed(&a,false)&&!ag_application_route_allowed(&a,true));
    tick(100);assert(b.can_steps==0&&!ag_application_leave_exclusive(&a,true,true));
    b.configuration.writable=true;assert(ag_application_leave_exclusive(&a,true,true));tick(101);assert(a.phase==AG_APP_RUNNING);
    start();b.connection.maintenance_blocked=true;o=a.options;assert(!ag_application_init(&a,&o));
}
static ag_bond_lookup_t known(void *v,const ag_client_id_t *id){(void)v;(void)id;return AG_BOND_RECOGNIZED;}
static bool secured(void *v,const ag_client_id_t *id,const ag_connection_lease_t *l,ag_link_security_t *out){(void)v;(void)id;(void)l;*out=(ag_link_security_t){true,true,true,true,true,128,false};return true;}
static void test_rpc_owner(void){
    start();tick(0);b.admission.hooks.lookup=known;b.admission.hooks.read_security=secured;
    ag_client_id_t client={{5}};ag_connection_lease_t lease;
    assert(ag_admission_connect(&b.admission,&client,0,&lease)==AG_ADMISSION_OK);
    assert(ag_admission_security_changed(&b.admission,&lease,0)==AG_ADMISSION_OK);
    assert(ag_device_endpoint_attach(&a.rpc,&lease,0)==AG_DEVICE_OK);
    assert(!ag_application_enter_exclusive(&a,true,true));
    const char *json="{\"op\":\"logs.sessions\",\"limit\":16}";uint8_t wire[AG_FRAME_MAX_SIZE];size_t length=0;
    assert(ag_frame_encode(1,1,(const uint8_t *)json,strlen(json),wire,sizeof(wire),&length)==AG_FRAME_OK);
    assert(ag_device_endpoint_feed(&a.rpc,&lease,wire,length,0)==AG_DEVICE_OK);tick(1);
    assert(ag_device_endpoint_take_output(&a.rpc,&lease,1,wire,sizeof(wire),&length)==AG_DEVICE_OK);
    ag_frame_view_t view;assert(ag_frame_decode(wire,length,&view)==AG_FRAME_OK);
    char body[4097];memcpy(body,view.payload,view.payload_length);body[view.payload_length]=0;
    assert(strstr(body,"\"op\":\"logs.sessions\"")&&strstr(body,"07000000000000000000000000000000"));
    b.configuration.writable=false;b.now_ms=2;
    assert(!ag_application_tick(&a)&&a.phase==AG_APP_FAULT);
}
static uint32_t incomplete_health(void *v){(void)v;return AG_BOOT_HEALTH_REQUIRED_MIN&~1u;}
static unsigned marks;
static bool count_mark(void *v){(void)v;++marks;return false;}
static bool failed_observe(void *v,uint32_t size,ag_boot_observation_t *out){(void)v;(void)size;(void)out;return false;}
static void test_health_and_safe_scene(void){
    start();tick(0);a.options.simulation=false;a.options.hooks.health_mask=incomplete_health;b.boot.hooks.mark_valid=count_mark;
    marks=0;uint64_t generation=b.boot.generation;
    assert(ag_application_confirm_health(&a)==AG_BOOT_POLICY&&marks==0&&b.boot.generation==generation);
    b.boot.hooks.observe=failed_observe;
    assert(!ag_application_enter_exclusive(&a,true,true)&&a.phase==AG_APP_FAULT);
    assert(b.rendered.widget_count==0&&b.rendered.power==AG_GAUGE_POWER_UNAVAILABLE);
    size_t renders=b.renders;assert(!ag_application_tick(&a)&&b.renders==renders);
    start();tick(0);b.configuration.active.revision=2;b.configuration.active.dashboards[0].widgets[0].pid=99;
    tick(1);assert(a.phase==AG_APP_CONFIG_REQUIRED&&a.transport_quiesced&&b.rendered.widget_count==0);
}
static ag_config_flash_io_t original_io;
static size_t committed_error(void *v,uint32_t offset,const uint8_t *bytes,size_t length){
    size_t written=original_io.program(v,offset,bytes,length);
    return offset%AG_CONFIG_SLOT_BYTES==AG_CONFIG_COMMIT_OFFSET&&written==4?0:written;
}
static void test_config_commit_after_effect(void){
    start();tick(0);size_t steps=b.can_steps;
    original_io=b.configuration.io;b.configuration.io.program=committed_error;
    ag_config_t candidate=b.configuration.active;candidate.revision=2;candidate.manual_percent=75;
    assert(ag_config_store_commit(&b.configuration,1,&candidate)==AG_CONFIG_STORE_IO_ERROR);
    assert(!b.configuration.ready&&b.configuration.active.revision==1);
    b.now_ms=1;assert(!ag_application_tick(&a)&&a.phase==AG_APP_FAULT);
    assert(b.can_steps==steps&&b.rendered.widget_count==0);
    b.configuration.io=original_io;ag_flash_sim_reboot(&b.config_flash);
    assert(ag_config_store_restore(&b.configuration)==AG_CONFIG_STORE_OK&&b.configuration.active.revision==2);
    /* Explicit reconciliation proves commit occurred; owner never resumes an
     * ambiguous prior session automatically or replays the write. */
    assert(!ag_application_tick(&a));
}
static void test_incremental_render_and_safe_completion(void){
    start();tick(0);assert(b.renderer.active&&b.renderer.next_y==16&&b.rendered_rows==16);
    for(uint64_t now=1;now<30;++now)tick(now);
    assert(!b.renderer.active&&b.rendered_rows==466);
    tick(30);assert(b.renderer.active&&b.renderer.next_y==16);
    b.policy.ignition=AG_IGN_OFF;tick(31);
    assert(!b.renderer.active&&b.rendered_rows==466+16+466);
    assert(b.rendered.power==AG_GAUGE_POWER_OFF);
}
static bool queued_request;
static bool is_queued(void *v){(void)v;return queued_request;}
static bool queue_discovery(void *v,ag_discovery_t *d,ag_runtime_t *r){
    ag_bench_profile_t *profile=v;(void)r;
    if(d&&d->phase==AG_DISCOVERY_READY){ag_simulated_dispatch_t dispatch;
        assert(ag_discovery_prepare(d,&profile->policy,profile->now_ms));
        assert(ag_discovery_dispatch(d,&profile->policy,profile->now_ms,&dispatch));queued_request=true;
    }
    return true;
}
static void test_queued_request_does_not_start_response_deadline(void){
    start();queued_request=false;a.options.hooks.can_step=queue_discovery;a.options.hooks.can_request_pending=is_queued;
    tick(0);assert(a.discovery.phase==AG_DISCOVERY_WAITING&&queued_request);
    tick(1000);assert(a.phase==AG_APP_DISCOVERING&&a.discovery.phase==AG_DISCOVERY_WAITING);
    assert(a.discovery.current==0); /* Response timeout is100ms, TX is still queued. */
}
int main(void){test_integrated_flow();test_guards_and_faults();test_reconfiguration_and_throttle();test_exclusive();test_reentry_and_stale_storage();test_rpc_owner();test_health_and_safe_scene();test_config_commit_after_effect();test_incremental_render_and_safe_completion();test_queued_request_does_not_start_response_deadline();puts("PASS: 10 application integration groups, volatile simulated peripherals only");return 0;}
