#include "ag_journal_history.h"
#include "ag_flash_history_sim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t sid_a[16]={1},sid_b[16]={2};
static ag_journal_io_t io_of(ag_flash_history_sim_t *sim){ag_flash_history_io_t io=ag_flash_history_sim_io(sim);return (ag_journal_io_t){io.context,io.read,io.program,io.erase_sector};}
static void init(ag_journal_history_t *s,ag_flash_history_sim_t *sim,uint8_t bytes[8192]){
    assert(ag_flash_history_sim_init(sim,bytes,8192));ag_journal_io_t io=io_of(sim);
    assert(ag_journal_open(s,&io,2)==AG_JOURNAL_EMPTY);assert(ag_journal_bootstrap(s,sid_a,1,1)==AG_JOURNAL_OK);
}
static void append(ag_journal_history_t *s,unsigned n){uint8_t raw[64];memset(raw,(int)(n&255),sizeof(raw));
    assert(ag_journal_append(s,12,0x7e8,n,AG_SAMPLE_VALID,raw,64)==AG_JOURNAL_OK);}
static ag_journal_result_t page(ag_journal_history_t *s,const uint8_t sid[16],uint64_t after,char out[4097],ag_history_page_info_t *info){return ag_journal_make_page(s,sid,after,16,out,4097,info);}
static void test_append_rollover_exact_loss_and_ack(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_journal_history_t s;init(&s,&sim,bytes);
    ag_flash_history_sim_reboot(&sim);append(&s,1);assert(sim.erases==0&&sim.programs==4);
    for(unsigned n=2;n<=75;++n)append(&s,n);
    assert(s.info.generation==3&&s.info.next_serial==76&&s.info.next_sequence==76&&s.info.evicted_samples==30);
    assert(sim.erases==1);char a[4097],b[4097];ag_history_page_info_t info;
    assert(page(&s,sid_a,0,a,&info)==AG_JOURNAL_OK&&info.lost_first==1&&info.lost_last==30&&info.record_count>0);
    size_t writes=sim.programs,erases=sim.erases;assert(ag_journal_acknowledge(&s,sid_a,75)==AG_JOURNAL_OK);
    assert(page(&s,sid_a,0,b,&info)==AG_JOURNAL_OK&&strcmp(a,b)==0&&sim.programs==writes&&sim.erases==erases);
    assert(page(&s,sid_a,65,b,&info)==AG_JOURNAL_OK&&!info.has_loss&&info.record_count==10);
    assert(page(&s,sid_a,76,b,&info)==AG_JOURNAL_CURSOR_AHEAD&&b[0]==0);
    uint8_t before[128];memcpy(before,bytes,128);ag_journal_io_t io=io_of(&sim);ag_journal_history_t boot;
    assert(ag_journal_open(&boot,&io,2)==AG_JOURNAL_OK&&!boot.writer_attached&&boot.info.next_sequence==76);
    uint8_t raw=1;assert(ag_journal_append(&boot,12,0x7e8,76,AG_SAMPLE_VALID,&raw,1)==AG_JOURNAL_NEW_SESSION_REQUIRED);
    assert(ag_journal_begin_session(&boot,sid_a,1)==AG_JOURNAL_SESSION_REUSED);
    assert(ag_journal_begin_session(&boot,sid_b,1)==AG_JOURNAL_OK);
    assert(boot.info.next_serial==77&&boot.info.next_sequence==1);append(&boot,0);
    assert(page(&boot,sid_a,65,b,&info)==AG_JOURNAL_OK&&info.record_count==10);
    assert(page(&boot,sid_b,0,b,&info)==AG_JOURNAL_OK&&info.record_count==1&&!info.has_loss);
    (void)before;
}
static void test_empty_sessions_controls_and_eviction(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_journal_history_t s;init(&s,&sim,bytes);
    char out[4097];ag_history_page_info_t info;assert(page(&s,sid_a,0,out,&info)==AG_JOURNAL_OK&&info.record_count==0);
    for(unsigned n=2;n<=65;++n){uint8_t sid[16]={0};sid[0]=(uint8_t)n;
        assert(ag_journal_begin_session(&s,sid,1)==AG_JOURNAL_OK);
    }
    assert(s.info.evicted_samples==0&&s.info.retained_first_serial==31&&s.info.next_serial==65);
    assert(page(&s,sid_a,0,out,&info)==AG_JOURNAL_NOT_RETAINED&&out[0]==0);
    uint8_t empty[16]={40};assert(page(&s,empty,0,out,&info)==AG_JOURNAL_OK&&info.record_count==0&&!info.has_loss);
    /* Empty initial SID, then samples, then all those samples evicted: no empty resurrection. */
    init(&s,&sim,bytes);for(unsigned n=1;n<=30;++n)append(&s,n);
    assert(ag_journal_begin_session(&s,sid_b,1)==AG_JOURNAL_OK);
    for(unsigned n=0;n<60;++n)append(&s,n);
    assert(page(&s,sid_a,0,out,&info)==AG_JOURNAL_NOT_RETAINED);
    assert(s.info.evicted_samples==59); /* 30 A samples +29 B, one control occupies an entry. */
}
static size_t cuts_for(unsigned initial_count,bool control){
    uint8_t bytes[8192],seed[8192];ag_flash_history_sim_t sim;ag_journal_history_t s;init(&s,&sim,bytes);
    for(unsigned n=1;n<=initial_count;++n)append(&s,n);
    memcpy(seed,bytes,sizeof(seed));ag_journal_history_t original=s;
    ag_flash_history_sim_reboot(&sim);
    if(control)assert(ag_journal_begin_session(&s,sid_b,1)==AG_JOURNAL_OK);else append(&s,initial_count+1);
    size_t calls=sim.operations;assert(calls<AG_FLASH_HISTORY_TRACE);
    size_t lengths[AG_FLASH_HISTORY_TRACE];uint8_t kinds[AG_FLASH_HISTORY_TRACE];
    memcpy(lengths,sim.trace_lengths,sizeof(lengths));memcpy(kinds,sim.trace_kinds,sizeof(kinds));size_t trials=0;
    for(size_t call=1;call<=calls;++call){size_t length=lengths[call-1];size_t step=kinds[call-1]==1?1:(length>1?length/2:1);
        for(size_t cut=0;;){memcpy(bytes,seed,sizeof(seed));s=original;ag_flash_history_sim_fail_at(&sim,call,cut);
            ag_journal_result_t result;if(control)result=ag_journal_begin_session(&s,sid_b,1);
            else {uint8_t raw[64];memset(raw,(int)((initial_count+1)&255),64);result=ag_journal_append(&s,12,0x7e8,initial_count+1,AG_SAMPLE_VALID,raw,64);}
            assert(!sim.invalid_access);
            /* Current intent/header sector cannot be the erased next victim. */
            assert(memcmp(bytes+original.current_index*4096,seed+original.current_index*4096,3968)==0||initial_count<30);
            ag_flash_history_sim_reboot(&sim);ag_journal_io_t io=io_of(&sim);ag_journal_history_t recovered;
            ag_journal_result_t opened=ag_journal_open(&recovered,&io,2);
            assert(opened==AG_JOURNAL_OK||opened==AG_JOURNAL_READ_ONLY);
            assert(!recovered.writer_attached);
            if(opened==AG_JOURNAL_OK){
                assert(recovered.info.next_serial>=original.info.next_serial);
                assert(recovered.info.next_serial<=original.info.next_serial+1);
                assert(recovered.info.evicted_samples>=original.info.evicted_samples);
                char page_data[4097];ag_history_page_info_t info;
                assert(page(&recovered,sid_a,0,page_data,&info)==AG_JOURNAL_OK);
                if(info.has_loss)assert(info.lost_last==30);
                if(result==AG_JOURNAL_OK)assert(recovered.info.next_serial==original.info.next_serial+1);
            }else{
                size_t erases=sim.erases,writes=sim.programs;
                assert(ag_journal_begin_session(&recovered,sid_b,1)==AG_JOURNAL_READ_ONLY);
                assert(sim.erases==erases&&sim.programs==writes);
                char page_data[4097];ag_history_page_info_t info;
                assert(page(&recovered,sid_a,0,page_data,&info)==AG_JOURNAL_READ_ONLY&&page_data[0]==0);
            }
            ++trials;if(cut==length)break;cut=length-cut<step?length:cut+step;
        }
    }return trials;
}
static void test_faults_append_intent_erase_header_control_and_resume(void){
    size_t cuts=cuts_for(0,false)+cuts_for(30,false)+cuts_for(60,false)+cuts_for(60,true);
    printf("PASS: %zu IO cuts, all programmed-byte boundaries + read/erase boundaries\n",cuts);
}
static void test_corruption_short_reads_and_no_default(void){
    uint8_t bytes[8192],seed[8192];ag_flash_history_sim_t sim;ag_journal_history_t s;init(&s,&sim,bytes);
    for(unsigned n=1;n<=31;++n)append(&s,n);memcpy(seed,bytes,sizeof(seed));
    for(unsigned at=0;at<8192;++at){memcpy(bytes,seed,sizeof(seed));bytes[at]^=1;ag_flash_history_sim_reboot(&sim);
        ag_journal_io_t io=io_of(&sim);ag_journal_history_t recovered;ag_journal_result_t result=ag_journal_open(&recovered,&io,2);
        assert(result==AG_JOURNAL_READ_ONLY||result==AG_JOURNAL_CORRUPT);
        assert(sim.erases==0&&sim.programs==0);
    }
    memcpy(bytes,seed,sizeof(seed));ag_flash_history_sim_fail_at(&sim,1,0);ag_journal_io_t io=io_of(&sim);ag_journal_history_t recovered;
    ag_journal_result_t result=ag_journal_open(&recovered,&io,2);assert(result==AG_JOURNAL_READ_ONLY||result==AG_JOURNAL_CORRUPT);
    ag_flash_history_sim_reboot(&sim);memset(bytes,0,sizeof(bytes));assert(ag_journal_open(&recovered,&io,2)==AG_JOURNAL_CORRUPT);
    assert(ag_journal_bootstrap(&recovered,sid_b,1,1)==AG_JOURNAL_CORRUPT&&sim.erases==0);
}
static void test_terminal_raw_clock_window_and_geometry(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;assert(ag_flash_history_sim_init(&sim,bytes,8192));ag_journal_io_t io=io_of(&sim);ag_journal_history_t s;
    assert(ag_journal_open(&s,&io,2)==AG_JOURNAL_EMPTY);
    assert(ag_journal_bootstrap(&s,sid_a,UINT64_MAX,UINT64_MAX)==AG_JOURNAL_OK);append(&s,0);
    uint8_t raw[65]={0};assert(ag_journal_append(&s,12,0x7e8,1,AG_SAMPLE_VALID,raw,1)==AG_JOURNAL_EXHAUSTED);
    assert(ag_journal_begin_session(&s,sid_b,1)==AG_JOURNAL_EXHAUSTED);
    char out[4097];ag_history_page_info_t info;assert(page(&s,sid_a,UINT64_MAX,out,&info)==AG_JOURNAL_OK&&info.record_count==0);
    init(&s,&sim,bytes);assert(ag_journal_append(&s,12,0x7e8,0,AG_SAMPLE_VALID,raw,65)==AG_JOURNAL_INVALID);
    append(&s,1);assert(ag_journal_append(&s,12,0x7e8,0,AG_SAMPLE_VALID,raw,1)==AG_JOURNAL_CLOCK_REGRESSION);
    char tiny[2]={'X','X'};assert(ag_journal_make_page(&s,sid_a,0,16,tiny,2,&info)==AG_JOURNAL_OUTPUT_SHORT&&tiny[0]==0&&info.written==0);
    static uint8_t maximum[2u*1024u*1024u];assert(ag_flash_history_sim_init(&sim,maximum,sizeof(maximum)));io=io_of(&sim);
    assert(ag_journal_open(&s,&io,512)==AG_JOURNAL_EMPTY);assert(ag_journal_bootstrap(&s,sid_a,1,1)==AG_JOURNAL_OK);append(&s,1);
    assert(ag_journal_open(&s,&io,513)==AG_JOURNAL_INVALID);
}
static void test_erased_acknowledged_entries_and_sector_never_roll_back(void){
    uint8_t bytes[8192],seed[8192];ag_flash_history_sim_t sim;ag_journal_history_t source;init(&source,&sim,bytes);
    for(unsigned n=1;n<=31;++n)append(&source,n);
    assert(ag_journal_acknowledge(&source,sid_a,31)==AG_JOURNAL_OK);memcpy(seed,bytes,sizeof(seed));
    for(unsigned kind=0;kind<3;++kind){memcpy(bytes,seed,sizeof(seed));uint32_t current=source.current_index*4096u;
        if(kind==0)memset(bytes+current+128,0xff,128);
        else if(kind==1)memset(bytes+current,0xff,4096);
        else {memset(bytes+current,0xff,128);memset(bytes+current+128,0xff,128);}
        ag_flash_history_sim_reboot(&sim);ag_journal_io_t io=io_of(&sim);ag_journal_history_t recovered;
        ag_journal_result_t result=ag_journal_open(&recovered,&io,2);assert(result==AG_JOURNAL_READ_ONLY||result==AG_JOURNAL_CORRUPT);
        char out[4097];ag_history_page_info_t info;assert(page(&recovered,sid_a,0,out,&info)!=AG_JOURNAL_OK&&out[0]==0);
        assert(ag_journal_acknowledge(&recovered,sid_a,31)!=AG_JOURNAL_OK&&sim.erases==0&&sim.programs==0);
    }
}
static void test_committed_intent_logical_eviction_and_safe_resume(void){
    uint8_t bytes[8192],seed[8192];ag_flash_history_sim_t sim;ag_journal_history_t s;init(&s,&sim,bytes);
    for(unsigned n=1;n<=60;++n)append(&s,n);
    memcpy(seed,bytes,sizeof(seed));ag_journal_history_t original=s;ag_flash_history_sim_reboot(&sim);append(&s,61);
    size_t erase_call=0;for(size_t n=0;n<sim.operations&&n<AG_FLASH_HISTORY_TRACE;++n)if(sim.trace_kinds[n]==2){erase_call=n+1;break;}
    assert(erase_call!=0);
    for(unsigned state=0;state<2;++state){memcpy(bytes,seed,sizeof(seed));s=original;
        ag_flash_history_sim_fail_at(&sim,erase_call,state?4096:0);uint8_t raw[64]={0};
        assert(ag_journal_append(&s,12,0x7e8,61,AG_SAMPLE_VALID,raw,64)==AG_JOURNAL_IO_ERROR);
        ag_flash_history_sim_reboot(&sim);ag_journal_io_t io=io_of(&sim);ag_journal_history_t recovered;
        assert(ag_journal_open(&recovered,&io,2)==AG_JOURNAL_OK&&recovered.pending_rollover);
        assert(recovered.info.evicted_samples==30&&recovered.info.next_serial==61);
        char out[4097];ag_history_page_info_t info;assert(page(&recovered,sid_a,0,out,&info)==AG_JOURNAL_OK&&info.lost_last==30);
        assert(ag_journal_begin_session(&recovered,sid_b,1)==AG_JOURNAL_OK);
        assert(recovered.info.evicted_samples==30&&recovered.info.next_serial==62&&!recovered.pending_rollover);
        append(&recovered,0);assert(page(&recovered,sid_b,0,out,&info)==AG_JOURNAL_OK&&info.record_count==1);
    }
}
static void test_bootstrap_program_byte_boundaries(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;assert(ag_flash_history_sim_init(&sim,bytes,8192));ag_journal_io_t io=io_of(&sim);ag_journal_history_t s;
    assert(ag_journal_open(&s,&io,2)==AG_JOURNAL_EMPTY);ag_flash_history_sim_reboot(&sim);
    assert(ag_journal_bootstrap(&s,sid_a,1,1)==AG_JOURNAL_OK);size_t calls=sim.operations;
    size_t lengths[AG_FLASH_HISTORY_TRACE];uint8_t kinds[AG_FLASH_HISTORY_TRACE];memcpy(lengths,sim.trace_lengths,sizeof(lengths));memcpy(kinds,sim.trace_kinds,sizeof(kinds));
    for(size_t call=1;call<=calls;++call)if(kinds[call-1]==1){for(size_t cut=0;cut<=lengths[call-1];++cut){
        assert(ag_flash_history_sim_init(&sim,bytes,8192));assert(ag_journal_open(&s,&io,2)==AG_JOURNAL_EMPTY);
        ag_flash_history_sim_fail_at(&sim,call,cut);(void)ag_journal_bootstrap(&s,sid_a,1,1);ag_flash_history_sim_reboot(&sim);
        ag_journal_result_t result=ag_journal_open(&s,&io,2);assert(result==AG_JOURNAL_EMPTY||result==AG_JOURNAL_CORRUPT||result==AG_JOURNAL_OK);
        if(result==AG_JOURNAL_CORRUPT)assert(ag_journal_bootstrap(&s,sid_b,1,1)==AG_JOURNAL_CORRUPT&&sim.erases==0&&sim.programs==0);
    }}
}
int main(void){test_append_rollover_exact_loss_and_ack();test_empty_sessions_controls_and_eviction();
    test_faults_append_intent_erase_header_control_and_resume();test_corruption_short_reads_and_no_default();test_terminal_raw_clock_window_and_geometry();
    test_erased_acknowledged_entries_and_sector_never_roll_back();test_committed_intent_logical_eviction_and_safe_resume();test_bootstrap_program_byte_boundaries();
    printf("PASS: 8 journal groups; sizeof=%zu metadata bytes; no hardware IO\n",sizeof(ag_journal_history_t));return 0;}
