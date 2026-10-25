#define main runtime_baseline_main
#include "test_runtime.c"
#undef main
static void init_degraded(ag_runtime_t *r,sink_t *s){ag_runtime_job_t j=job(0x7e8);ag_config_t c=config();uint8_t sid[16]={6};ag_runtime_sink_t sink={s,begin,save};
    assert(ag_runtime_init(r,&c,&j,1,sid,&sink));assert(ag_runtime_allow_degraded_logging(r));assert(ag_runtime_start(r));assert(!ag_runtime_allow_degraded_logging(r));}
int main(void){
    assert(runtime_baseline_main()==0);
    for(unsigned n=AG_RUNTIME_STORE_FAILED;n<=AG_RUNTIME_STORE_AMBIGUOUS;++n){
        ag_runtime_t r;sink_t s={.begin_result=(ag_runtime_store_result_t)n};init_degraded(&r,&s);assert(r.logging_disabled&&!r.fault&&s.begins==1);
        ag_can_policy_t p=policy();response(&r,&p,0);assert(r.scheduler.jobs[0].sample.status==AG_SAMPLE_VALID&&r.count==0&&r.not_recorded==1&&r.persisted==0);
        assert(!ag_runtime_drain(&r)&&s.calls==0);response(&r,&p,101);assert(r.not_recorded==2&&s.begins==1);
    }
    ag_runtime_t r;sink_t s={0};init_degraded(&r,&s);ag_can_policy_t p=policy();response(&r,&p,0);s.result=AG_RUNTIME_STORE_AMBIGUOUS;s.commit_ambiguous=true;
    assert(!ag_runtime_drain(&r)&&r.logging_disabled&&r.storage_ambiguous&&!r.fault&&r.not_recorded==1&&r.persisted==0&&s.calls==1&&s.count==1);
    response(&r,&p,1000);assert(r.not_recorded==2&&r.count==0&&!ag_runtime_drain(&r)&&s.calls==1);
    p.ignition=AG_IGN_OFF;ag_simulated_dispatch_t d;assert(!ag_runtime_dispatch(&r,&p,2000,&d)&&d.request.length==0);
    puts("PASS: degraded logging preserves live acquisition without retry or claimed persistence");return 0;
}
