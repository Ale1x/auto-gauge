#include "ag_application.h"
#include <string.h>
static bool fault(ag_application_t *a){
    a->phase=AG_APP_FAULT;memset(&a->scene,0,sizeof(a->scene));
    if(!a->fault_cleanup_attempted){
        a->fault_cleanup_attempted=true;
        /* A failed/ambiguous stop is never repeated. The safe display command
         * is distinct from a failed value frame; no recursive owner entry. */
        if(!a->transport_quiesced&&!a->quiesce_attempted&&a->options.hooks.can_quiesce){
            a->quiesce_attempted=true;
            a->transport_quiesced=a->options.hooks.can_quiesce(a->options.hooks.context);
        }
        if(a->options.hooks.render)(void)a->options.hooks.render(a->options.hooks.context,&a->scene);
    }
    return false;
}
static bool fresh(ag_application_t *a,uint64_t *now,ag_can_policy_t *p){
    if(!a->options.hooks.clock_ms(a->options.hooks.context,now)||(a->has_time&&*now<a->last_ms))return fault(a);
    a->last_ms=*now;a->has_time=true;ag_can_policy_init(p);
    if(!a->options.hooks.read_policy(a->options.hooks.context,p))return fault(a);
    return true;
}
static bool quiesce(ag_application_t *a){
    a->quiesce_attempted=true;
    if(!a->options.hooks.can_quiesce(a->options.hooks.context))return fault(a);
    a->transport_quiesced=true;return true;
}
static bool connection_idle(const ag_application_t *a){return a->options.admission->connection->state==AG_CONNECTION_IDLE&&!a->rpc.attached;}
static bool jobs(ag_application_t *a,const ag_config_t *c,ag_runtime_job_t out[AG_POLL_JOB_CAPACITY],size_t *count){
    *count=0;ag_pid_mask_t seen={0};
    for(size_t d=0;d<c->dashboard_count;++d)for(size_t w=0;w<c->dashboards[d].widget_count;++w){
        uint8_t pid=(uint8_t)c->dashboards[d].widgets[w].pid;if(ag_pid_mask_has(&seen,pid))continue;
        if(*count==AG_POLL_JOB_CAPACITY)return false;
        size_t ecu=a->discovery.count;
        /* Explicit deterministic binding policy: first candidate supplied by
         * local profile with usable advertised PID; no wire identity guessing. */
        for(size_t i=0;i<a->discovery.count;++i){ag_pid_mask_t usable;ag_obd_usable_pids(&a->discovery.ecus[i].capabilities,&usable);
            if(ag_pid_mask_has(&usable,pid)){ecu=i;break;}}
        if(ecu==a->discovery.count)return false;
        out[*count]=(ag_runtime_job_t){.job={pid,a->discovery.ecus[ecu].profile,a->options.poll_interval_ms,
            a->options.response_timeout_ms,a->options.stale_after_ms},.capabilities=a->discovery.ecus[ecu].capabilities};
        ag_pid_mask_set(&seen,pid);++*count;
    }
    return *count>0;
}
static bool configure(ag_application_t *a){
    if(!a->options.configuration->ready||!a->options.configuration->writable)return fault(a);
    ag_config_t c;ag_runtime_job_t plan[AG_POLL_JOB_CAPACITY];size_t count;
    if(!ag_config_store_get(a->options.configuration,&c)||ag_config_validate(&c,&a->supported)!=AG_CONFIG_OK){
        if(!quiesce(a))return false;
        memset(&a->scene,0,sizeof(a->scene));
        a->phase=AG_APP_CONFIG_REQUIRED;
        if(!a->options.hooks.render(a->options.hooks.context,&a->scene))return fault(a);
        return true;
    }
    if(!jobs(a,&c,plan,&count))return fault(a);
    if(a->runtime_started){
        if(!quiesce(a)||!ag_runtime_reconfigure(&a->runtime,&c,plan,count))return fault(a);
    }else{
        ag_runtime_sink_t sink={a->options.journal,ag_runtime_journal_begin,ag_runtime_journal_append};
        if(!ag_runtime_init(&a->runtime,&c,plan,count,a->options.session_id,&sink))return fault(a);
        if(a->options.allow_degraded_logging&&!ag_runtime_allow_degraded_logging(&a->runtime))return fault(a);
        if(!ag_runtime_start(&a->runtime))return fault(a);
        a->runtime_started=true;
    }
    a->binding_count=count;
    for(size_t i=0;i<count;++i)a->bindings[i]=(ag_gauge_binding_t){plan[i].job.pid,plan[i].job.profile.response_id};
    a->configuration=c;a->phase=AG_APP_RUNNING;return true;
}
static bool finish_discovery(ag_application_t *a){
    if(!ag_discovery_usable(&a->discovery,&a->supported))return fault(a);
    a->options.configuration->supported=a->supported;
    /* Re-evaluate compatibility against actual discovered capabilities; no IO
     * rewrite or default provisioning when an existing config is incompatible. */
    a->options.configuration->compatible=ag_config_validate(&a->options.configuration->active,&a->supported)==AG_CONFIG_OK;
    ag_device_endpoint_options_t opts={.history_context=a->options.journal,.history_read=ag_journal_endpoint_read,
        .history_sessions=ag_journal_endpoint_sessions,.device_id=a->options.device_id,.supported=a->supported,
        .config_persistence=a->options.simulation?AG_RPC_VOLATILE:AG_RPC_DURABLE,.frame_timeout_ms=2000,
        .request_timeout_ms=10000,.simulation=a->options.simulation};
    if(!ag_device_endpoint_init(&a->rpc,a->options.admission,a->options.configuration,&opts))return fault(a);
    return configure(a);
}
bool ag_application_init(ag_application_t *a,const ag_application_options_t *o){
    if(a==NULL)return false;
    memset(a,0,sizeof(*a));a->phase=AG_APP_BLOCKED;
    if(o==NULL||o->configuration==NULL||o->journal==NULL||o->admission==NULL||o->boot==NULL
        ||o->device_id==NULL||o->hooks.clock_ms==NULL||o->hooks.read_policy==NULL||o->hooks.can_step==NULL
        ||o->hooks.can_quiesce==NULL||o->hooks.render==NULL||o->poll_interval_ms==0||o->response_timeout_ms==0
        ||o->stale_after_ms==0||!o->configuration->initialized||!o->journal->initialized
        ||!o->admission->initialized||o->admission->connection==NULL
        ||!o->admission->connection->boot_journal_loaded||o->admission->connection->maintenance_blocked
        ||o->admission->connection->state!=AG_CONNECTION_IDLE)return false;
    bool sid=false;for(size_t i=0;i<16;++i)sid|=o->session_id[i]!=0;if(!sid)return false;
    uint64_t floor;if(!ag_boot_health_floor(o->boot,&floor))return false;
    a->options=*o;
    if(!ag_gauge_ui_init(&a->ui,&o->ui_options)||!ag_discovery_init(&a->discovery,o->profiles,o->profile_count,o->response_timeout_ms))return false;
    ag_boot_result_t boot=ag_boot_health_reconcile(o->boot);
    if(boot!=AG_BOOT_OK&&boot!=AG_BOOT_NEEDS_HEALTH&&boot!=AG_BOOT_RECOVERED_FALLBACK)return false;
    a->initialized=true;a->phase=AG_APP_WAIT_POWER;a->transport_quiesced=true;
    if(!o->configuration->ready||!o->configuration->writable||!o->journal->ready
        ||(!o->journal->writable&&!o->allow_degraded_logging)){
        a->commissioning_required=true;a->phase=AG_APP_MAINTENANCE;
    }
    return true;
}
static bool project(ag_application_t *a,uint64_t now,const ag_can_policy_t *p){
    ag_measurement_t readings[AG_POLL_JOB_CAPACITY];
    for(size_t i=0;i<a->runtime.scheduler.count;++i)readings[i]=a->runtime.scheduler.jobs[i].sample;
    uint8_t percent=0;bool als=false;
    if(a->options.hooks.als_percent)als=a->options.hooks.als_percent(a->options.hooks.context,&percent);
    /* ALS hook may perform IO: refresh both time and power before projection. */
    ag_can_policy_t policy=*p;if(!fresh(a,&now,&policy))return false;
    ag_gauge_input_t input={.configuration=&a->configuration,.supported=&a->supported,
        .bindings=a->bindings,.binding_count=a->binding_count,.readings=readings,.reading_count=a->runtime.scheduler.count,
        .power_policy=&policy,.now_ms=now,.als_valid=als,.als_percent=percent};
    if(ag_gauge_ui_project(&a->ui,&input,&a->scene)!=AG_GAUGE_UI_OK)return fault(a);
    if(!a->options.hooks.render(a->options.hooks.context,&a->scene))return fault(a);
    return fresh(a,&now,&policy);
}
bool ag_application_tick(ag_application_t *a){
    if(a==NULL||!a->initialized||a->busy||a->phase==AG_APP_FAULT)return false;
    a->busy=true;bool ok=false;uint64_t now;ag_can_policy_t p;
    if(!fresh(a,&now,&p))goto done;
    (void)ag_admission_tick(a->options.admission,now);
    if(a->options.admission->clock_fault){fault(a);goto done;}
    if(a->phase==AG_APP_OTA||a->phase==AG_APP_MAINTENANCE){ok=true;goto done;}
    if(a->phase==AG_APP_WAIT_POWER){
        if(!ag_can_tx_allowed(&p)){ok=true;goto done;}
        if(!ag_discovery_init(&a->discovery,a->options.profiles,a->options.profile_count,a->options.response_timeout_ms)){fault(a);goto done;}
        a->phase=AG_APP_DISCOVERING;
    }
    if(a->phase==AG_APP_DISCOVERING){
        if(!ag_can_tx_allowed(&p)){
            if(!quiesce(a))goto done;
            a->phase=AG_APP_WAIT_POWER;ok=true;goto done;
        }
        a->transport_quiesced=false;a->quiesce_attempted=false;
        if(!a->options.hooks.can_step(a->options.hooks.context,&a->discovery,NULL)||!fresh(a,&now,&p)){fault(a);goto done;}
        bool pending=a->options.hooks.can_request_pending&&a->options.hooks.can_request_pending(a->options.hooks.context);
        if(!pending&&!ag_discovery_tick(&a->discovery,&p,now)){fault(a);goto done;}
        if(a->discovery.phase==AG_DISCOVERY_COMPLETE){if(!quiesce(a)||!finish_discovery(a)||!fresh(a,&now,&p))goto done;}
        else {ok=true;goto done;}
    }
    /* Bridge pumps feed/output under the same owner; exactly one RPC phase
     * here, rechecking admission internally and refreshing clock after IO. */
    if(a->rpc.attached&&!a->options.rpc_externally_pumped){(void)ag_device_endpoint_step(&a->rpc,now);if(!fresh(a,&now,&p))goto done;}
    if(a->phase==AG_APP_CONFIG_REQUIRED){
        if(!configure(a)||!fresh(a,&now,&p))goto done;
        if(a->phase==AG_APP_CONFIG_REQUIRED){ok=true;goto done;}
    }
    if(!a->options.configuration->ready||!a->options.configuration->writable){fault(a);goto done;}
    if(a->options.configuration->active.revision!=a->configuration.revision){if(!configure(a)||!fresh(a,&now,&p))goto done;}
    if(a->phase!=AG_APP_RUNNING){ok=true;goto done;}
    if(ag_can_tx_allowed(&p)){
        a->transport_quiesced=false;a->quiesce_attempted=false;
        if(!a->options.hooks.can_step(a->options.hooks.context,NULL,&a->runtime)||!fresh(a,&now,&p)){fault(a);goto done;}
    }else if(!a->transport_quiesced&&!quiesce(a))goto done;
    bool pending=a->options.hooks.can_request_pending&&a->options.hooks.can_request_pending(a->options.hooks.context);
    if(!pending&&!ag_runtime_tick(&a->runtime,&p,now)){fault(a);goto done;}
    (void)ag_runtime_drain(&a->runtime);
    if(a->runtime.fault||!fresh(a,&now,&p)){fault(a);goto done;}
    ok=project(a,now,&p);
done:a->busy=false;return ok;
}
bool ag_application_route_allowed(void *context,bool ota){
    ag_application_t *a=context;
    if(a==NULL||!a->initialized||a->phase==AG_APP_FAULT)return false;
    return ota?a->phase==AG_APP_OTA:(a->phase==AG_APP_RUNNING||a->phase==AG_APP_CONFIG_REQUIRED);
}
bool ag_application_enter_exclusive(ag_application_t *a,bool local,bool ota){
    if(a==NULL||!a->initialized||a->busy||!local||!connection_idle(a)
        ||(a->phase!=AG_APP_RUNNING&&a->phase!=AG_APP_CONFIG_REQUIRED)
        ||(a->runtime_started&&a->runtime.count))return false;
    a->busy=true;bool ok=quiesce(a);uint64_t now;ag_can_policy_t p;
    if(ok)ok=fresh(a,&now,&p);
    if(ok){ag_boot_result_t r=ag_boot_health_reconcile(a->options.boot);ok=r==AG_BOOT_OK;
        if(r==AG_BOOT_UNKNOWN||r==AG_BOOT_IO||r==AG_BOOT_INVALID)fault(a);
    }
    if(ok)ok=fresh(a,&now,&p);
    if(ok){a->phase=ota?AG_APP_OTA:AG_APP_MAINTENANCE;memset(&a->scene,0,sizeof(a->scene));
        if(!a->options.hooks.render(a->options.hooks.context,&a->scene))ok=fault(a);
        if(ok)ok=fresh(a,&now,&p);
    }
    a->busy=false;return ok;
}
bool ag_application_leave_exclusive(ag_application_t *a,bool cleanup,bool unchanged){
    if(a==NULL||!a->initialized||a->busy||(a->phase!=AG_APP_OTA&&a->phase!=AG_APP_MAINTENANCE)||!connection_idle(a))return false;
    if(!cleanup||!unchanged)return fault(a);
    a->busy=true;bool ok=true;
    if(a->commissioning_required){
        if(!a->options.configuration->ready||!a->options.configuration->writable||!a->options.journal->ready
            ||(!a->options.journal->writable&&!a->options.allow_degraded_logging)){a->busy=false;return false;}
        a->commissioning_required=false;a->phase=AG_APP_WAIT_POWER;a->busy=false;return true;
    }
    /* Stale RX state must not survive a mode switch. Reconfigure preserves the
     * same journal session and per-PID throttle instead of a second begin. */
    if(a->runtime_started)ok=configure(a);else a->phase=AG_APP_CONFIG_REQUIRED;
    uint64_t now;ag_can_policy_t p;if(ok)ok=fresh(a,&now,&p);
    a->busy=false;return ok;
}
ag_gauge_ui_result_t ag_application_tap(ag_application_t *a,bool local){
    if(a==NULL||!a->initialized||a->busy||a->phase!=AG_APP_RUNNING)return AG_GAUGE_UI_NOT_READY;
    a->busy=true;uint64_t now;ag_can_policy_t p;ag_gauge_ui_result_t r=AG_GAUGE_UI_CLOCK_FAULT;
    if(fresh(a,&now,&p))r=ag_gauge_ui_tap(&a->ui,local,&p,now);
    a->busy=false;return r;
}
ag_boot_result_t ag_application_confirm_health(ag_application_t *a){
    if(a==NULL||!a->initialized||a->busy||a->phase!=AG_APP_RUNNING||a->options.simulation
        ||a->options.hooks.health_mask==NULL||a->runtime.logging_disabled)return AG_BOOT_POLICY;
    a->busy=true;uint64_t now;ag_can_policy_t p;ag_boot_result_t r=AG_BOOT_POLICY;
    uint32_t mask=a->options.hooks.health_mask(a->options.hooks.context);
    if(fresh(a,&now,&p)&&a->options.configuration->ready&&a->options.configuration->writable
        &&a->options.journal->ready&&a->options.journal->writable&&!a->runtime.fault
        &&(mask&a->options.boot->required_health)==a->options.boot->required_health
        &&!(mask&~AG_BOOT_HEALTH_KNOWN)
        &&!a->runtime.storage_ambiguous&&ag_can_tx_allowed(&p))r=ag_boot_health_confirm(a->options.boot,mask);
    if(!fresh(a,&now,&p))r=AG_BOOT_UNKNOWN;
    if(r==AG_BOOT_UNKNOWN||r==AG_BOOT_IO)fault(a);
    a->busy=false;return r;
}
