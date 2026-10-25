#include "ag_bench_profile.h"
#include <string.h>
static bool clock_ms(void *v,uint64_t *out){*out=((ag_bench_profile_t *)v)->now_ms;return true;}
static bool policy(void *v,ag_can_policy_t *out){*out=((ag_bench_profile_t *)v)->policy;return true;}
static bool row(void *v,uint16_t y,const uint16_t *pixels,size_t width){
    ag_bench_profile_t *b=v;b->pixel_digest^=y;
    for(size_t x=0;x<width;++x)b->pixel_digest=(b->pixel_digest^pixels[x])*16777619u;
    ++b->rendered_rows;return true;
}
static bool render(void *v,const ag_gauge_scene_t *scene){
    ag_bench_profile_t *b=v;b->rendered=*scene;++b->renders;
    bool safe=scene->widget_count==0||scene->power!=AG_GAUGE_POWER_ON;
    if(safe&&b->renderer.active&&ag_gauge_renderer_cancel_pending(&b->renderer)!=AG_RENDER_OK)return false;
    if(!b->renderer.active&&ag_gauge_renderer_begin(&b->renderer,scene)!=AG_RENDER_OK)return false;
    /* Normal frames finish their immutable snapshot before accepting the next
     * scene, preventing continuous y=0 restarts. Emergency/off scenes complete
     * the bounded RAM sink clear now, even if no further app tick will run. */
    do {
        ag_gauge_render_result_t r=ag_gauge_renderer_step(&b->renderer,16);
        if(r!=AG_RENDER_OK&&r!=AG_RENDER_DONE)return false;
    }while(safe&&b->renderer.active);
    return true;
}
static bool quiesce(void *v){(void)v;return true;}
static bool entropy(void *v,uint32_t *out){(void)v;(void)out;return false;}
static ag_bond_lookup_t lookup(void *v,const ag_client_id_t *id){(void)v;(void)id;return AG_BOND_LOOKUP_FAILED;}
static bool security(void *v,const ag_client_id_t *id,const ag_connection_lease_t *l,ag_link_security_t *out){(void)v;(void)id;(void)l;(void)out;return false;}
static bool bond(void *v,const ag_client_id_t *id,const ag_connection_lease_t *l){(void)v;(void)id;(void)l;return false;}
static ag_boot_load_t boot_read(void *v,uint8_t out[96]){ag_bench_profile_t *b=v;if(!b->boot_present)return AG_BOOT_LOAD_EMPTY;memcpy(out,b->boot_bytes,96);return AG_BOOT_LOAD_OK;}
static bool boot_write(void *v,const uint8_t bytes[96]){ag_bench_profile_t *b=v;memcpy(b->boot_bytes,bytes,96);b->boot_present=true;return true;}
static bool boot_observe(void *v,uint32_t size,ag_boot_observation_t *o){(void)v;(void)size;memset(o,0,sizeof(*o));o->running_address=o->boot_address=0x10000;o->state=AG_BOOT_IMAGE_VALID;return true;}
static bool target(void *v,uint32_t address,uint32_t size,const uint8_t hash[32]){(void)v;(void)address;(void)size;(void)hash;return false;}
static bool mark_valid(void *v){(void)v;return false;}
static bool can_step(void *v,ag_discovery_t *discovery,ag_runtime_t *runtime){
    ag_bench_profile_t *b=v;++b->can_steps;ag_simulated_dispatch_t dispatch;
    if(discovery){
        if(ag_discovery_prepare(discovery,&b->policy,b->now_ms)&&ag_discovery_dispatch(discovery,&b->policy,b->now_ms,&dispatch)){
            /* Deliberately tiny fixture ECU supports RPM and road speed only. */
            ag_can_frame_t frame={.id=dispatch.profile.response_id,.length=8,.data={6,0x41,0,0,0x18,0,0,0}};
            return ag_discovery_receive(discovery,&b->policy,&frame,b->now_ms)==AG_DISCOVERY_RX_ACCEPTED;
        }
    }else if(runtime){
        if(ag_runtime_prepare(runtime,&b->policy,b->now_ms)&&ag_runtime_dispatch(runtime,&b->policy,b->now_ms,&dispatch)){
            uint8_t pid=dispatch.request.bytes[1];
            ag_can_frame_t frame={.id=dispatch.profile.response_id,.length=8,.data={4,0x41,pid,0x1f,0x40}};
            if(pid==13){frame.data[0]=3;frame.data[3]=42;}
            return ag_runtime_receive(runtime,&b->policy,&frame,b->now_ms)==AG_SCHED_RX_ACCEPTED;
        }
    }else return false;
    return true;
}
bool ag_bench_profile_start(ag_bench_profile_t *b,ag_application_t *a,const uint8_t sid[16]){
    if(b==NULL||a==NULL||sid==NULL)return false;
    memset(b,0,sizeof(*b));
    ag_gauge_renderer_options_t renderer={.context=b,.write_row=row,.width=466,.height=466};
    if(!ag_gauge_renderer_init(&b->renderer,&renderer))return false;
    ag_pid_mask_t caps={0};ag_pid_mask_set(&caps,12);ag_pid_mask_set(&caps,13);
    ag_flash_sim_init(&b->config_flash);ag_config_flash_io_t cfg=ag_flash_sim_io(&b->config_flash);
    if(ag_config_store_open(&b->configuration,&cfg,&caps)!=AG_CONFIG_STORE_NO_VALID_CONFIG)return false;
    ag_config_t c={.schema_version=1,.revision=1,.dashboard_count=2,.manual_percent=50};
    for(size_t i=0;i<2;++i){c.dashboards[i].id=(int32_t)i+1;c.dashboards[i].widget_count=1;
        c.dashboards[i].widgets[0]=(ag_widget_t){1,(int32_t)i+12,{2000,2000,6000,6000}};}
    if(ag_config_store_provision_blank(&b->configuration,&c)!=AG_CONFIG_STORE_OK)return false;
    if(!ag_flash_history_sim_init(&b->log_flash,b->log_bytes,sizeof(b->log_bytes)))return false;
    ag_flash_history_io_t raw=ag_flash_history_sim_io(&b->log_flash);ag_journal_io_t log={raw.context,raw.read,raw.program,raw.erase_sector};
    if(ag_journal_open(&b->journal,&log,2)!=AG_JOURNAL_EMPTY)return false;
    uint8_t prior[16];memcpy(prior,sid,16);prior[0]^=0xff;if(!memcmp(prior,(uint8_t[16]){0},16))prior[1]=1;
    if(ag_journal_bootstrap(&b->journal,prior,1,1)!=AG_JOURNAL_OK)return false;
    if(!ag_connection_init(&b->connection,sid,NULL,NULL))return false;
    ag_reset_snapshot_t clean=ag_reset_clean_snapshot();
    if(!ag_factory_reset_restore(&b->reset,&b->connection,NULL,true,&clean))return false;
    ag_admission_config_t admission={60000,10000,1000,3};ag_admission_hooks_t hooks={b,entropy,lookup,security,bond};
    if(!ag_admission_init(&b->admission,&b->connection,&admission,&hooks))return false;
    ag_boot_hooks_t boot={.context=b,.read=boot_read,.write=boot_write,.observe=boot_observe,.target_matches=target,.mark_valid=mark_valid};
    if(ag_boot_health_open(&b->boot,&boot,AG_BOOT_HEALTH_REQUIRED_MIN)!=AG_BOOT_EMPTY||ag_boot_health_provision(&b->boot,0)!=AG_BOOT_OK)return false;
    b->policy=(ag_can_policy_t){.power=AG_POWER_OBD,.ignition=AG_IGN_QUALIFIED_ON,.hardware_validated=true,.rails_valid=true,.diagnostic_session_active=true};
    ag_application_options_t options={.configuration=&b->configuration,.journal=&b->journal,.admission=&b->admission,.boot=&b->boot,
        .device_id="SIMULATO-BANCO-001",.profile_count=1,.profiles={{AG_CAN_ID_11,500000,0x7e0,0x7e8}},
        .response_timeout_ms=100,.poll_interval_ms=200,.stale_after_ms=2000,.ui_options={2000,100},
        .hooks={.context=b,.clock_ms=clock_ms,.read_policy=policy,.can_step=can_step,.can_quiesce=quiesce,.render=render},.simulation=true};
    memcpy(options.session_id,sid,16);return ag_application_init(a,&options);
}
