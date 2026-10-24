#include "ag_flash_history.h"
#include "ag_flash_history_sim.h"
#include "history_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t sid_a[16]={1},sid_b[16]={2},sid_c[16]={3};
static uint32_t test_crc(uint32_t n,const uint8_t *b,size_t length){
    for(size_t i=0;i<length;++i){n^=b[i];for(unsigned bit=0;bit<8;++bit)n=(n>>1)^((n&1)?UINT32_C(0xedb88320):0);}return n;
}
static void put32_test(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;++i)b[i]=(uint8_t)(n>>(8*i));}
static void setup(ag_flash_history_t *s,ag_flash_history_sim_t *sim,uint8_t bytes[8192],uint32_t cap){
    assert(ag_flash_history_sim_init(sim,bytes,8192));ag_flash_history_io_t io=ag_flash_history_sim_io(sim);
    assert(ag_flash_history_open(s,&io,8192,cap)==AG_FLASH_HISTORY_EMPTY);
    assert(ag_flash_history_bootstrap(s,sid_a,1,1)==AG_FLASH_HISTORY_OK);
}
static void append(ag_flash_history_t *s,unsigned value,uint64_t now){
    uint8_t raw=(uint8_t)value;assert(ag_flash_history_append(s,12,0x7e8,now,AG_SAMPLE_VALID,&raw,1)==AG_FLASH_HISTORY_OK);
}
static ag_flash_history_result_t page(ag_flash_history_t *s,const uint8_t sid[16],uint64_t after,char out[4097],ag_history_page_info_t *info){
    return ag_flash_history_make_page(s,sid,after,16,out,4097,info);
}
static void test_ram_interop_loss_ack_and_output_bounds(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;setup(&s,&sim,bytes,3);
    ag_history_source_t ram;assert(ag_history_init(&ram,sid_a,3,1));
    for(unsigned n=1;n<=5;++n){uint8_t raw=(uint8_t)n;append(&s,n,n*10);
        assert(ag_history_append(&ram,12,0x7e8,n*10,AG_SAMPLE_VALID,&raw,1)==AG_HISTORY_OK);}
    assert(s.info.overwritten==2 && s.info.count==3 && s.info.first_serial==3);
    ag_history_snapshot_t snap;assert(ag_history_export(&ram,&snap)==AG_HISTORY_OK);
    for(uint64_t cursor=0;cursor<=5;++cursor){char actual[4097],expected[4097];ag_history_page_info_t a,b;
        assert(page(&s,sid_a,cursor,actual,&a)==AG_FLASH_HISTORY_OK);
        assert(ag_history_make_page(&snap,sid_a,cursor,16,expected,sizeof(expected),&b)==AG_HISTORY_OK);
        assert(strcmp(actual,expected)==0 && a.written==b.written);
    }
    char before[4097],after[4097];ag_history_page_info_t a,b;
    assert(page(&s,sid_a,0,before,&a)==AG_FLASH_HISTORY_OK);
    size_t writes=sim.programs,erases=sim.erases;
    assert(ag_flash_history_acknowledge(&s,sid_a,5)==AG_FLASH_HISTORY_OK);
    assert(page(&s,sid_a,0,after,&b)==AG_FLASH_HISTORY_OK && strcmp(before,after)==0);
    assert(sim.programs==writes && sim.erases==erases);
    after[0]='X';assert(page(&s,sid_a,0,after,&b)==AG_FLASH_HISTORY_OK && strcmp(before,after)==0);
    char tiny[2]={'X','X'};memset(&b,0xff,sizeof(b));
    assert(ag_flash_history_make_page(&s,sid_a,0,16,tiny,sizeof(tiny),&b)==AG_FLASH_HISTORY_OUTPUT_SHORT);
    assert(tiny[0]==0 && b.written==0 && b.record_count==0);
    assert(ag_flash_history_acknowledge(&s,sid_a,6)==AG_FLASH_HISTORY_CURSOR_AHEAD);
}
static void test_reboot_sessions_preserved_and_evicted_not_empty(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;setup(&s,&sim,bytes,3);
    append(&s,1,1000);append(&s,2,2000);
    ag_flash_history_sim_reboot(&sim);ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);ag_flash_history_t boot;
    assert(ag_flash_history_open(&boot,&io,8192,3)==AG_FLASH_HISTORY_OK && !boot.writer_attached);
    uint8_t raw=1;
    assert(ag_flash_history_append(&boot,12,0x7e8,0,AG_SAMPLE_VALID,&raw,1)==AG_FLASH_HISTORY_NEW_SESSION_REQUIRED);
    assert(ag_flash_history_begin_session(&boot,sid_a,1)==AG_FLASH_HISTORY_SESSION_REUSED);
    assert(ag_flash_history_begin_session(&boot,sid_b,1)==AG_FLASH_HISTORY_OK);
    char out[4097];ag_history_page_info_t info;
    assert(page(&boot,sid_a,0,out,&info)==AG_FLASH_HISTORY_OK && info.record_count==2);
    assert(page(&boot,sid_b,0,out,&info)==AG_FLASH_HISTORY_OK && info.record_count==0 && !info.has_loss);
    append(&boot,3,0);append(&boot,4,1);
    assert(page(&boot,sid_a,0,out,&info)==AG_FLASH_HISTORY_OK && info.record_count==1 && info.lost_first==1 && info.lost_last==1);
    assert(ag_flash_history_begin_session(&boot,sid_a,1)==AG_FLASH_HISTORY_SESSION_REUSED);
    assert(ag_flash_history_begin_session(&boot,sid_c,1)==AG_FLASH_HISTORY_OK);append(&boot,5,0);
    assert(page(&boot,sid_a,0,out,&info)==AG_FLASH_HISTORY_NOT_RETAINED && out[0]==0 && info.written==0);
    assert(page(&boot,sid_b,0,out,&info)==AG_FLASH_HISTORY_OK && info.record_count==2 && !info.has_loss);
    assert(ag_flash_history_acknowledge(&boot,sid_a,2)==AG_FLASH_HISTORY_NOT_RETAINED);
    assert(boot.info.overwritten==2 && boot.info.next_serial==6);
}
static void test_raw_status_terminal_and_geometry(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;setup(&s,&sim,bytes,3);
    uint8_t raw[65];memset(raw,0xab,sizeof(raw));size_t writes=sim.programs;
    assert(ag_flash_history_append(&s,12,0x7e8,0,AG_SAMPLE_VALID,raw,65)==AG_FLASH_HISTORY_INVALID && sim.programs==writes);
    assert(ag_flash_history_append(&s,12,0x7e8,0,AG_SAMPLE_VALID,raw,64)==AG_FLASH_HISTORY_OK);
    assert(ag_flash_history_append(&s,12,0x7e8,1,AG_SAMPLE_TIMEOUT,NULL,0)==AG_FLASH_HISTORY_OK);
    assert(ag_flash_history_append(&s,12,0x7e8,0,AG_SAMPLE_VALID,raw,1)==AG_FLASH_HISTORY_CLOCK_REGRESSION);
    char out[4097];ag_history_page_info_t info;assert(page(&s,sid_a,0,out,&info)==AG_FLASH_HISTORY_OK);
    assert(strstr(out,"abababab")!=NULL && strstr(out,"timeout")!=NULL);
    ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
    assert(ag_flash_history_open(&s,&io,12288,3)==AG_FLASH_HISTORY_INVALID);
    assert(ag_flash_history_open(&s,&io,8192,31)==AG_FLASH_HISTORY_INVALID);
    assert(ag_flash_history_sim_init(&sim,bytes,8192));io=ag_flash_history_sim_io(&sim);
    assert(ag_flash_history_open(&s,&io,8192,3)==AG_FLASH_HISTORY_EMPTY);
    assert(ag_flash_history_bootstrap(&s,sid_a,UINT64_MAX,1)==AG_FLASH_HISTORY_OK);
    append(&s,1,UINT64_MAX);
    assert(s.info.current_exhausted && !s.info.serial_exhausted);
    assert(ag_flash_history_append(&s,12,0x7e8,UINT64_MAX,AG_SAMPLE_VALID,raw,1)==AG_FLASH_HISTORY_EXHAUSTED);
    assert(page(&s,sid_a,UINT64_MAX,out,&info)==AG_FLASH_HISTORY_OK && info.record_count==0);
    assert(ag_flash_history_begin_session(&s,sid_b,1)==AG_FLASH_HISTORY_OK);append(&s,2,0);
    assert(ag_flash_history_sim_init(&sim,bytes,8192));io=ag_flash_history_sim_io(&sim);
    assert(ag_flash_history_open(&s,&io,8192,3)==AG_FLASH_HISTORY_EMPTY);
    assert(ag_flash_history_bootstrap(&s,sid_a,1,UINT64_MAX)==AG_FLASH_HISTORY_OK);append(&s,1,0);
    assert(s.info.serial_exhausted && s.info.next_serial==UINT64_MAX);
    assert(ag_flash_history_begin_session(&s,sid_b,1)==AG_FLASH_HISTORY_EXHAUSTED);
}
static size_t exercise_faults(bool new_session){
    uint8_t bytes[8192],seed[8192],active[4096];ag_flash_history_sim_t sim;ag_flash_history_t s;
    setup(&s,&sim,bytes,2);append(&s,1,1);append(&s,2,2);
    memcpy(seed,bytes,sizeof(seed));memcpy(active,bytes+s.active_bank*4096,4096);
    ag_flash_history_t original=s;ag_flash_history_sim_reboot(&sim);uint8_t raw=3;
    assert((new_session?ag_flash_history_begin_session(&s,sid_b,1)
        :ag_flash_history_append(&s,12,0x7e8,3,AG_SAMPLE_VALID,&raw,1))==AG_FLASH_HISTORY_OK);
    size_t calls=sim.operations;assert(calls<AG_FLASH_HISTORY_TRACE);
    size_t lengths[AG_FLASH_HISTORY_TRACE];uint8_t kinds[AG_FLASH_HISTORY_TRACE];
    memcpy(lengths,sim.trace_lengths,sizeof(lengths));memcpy(kinds,sim.trace_kinds,sizeof(kinds));size_t trials=0;
    for(size_t call=1;call<=calls;++call){size_t length=lengths[call-1];
        size_t step=kinds[call-1]==1?1:(length>8?length/2:1);
        for(size_t cut=0;;){
            memcpy(bytes,seed,sizeof(seed));ag_flash_history_sim_fail_at(&sim,call,cut);s=original;
            ag_flash_history_result_t result=new_session?ag_flash_history_begin_session(&s,sid_b,1)
                :ag_flash_history_append(&s,12,0x7e8,3,AG_SAMPLE_VALID,&raw,1);
            assert(memcmp(active,bytes+original.active_bank*4096,4096)==0 && !sim.invalid_access);
            ag_flash_history_sim_reboot(&sim);ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);ag_flash_history_t recovered;
            ag_flash_history_result_t opened=ag_flash_history_open(&recovered,&io,8192,2);
            assert(opened==AG_FLASH_HISTORY_OK || opened==AG_FLASH_HISTORY_READ_ONLY);
            assert(recovered.ready && !recovered.writer_attached);
            assert(recovered.info.generation==original.info.generation || recovered.info.generation==original.info.generation+1);
            if(result==AG_FLASH_HISTORY_OK)assert(recovered.info.generation==original.info.generation+1);
            if(opened==AG_FLASH_HISTORY_READ_ONLY){
                size_t erases=sim.erases;assert(ag_flash_history_begin_session(&recovered,sid_c,1)==AG_FLASH_HISTORY_READ_ONLY);
                assert(sim.erases==erases);
            }
            ++trials;if(cut==length)break;cut=length-cut<step?length:cut+step;
        }
    }return trials;
}
static void test_power_loss_bootstrap_append_and_session_metadata(void){
    size_t trials=exercise_faults(false)+exercise_faults(true);
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;
    assert(ag_flash_history_sim_init(&sim,bytes,8192));ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
    assert(ag_flash_history_open(&s,&io,8192,2)==AG_FLASH_HISTORY_EMPTY);
    ag_flash_history_sim_reboot(&sim);
    assert(ag_flash_history_bootstrap(&s,sid_a,1,1)==AG_FLASH_HISTORY_OK);
    size_t calls=sim.operations,lengths[AG_FLASH_HISTORY_TRACE];uint8_t kinds[AG_FLASH_HISTORY_TRACE];
    assert(calls<AG_FLASH_HISTORY_TRACE);memcpy(lengths,sim.trace_lengths,sizeof(lengths));memcpy(kinds,sim.trace_kinds,sizeof(kinds));
    for(size_t call=1;call<=calls;++call){size_t n=lengths[call-1],step=kinds[call-1]==1?1:n;
        for(size_t cut=0;;){
            assert(ag_flash_history_sim_init(&sim,bytes,8192));assert(ag_flash_history_open(&s,&io,8192,2)==AG_FLASH_HISTORY_EMPTY);
            ag_flash_history_sim_fail_at(&sim,call,cut);(void)ag_flash_history_bootstrap(&s,sid_a,1,1);
            ag_flash_history_sim_reboot(&sim);ag_flash_history_result_t result=ag_flash_history_open(&s,&io,8192,2);
            assert(result==AG_FLASH_HISTORY_EMPTY || result==AG_FLASH_HISTORY_CORRUPT || result==AG_FLASH_HISTORY_OK);
            if(result==AG_FLASH_HISTORY_CORRUPT){size_t erase=sim.erases;
                assert(ag_flash_history_bootstrap(&s,sid_b,1,1)==AG_FLASH_HISTORY_CONFLICT && sim.erases==erase);}
            ++trials;if(cut==n)break;cut=n-cut<step?n:cut+step;
        }
    }
    printf("PASS: %zu simulated IO cuts (every programmed byte incl markers; erase/read boundaries)\n",trials);
}
static void test_shared_golden_pages(void){
    for(size_t f=0;f<kHistoryFixtureCount;++f){
        const ag_history_fixture_t *fixture=&kHistoryFixtures[f];uint8_t bytes[32768];
        ag_flash_history_sim_t sim;ag_flash_history_t s;
        assert(ag_flash_history_sim_init(&sim,bytes,sizeof(bytes)));ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
        assert(ag_flash_history_open(&s,&io,sizeof(bytes),(uint32_t)fixture->capacity)==AG_FLASH_HISTORY_EMPTY);
        assert(ag_flash_history_bootstrap(&s,fixture->session,fixture->first_sequence,1)==AG_FLASH_HISTORY_OK);
        for(size_t n=0;n<fixture->record_count;++n){const ag_history_record_t *r=&fixture->records[n];
            assert(ag_flash_history_append(&s,r->pid,r->ecu_source,r->monotonic_ms,r->status,r->raw,r->raw_length)==AG_FLASH_HISTORY_OK);}
        for(size_t n=0;n<fixture->read_count;++n){const ag_history_read_fixture_t *r=&fixture->reads[n];
            char out[4097];ag_history_page_info_t info;
            assert(ag_flash_history_make_page(&s,fixture->session,r->cursor,r->limit,out,sizeof(out),&info)==AG_FLASH_HISTORY_OK);
            assert(strcmp(out,r->json)==0 && info.record_count==r->record_count && info.through_sequence==r->through);
        }
        for(size_t n=0;n<fixture->ack_count;++n){
            uint32_t count=s.info.count;
            assert(ag_flash_history_acknowledge(&s,fixture->session,fixture->acks[n].through)==AG_FLASH_HISTORY_OK && s.info.count==count);
        }
    }
}
static void test_bitflip_short_read_and_no_default(void){
    uint8_t bytes[8192],seed[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;setup(&s,&sim,bytes,3);
    append(&s,1,1);append(&s,2,2);memcpy(seed,bytes,sizeof(seed));unsigned newest=s.active_bank;
    for(size_t byte=0;byte<4096;++byte){memcpy(bytes,seed,sizeof(seed));bytes[newest*4096+byte]^=1;
        ag_flash_history_sim_reboot(&sim);ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
        ag_flash_history_t recovered;assert(ag_flash_history_open(&recovered,&io,8192,3)==AG_FLASH_HISTORY_READ_ONLY);
        assert(recovered.info.generation==s.info.generation-1 && !recovered.writable);
    }
    memcpy(bytes,seed,sizeof(seed));ag_flash_history_sim_reboot(&sim);ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
    ag_flash_history_t read;assert(ag_flash_history_open(&read,&io,8192,3)==AG_FLASH_HISTORY_OK);size_t calls=sim.operations;
    ag_flash_history_sim_fail_at(&sim,calls-1,1);
    ag_flash_history_result_t result=ag_flash_history_open(&read,&io,8192,3);
    assert(result==AG_FLASH_HISTORY_READ_ONLY && read.ready && !read.writable);
    memcpy(bytes,seed,sizeof(seed));bytes[0]^=1;bytes[4096]^=1;ag_flash_history_sim_reboot(&sim);
    assert(ag_flash_history_open(&read,&io,8192,3)==AG_FLASH_HISTORY_CORRUPT && !read.ready);
    assert(ag_flash_history_bootstrap(&read,sid_b,1,1)==AG_FLASH_HISTORY_CONFLICT && sim.erases==0);
}
static void test_two_mebibyte_bounded_model(void){
    static uint8_t bytes[AG_FLASH_HISTORY_MAX_BYTES];ag_flash_history_sim_t sim;ag_flash_history_t s;
    assert(ag_flash_history_sim_init(&sim,bytes,sizeof(bytes)));ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);
    assert(ag_flash_history_open(&s,&io,sizeof(bytes),8190)==AG_FLASH_HISTORY_EMPTY);
    assert(ag_flash_history_bootstrap(&s,sid_a,1,1)==AG_FLASH_HISTORY_OK);append(&s,1,0);
    ag_flash_history_t reboot;assert(ag_flash_history_open(&reboot,&io,sizeof(bytes),8190)==AG_FLASH_HISTORY_OK);
    assert(reboot.info.count==1 && !sim.invalid_access);
}
static void test_metadata_conflicts_and_uncertain_retention(void){
    uint8_t bytes[8192];ag_flash_history_sim_t sim;ag_flash_history_t s;setup(&s,&sim,bytes,3);append(&s,1,1);
    memcpy(bytes+(1-s.active_bank)*4096,bytes+s.active_bank*4096,4096);
    ag_flash_history_io_t io=ag_flash_history_sim_io(&sim);ag_flash_history_t recovered;
    assert(ag_flash_history_open(&recovered,&io,8192,3)==AG_FLASH_HISTORY_OK); /* Identical generation/content. */
    uint8_t *record=bytes+4096+128;record[52]^=1;
    uint32_t crc=test_crc(UINT32_MAX,record,116);crc=test_crc(crc,record+120,4)^UINT32_MAX;put32_test(record+116,crc);
    assert(ag_flash_history_open(&recovered,&io,8192,3)==AG_FLASH_HISTORY_CONFLICT && !recovered.ready);
    setup(&s,&sim,bytes,3);append(&s,1,1);
    uint8_t *header=bytes+s.active_bank*4096;memset(header+8,0xff,8);
    put32_test(header+124,test_crc(UINT32_MAX,header,124)^UINT32_MAX);
    assert(ag_flash_history_open(&recovered,&io,8192,3)==AG_FLASH_HISTORY_OK);
    size_t writes=sim.programs,erases=sim.erases;
    assert(ag_flash_history_begin_session(&recovered,sid_b,1)==AG_FLASH_HISTORY_EXHAUSTED);
    assert(sim.programs==writes && sim.erases==erases);
    setup(&s,&sim,bytes,3);append(&s,1,1);assert(ag_flash_history_begin_session(&s,sid_b,1)==AG_FLASH_HISTORY_OK);
    bytes[s.active_bank*4096]^=1;
    assert(ag_flash_history_open(&recovered,&io,8192,3)==AG_FLASH_HISTORY_READ_ONLY);
    char out[4097];ag_history_page_info_t info;
    assert(page(&recovered,sid_b,0,out,&info)==AG_FLASH_HISTORY_READ_ONLY && out[0]==0 && info.written==0);
    assert(ag_flash_history_acknowledge(&recovered,sid_b,0)==AG_FLASH_HISTORY_READ_ONLY);
    assert(page(&recovered,sid_a,0,out,&info)==AG_FLASH_HISTORY_READ_ONLY && info.record_count==1);
    bytes[recovered.active_bank*4096]^=1;
    assert(page(&recovered,sid_a,0,out,&info)==AG_FLASH_HISTORY_IO_ERROR && out[0]==0 && !recovered.ready);
}
int main(void){
    test_ram_interop_loss_ack_and_output_bounds();test_reboot_sessions_preserved_and_evicted_not_empty();
    test_raw_status_terminal_and_geometry();test_power_loss_bootstrap_append_and_session_metadata();
    test_bitflip_short_read_and_no_default();test_two_mebibyte_bounded_model();test_shared_golden_pages();
    test_metadata_conflicts_and_uncertain_retention();
    printf("PASS: 8 persistent flash MODEL groups + 6 shared golden cases/12 pages; store=%zu sim=%zu host bytes; no hardware IO\n",sizeof(ag_flash_history_t),sizeof(ag_flash_history_sim_t));return 0;
}
