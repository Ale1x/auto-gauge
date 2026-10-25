#include "ag_boot_health.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ag_boot_health_t state;
static struct {uint8_t blob[96];bool empty,read_fail,write_fail,commit_on_error,observe_fail,target_ok,mark_fail,mark_change,reenter;unsigned writes,marks,fail_write_n;ag_boot_observation_t observation;} mock;
static ag_boot_load_t read_record(void *c,uint8_t out[96]){assert(c==&mock);if(mock.read_fail)return AG_BOOT_LOAD_IO;if(mock.empty)return AG_BOOT_LOAD_EMPTY;memcpy(out,mock.blob,96);return AG_BOOT_LOAD_OK;}
static bool write_record(void *c,const uint8_t bytes[96]){assert(c==&mock);++mock.writes;if(mock.reenter)assert(ag_boot_health_confirm(&state,15)==AG_BOOT_BUSY);bool fail=mock.write_fail||mock.writes==mock.fail_write_n;if(!fail||mock.commit_on_error){memcpy(mock.blob,bytes,96);mock.empty=false;}return !fail;}
static bool observe(void *c,uint32_t size,ag_boot_observation_t *out){assert(c==&mock);(void)size;if(mock.observe_fail)return false;*out=mock.observation;return true;}
static bool target(void *c,uint32_t address,uint32_t bytes,const uint8_t hash[32]){assert(c==&mock);return mock.target_ok&&address==0x110000&&bytes==2080&&hash[0]==42;}
static bool mark(void *c){assert(c==&mock);++mock.marks;if(!mock.mark_fail||mock.mark_change)mock.observation.state=AG_BOOT_IMAGE_VALID;return !mock.mark_fail;}
static ag_boot_hooks_t hooks(void){return (ag_boot_hooks_t){&mock,read_record,write_record,observe,target,mark,false};}
static void open_again(ag_boot_result_t expected){memset(&state,0,sizeof(state));ag_boot_hooks_t h=hooks();assert(ag_boot_health_open(&state,&h,15)==expected);}
static void boot(void){memset(&state,0,sizeof(state));memset(&mock,0,sizeof(mock));mock.empty=mock.target_ok=true;mock.observation.running_address=mock.observation.boot_address=0x10000;mock.observation.state=AG_BOOT_IMAGE_VALID;mock.observation.digest[0]=42;open_again(AG_BOOT_EMPTY);}
static void provision(void){boot();assert(ag_boot_health_provision(&state,9)==AG_BOOT_OK);uint64_t floor;assert(ag_boot_health_floor(&state,&floor)&&floor==9);}
static void staged(void){provision();uint8_t hash[32]={42};assert(ag_boot_health_stage(&state,10,0x110000,2080,hash)==AG_BOOT_OK&&state.pending==1&&state.accepted_floor==9);}
static void trial(void){staged();mock.observation.running_address=mock.observation.boot_address=0x110000;mock.observation.state=AG_BOOT_IMAGE_PENDING;assert(ag_boot_health_reconcile(&state)==AG_BOOT_NEEDS_HEALTH);}
static void test_order(void){
    trial();assert(ag_boot_health_confirm(&state,7)==AG_BOOT_POLICY&&mock.marks==0&&mock.writes==2);
    mock.reenter=true;assert(ag_boot_health_confirm(&state,15)==AG_BOOT_OK&&state.accepted_floor==10&&state.pending==0&&mock.marks==1&&mock.writes==4);
    open_again(AG_BOOT_OK);assert(ag_boot_health_reconcile(&state)==AG_BOOT_OK&&state.accepted_floor==10);
    assert(ag_boot_health_confirm(&state,15)==AG_BOOT_POLICY&&mock.marks==1);
}
static void test_fallback(void){
    staged();mock.observation.boot_address=0x110000;assert(ag_boot_health_reconcile(&state)==AG_BOOT_AWAITING_REBOOT&&state.pending==1);
    mock.observation.boot_address=0x10000;assert(ag_boot_health_reconcile(&state)==AG_BOOT_RECOVERED_FALLBACK&&state.pending==0&&state.accepted_floor==9&&mock.marks==0);
    trial();mock.observation.state=AG_BOOT_IMAGE_INVALID;assert(ag_boot_health_reconcile(&state)==AG_BOOT_POLICY&&state.fault&&mock.marks==0);
}
static void test_ambiguous_mark(void){
    for(unsigned changed=0;changed<2;++changed){trial();mock.mark_fail=true;mock.mark_change=changed;
        assert(ag_boot_health_confirm(&state,15)==AG_BOOT_UNKNOWN&&state.fault&&state.accepted_floor==9&&mock.marks==1);
        assert(ag_boot_health_confirm(&state,15)==AG_BOOT_INVALID&&mock.marks==1);
        mock.mark_fail=false;open_again(AG_BOOT_OK);
        if(changed){assert(ag_boot_health_reconcile(&state)==AG_BOOT_OK&&state.accepted_floor==10&&mock.marks==1);}
        else{mock.observation.running_address=mock.observation.boot_address=0x10000;mock.observation.state=AG_BOOT_IMAGE_VALID;
            assert(ag_boot_health_reconcile(&state)==AG_BOOT_RECOVERED_FALLBACK&&state.accepted_floor==9&&mock.marks==1);}
    }
}
static void test_store_unknown(void){
    for(unsigned committed=0;committed<2;++committed){trial();mock.write_fail=true;mock.commit_on_error=committed;
        assert(ag_boot_health_confirm(&state,15)==AG_BOOT_UNKNOWN&&mock.marks==0&&state.accepted_floor==9);
        mock.write_fail=false;open_again(AG_BOOT_OK);assert(state.pending==(committed?2:1));
        assert(ag_boot_health_confirm(&state,15)==AG_BOOT_OK&&state.accepted_floor==10&&mock.marks==1);
    }
    provision();mock.write_fail=true;mock.commit_on_error=true;uint8_t hash[32]={42};assert(ag_boot_health_stage(&state,10,0x110000,2080,hash)==AG_BOOT_UNKNOWN);
    mock.write_fail=false;open_again(AG_BOOT_OK);assert(state.pending==1&&state.accepted_floor==9);
}
static void test_corruption_and_policy(void){
    provision();uint8_t original[96];memcpy(original,mock.blob,96);
    for(unsigned i=0;i<96;++i){memcpy(mock.blob,original,96);mock.blob[i]^=1;open_again(AG_BOOT_INVALID);assert(state.fault);}
    memcpy(mock.blob,original,96);open_again(AG_BOOT_OK);uint8_t hash[32]={42};
    assert(ag_boot_health_stage(&state,9,0x110000,2080,hash)==AG_BOOT_POLICY);
    assert(ag_boot_health_stage(&state,10,0x10000,2080,hash)==AG_BOOT_POLICY);
    mock.target_ok=false;assert(ag_boot_health_stage(&state,10,0x110000,2080,hash)==AG_BOOT_POLICY&&mock.marks==0);
    boot();mock.observation.state=AG_BOOT_IMAGE_PENDING;assert(ag_boot_health_provision(&state,9)==AG_BOOT_POLICY&&mock.writes==0);
}
static void test_final_floor_commit_and_witness(void){
    for(unsigned committed=0;committed<2;++committed){trial();mock.fail_write_n=4;mock.commit_on_error=committed;
        assert(ag_boot_health_confirm(&state,15)==AG_BOOT_UNKNOWN&&mock.marks==1&&state.fault);
        mock.fail_write_n=0;open_again(AG_BOOT_OK);assert(ag_boot_health_reconcile(&state)==AG_BOOT_OK&&state.accepted_floor==10&&mock.marks==1);
    }
    trial();mock.mark_fail=true;mock.mark_change=true;assert(ag_boot_health_confirm(&state,15)==AG_BOOT_UNKNOWN);
    memset(&state,0,sizeof(state));ag_boot_hooks_t h=hooks();assert(ag_boot_health_open(&state,&h,255)==AG_BOOT_OK);
    assert(ag_boot_health_reconcile(&state)==AG_BOOT_POLICY&&state.accepted_floor==9&&state.fault);
}
static void test_explicit_factory_bootstrap(void){
    boot();mock.observation.state=AG_BOOT_IMAGE_FACTORY;assert(ag_boot_health_provision(&state,9)==AG_BOOT_POLICY&&mock.writes==0);
    for(unsigned fallback=0;fallback<2;++fallback){
        boot();mock.observation.state=AG_BOOT_IMAGE_FACTORY;memset(&state,0,sizeof(state));ag_boot_hooks_t h=hooks();h.allow_factory_bootstrap=true;
        assert(ag_boot_health_open(&state,&h,15)==AG_BOOT_EMPTY&&ag_boot_health_provision(&state,9)==AG_BOOT_OK);
        uint8_t hash[32]={42};assert(ag_boot_health_stage(&state,10,0x110000,2080,hash)==AG_BOOT_OK);
        if(fallback){assert(ag_boot_health_reconcile(&state)==AG_BOOT_RECOVERED_FALLBACK&&state.accepted_floor==9&&mock.marks==0);}
        else{mock.observation.running_address=mock.observation.boot_address=0x110000;mock.observation.state=AG_BOOT_IMAGE_PENDING;
            assert(ag_boot_health_confirm(&state,15)==AG_BOOT_OK&&state.accepted_floor==10&&mock.marks==1);}
    }
}
static void test_identity(void){
    trial();mock.observation.digest[0]^=1;assert(ag_boot_health_confirm(&state,15)==AG_BOOT_POLICY&&mock.marks==0);
    trial();mock.observation.state=AG_BOOT_IMAGE_VALID;assert(ag_boot_health_reconcile(&state)==AG_BOOT_POLICY&&state.fault&&state.accepted_floor==9);
    provision();mock.observe_fail=true;assert(ag_boot_health_reconcile(&state)==AG_BOOT_IO&&state.fault);
}
int main(void){test_order();test_fallback();test_ambiguous_mark();test_store_unknown();test_corruption_and_policy();test_final_floor_commit_and_witness();test_explicit_factory_bootstrap();test_identity();puts("PASS: 8 boot health durable-order groups; behavioral backend only");return 0;}
