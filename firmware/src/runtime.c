#include "ag_runtime.h"
#include <string.h>
static bool fail(ag_runtime_t *r){r->fault=true;return false;}
static bool increment(ag_runtime_t *r,uint64_t *n){if(*n==UINT64_MAX)return fail(r);++*n;return true;}
static bool ready(ag_runtime_t *r,uint64_t now){
    if(r==NULL||!r->initialized||!r->started||r->fault)return false;
    if(r->has_time&&now<r->last_ms)return fail(r);
    r->last_ms=now;r->has_time=true;return true;
}
static bool changed(const ag_measurement_t *a,const ag_measurement_t *b){
    return a->status!=b->status||a->updated_ms!=b->updated_ms||a->has_value!=b->has_value
        ||a->raw_length!=b->raw_length||memcmp(a->raw,b->raw,a->raw_length)!=0
        ||a->negative_response_code!=b->negative_response_code;
}
static void collect(ag_runtime_t *r,uint64_t now){
    if(r->scheduler.clock_fault){fail(r);return;}
    for(size_t i=0;i<r->scheduler.count&&!r->fault;++i){
        const ag_measurement_t *m=&r->scheduler.jobs[i].sample;
        if(!changed(&r->observed[i],m))continue;
        r->observed[i]=*m;
        if(r->logging_disabled){increment(r,&r->not_recorded);continue;}
        size_t budget=i;for(size_t j=0;j<i;++j)if(r->scheduler.jobs[j].config.pid==m->pid){budget=j;break;}
        if(r->has_logged[budget]&&now-r->last_logged[budget]<1000){increment(r,&r->rate_limited);continue;}
        r->last_logged[budget]=now;r->has_logged[budget]=true;
        if(r->count==AG_RUNTIME_QUEUE){increment(r,&r->producer_dropped);continue;}
        ag_history_record_t *out=&r->queue[(r->head+r->count)%AG_RUNTIME_QUEUE];
        memset(out,0,sizeof(*out));out->pid=m->pid;out->ecu_source=m->ecu_source;
        out->monotonic_ms=now;out->status=m->status;
        if(m->status==AG_SAMPLE_VALID){out->raw_length=m->raw_length;memcpy(out->raw,m->raw,m->raw_length);}
        ++r->count;
    }
}
bool ag_runtime_init(ag_runtime_t *r,const ag_config_t *config,const ag_runtime_job_t *jobs,size_t n,
                     const uint8_t sid[16],const ag_runtime_sink_t *sink){
    if(r==NULL)return false;
    memset(r,0,sizeof(*r));
    if(config==NULL||jobs==NULL||n==0||n>AG_POLL_JOB_CAPACITY||sid==NULL||sink==NULL||sink->begin==NULL||sink->append==NULL)return false;
    bool nonzero=false;for(unsigned i=0;i<16;++i)nonzero|=sid[i]!=0;if(!nonzero)return false;
    ag_pid_mask_t supported,configured;ag_pid_mask_clear(&supported);ag_pid_mask_clear(&configured);
    for(size_t i=0;i<n;++i){ag_pid_mask_t usable;ag_obd_usable_pids(&jobs[i].capabilities,&usable);
        if(ag_pid_mask_has(&usable,jobs[i].job.pid))ag_pid_mask_set(&supported,jobs[i].job.pid);}
    if(ag_config_validate(config,&supported)!=AG_CONFIG_OK)return false;
    for(size_t i=0;i<config->dashboard_count;++i)for(size_t j=0;j<config->dashboards[i].widget_count;++j)
        ag_pid_mask_set(&configured,(uint8_t)config->dashboards[i].widgets[j].pid);
    ag_scheduler_init(&r->scheduler);
    for(size_t i=0;i<n;++i){if(!ag_pid_mask_has(&configured,jobs[i].job.pid)
            ||!ag_scheduler_add(&r->scheduler,&jobs[i].job,&jobs[i].capabilities))return false;
        r->observed[i]=r->scheduler.jobs[i].sample;}
    r->sink=*sink;memcpy(r->session_id,sid,16);r->initialized=true;return true;
}
bool ag_runtime_allow_degraded_logging(ag_runtime_t *r){
    if(r==NULL||!r->initialized||r->started||r->fault)return false;
    r->allow_live_without_logging=true;return true;
}
bool ag_runtime_start(ag_runtime_t *r){
    if(r==NULL||!r->initialized||r->started||r->fault)return false;
    ag_runtime_store_result_t result=r->sink.begin(r->sink.context,r->session_id);
    if(result!=AG_RUNTIME_STORE_OK){
        r->storage_ambiguous=result==AG_RUNTIME_STORE_AMBIGUOUS;
        if(!r->allow_live_without_logging)return fail(r);
        r->logging_disabled=true;
    }
    r->started=true;return true;
}
bool ag_runtime_tick(ag_runtime_t *r,const ag_can_policy_t *p,uint64_t now){
    if(!ready(r,now))return false;
    bool ok=ag_scheduler_tick(&r->scheduler,p,now);collect(r,now);return ok&&!r->fault;
}
bool ag_runtime_prepare(ag_runtime_t *r,const ag_can_policy_t *p,uint64_t now){
    if(!ready(r,now))return false;
    bool ok=ag_scheduler_prepare(&r->scheduler,p,now);collect(r,now);return ok&&!r->fault;
}
bool ag_runtime_dispatch(ag_runtime_t *r,const ag_can_policy_t *p,uint64_t now,ag_simulated_dispatch_t *out){
    if(out==NULL)return false;
    memset(out,0,sizeof(*out));if(!ready(r,now))return false;
    bool ok=ag_scheduler_dispatch(&r->scheduler,p,now,out);collect(r,now);if(r->fault)memset(out,0,sizeof(*out));return ok&&!r->fault;
}
ag_scheduler_rx_result_t ag_runtime_receive(ag_runtime_t *r,const ag_can_policy_t *p,const ag_can_frame_t *f,uint64_t now){
    if(!ready(r,now))return AG_SCHED_RX_BLOCKED;
    ag_scheduler_rx_result_t result=ag_scheduler_receive(&r->scheduler,p,f,now);collect(r,now);
    return r->fault?AG_SCHED_RX_BLOCKED:result;
}
bool ag_runtime_fc_prepare(ag_runtime_t *r,const ag_can_policy_t *p,uint64_t now){
    if(!ready(r,now))return false;
    bool ok=ag_scheduler_fc_prepare(&r->scheduler,p,now);collect(r,now);return ok&&!r->fault;
}
bool ag_runtime_fc_dispatch(ag_runtime_t *r,const ag_can_policy_t *p,uint64_t now,ag_can_frame_t *out){
    if(out==NULL)return false;
    memset(out,0,sizeof(*out));if(!ready(r,now))return false;
    bool ok=ag_scheduler_fc_dispatch(&r->scheduler,p,now,out);collect(r,now);if(r->fault)memset(out,0,sizeof(*out));return ok&&!r->fault;
}
bool ag_runtime_drain(ag_runtime_t *r){
    if(r==NULL||!r->started||r->fault||r->logging_disabled||r->count==0)return false;
    if(r->persisted==UINT64_MAX)return fail(r);
    ag_runtime_store_result_t result=r->sink.append(r->sink.context,&r->queue[r->head]);
    if(result==AG_RUNTIME_STORE_BUSY)return false;
    if(result!=AG_RUNTIME_STORE_OK){
        r->storage_ambiguous=result==AG_RUNTIME_STORE_AMBIGUOUS;
        if(!r->allow_live_without_logging)return fail(r);
        r->logging_disabled=true;
        if(UINT64_MAX-r->not_recorded<r->count)return fail(r);
        r->not_recorded+=r->count;r->count=0;r->head=0;return false;
    }
    r->head=(r->head+1)%AG_RUNTIME_QUEUE;--r->count;return increment(r,&r->persisted);
}
ag_runtime_store_result_t ag_runtime_journal_begin(void *context,const uint8_t sid[16]){
    ag_journal_result_t result=ag_journal_begin_session(context,sid,1);
    return result==AG_JOURNAL_OK?AG_RUNTIME_STORE_OK:result==AG_JOURNAL_IO_ERROR?AG_RUNTIME_STORE_AMBIGUOUS:AG_RUNTIME_STORE_FAILED;
}
ag_runtime_store_result_t ag_runtime_journal_append(void *context,const ag_history_record_t *record){
    ag_journal_result_t result=ag_journal_append(context,record->pid,record->ecu_source,record->monotonic_ms,
        record->status,record->raw,record->raw_length);
    return result==AG_JOURNAL_OK?AG_RUNTIME_STORE_OK:result==AG_JOURNAL_IO_ERROR?AG_RUNTIME_STORE_AMBIGUOUS:AG_RUNTIME_STORE_FAILED;
}
