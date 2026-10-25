/* Independent catalog oracle uses production record queries after real simulated
 * journal rollover/eviction; no writes are permitted by the catalog calls. */
#include "ag_journal_history.h"
#include "ag_flash_history_sim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[4*4096];static ag_flash_history_sim_t sim;static ag_journal_history_t history;
static void boot(void){
    assert(ag_flash_history_sim_init(&sim,bytes,sizeof(bytes)));ag_flash_history_io_t base=ag_flash_history_sim_io(&sim);
    ag_journal_io_t io={base.context,base.read,base.program,base.erase_sector};assert(ag_journal_open(&history,&io,4)==AG_JOURNAL_EMPTY);
    uint8_t sid[16]={250};assert(ag_journal_bootstrap(&history,sid,1,1)==AG_JOURNAL_OK);
}
static void populate(unsigned sessions){
    boot();for(unsigned i=1;i<=sessions;++i){uint8_t sid[16]={(uint8_t)i};assert(ag_journal_begin_session(&history,sid,1)==AG_JOURNAL_OK);
        if(i%3){uint8_t raw=(uint8_t)i;assert(ag_journal_append(&history,13,0x7e8,1,AG_SAMPLE_VALID,&raw,1)==AG_JOURNAL_OK);}}
}
static void test_eviction_matches_query_oracle(void){
    populate(200);bool expected[256]={false},actual[256]={false};size_t expected_count=0,actual_count=0;
    for(unsigned i=1;i<=250;++i){uint8_t sid[16]={(uint8_t)i};ag_history_snapshot_t snapshot;
        ag_journal_result_t result=ag_journal_window(&history,sid,0,&snapshot);
        assert(result==AG_JOURNAL_OK||result==AG_JOURNAL_NOT_RETAINED);expected[i]=result==AG_JOURNAL_OK;if(expected[i])++expected_count;
    }
    assert(!expected[1]&&!expected[250]&&expected[200]);
    uint8_t cursor[16]={0},ids[16][16];bool more=true;
    for(unsigned page=0;more&&page<32;++page){size_t count=99,programs=sim.programs,erases=sim.erases;
        assert(ag_journal_sessions(&history,page?cursor:NULL,7,ids,&count,&more)==AG_JOURNAL_OK&&count<=7);
        assert(sim.programs==programs&&sim.erases==erases);
        for(size_t i=0;i<count;++i){assert(memcmp(ids[i],cursor,16)>0&&!actual[ids[i][0]]);actual[ids[i][0]]=true;++actual_count;memcpy(cursor,ids[i],16);}
    }
    assert(!more&&expected_count==actual_count&&!memcmp(expected,actual,sizeof(actual)));
    uint8_t fresh[16]={251};assert(ag_journal_begin_session(&history,fresh,1)==AG_JOURNAL_OK);
    size_t count=0;assert(ag_journal_sessions(&history,cursor,1,ids,&count,&more)==AG_JOURNAL_OK&&count==1&&ids[0][0]==251&&!more);
}
static void test_corruption_and_transactional_outputs(void){
    populate(200);unsigned oldest=(unsigned)((history.first_generation-1)%history.sectors);
    bytes[oldest*4096u+128u+52u]^=1; /* A retained noncurrent entry's CRC is broken. */
    uint8_t ids[16][16];memset(ids,0xa5,sizeof(ids));size_t count=99;bool more=true;
    size_t programs=sim.programs,erases=sim.erases;
    assert(ag_journal_sessions(&history,NULL,16,ids,&count,&more)==AG_JOURNAL_READ_ONLY);
    assert(count==99&&more&&ids[0][0]==0xa5&&sim.programs==programs&&sim.erases==erases&&!history.writable);
    assert(ag_journal_sessions(&history,NULL,16,ids,&count,&more)==AG_JOURNAL_READ_ONLY&&count==99&&ids[0][0]==0xa5);
}
static void test_cursor_and_limits_preserve_buffers(void){
    populate(4);uint8_t ids[16][16],zero[16]={0};memset(ids,0x55,sizeof(ids));size_t count=99;bool more=true;
    assert(ag_journal_sessions(&history,NULL,0,ids,&count,&more)==AG_JOURNAL_INVALID&&count==99);
    assert(ag_journal_sessions(&history,NULL,17,ids,&count,&more)==AG_JOURNAL_INVALID&&count==99);
    assert(ag_journal_sessions(&history,zero,16,ids,&count,&more)==AG_JOURNAL_INVALID&&count==99&&more&&ids[15][15]==0x55);
    uint8_t last[16];memset(last,0xff,sizeof(last));assert(ag_journal_sessions(&history,last,16,ids,&count,&more)==AG_JOURNAL_OK&&count==0&&!more);
    for(size_t i=0;i<sizeof(ids);++i)assert(((uint8_t *)ids)[i]==0);
}
int main(void){test_eviction_matches_query_oracle();test_corruption_and_transactional_outputs();test_cursor_and_limits_preserve_buffers();puts("PASS: 3 independent journal-catalog groups; simulated flash only");return 0;}
