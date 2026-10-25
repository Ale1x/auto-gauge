#include "ag_journal_history.h"
#include "ag_flash_history_sim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[4*4096];static ag_flash_history_sim_t sim;static ag_journal_history_t journal;
static ag_journal_io_t io(void){ag_flash_history_io_t i=ag_flash_history_sim_io(&sim);return (ag_journal_io_t){i.context,i.read,i.program,i.erase_sector};}
static void boot(void){assert(ag_flash_history_sim_init(&sim,flash,sizeof(flash)));ag_journal_io_t i=io();assert(ag_journal_open(&journal,&i,4)==AG_JOURNAL_EMPTY);uint8_t sid[16]={30};assert(ag_journal_bootstrap(&journal,sid,1,1)==AG_JOURNAL_OK);}
int main(void){
    boot();uint8_t sids[16][16];size_t count=0;bool more=true;assert(ag_journal_sessions(&journal,NULL,16,sids,&count,&more)==AG_JOURNAL_OK&&count==1&&sids[0][0]==30&&!more);
    for(unsigned i=1;i<=20;++i){uint8_t sid[16]={(uint8_t)(21-i)};assert(ag_journal_begin_session(&journal,sid,1)==AG_JOURNAL_OK);uint8_t raw=0;
        assert(ag_journal_append(&journal,0x0d,0x7e8,1,AG_SAMPLE_VALID,&raw,1)==AG_JOURNAL_OK);}
    size_t total=0;uint8_t cursor[16]={0};
    for(unsigned page=0;page<8;++page){size_t programs=sim.programs,erases=sim.erases;
        assert(ag_journal_sessions(&journal,total?cursor:NULL,3,sids,&count,&more)==AG_JOURNAL_OK&&count<=3&&sim.programs==programs&&sim.erases==erases);
        for(size_t i=0;i<count;++i){assert(!total||memcmp(sids[i],cursor,16)>0);memcpy(cursor,sids[i],16);++total;}
        if(!more)break;
    }
    assert(total==21); /* Includes intentionally empty bootstrap session. */
    assert(ag_journal_sessions(&journal,cursor,16,sids,&count,&more)==AG_JOURNAL_OK&&count==0&&!more);
    uint8_t zero[16]={0};assert(ag_journal_sessions(&journal,zero,16,sids,&count,&more)==AG_JOURNAL_INVALID);
    memset(sids,0xa5,sizeof(sids));count=99;more=true;ag_flash_history_sim_fail_at(&sim,1,0);
    assert(ag_journal_sessions(&journal,NULL,16,sids,&count,&more)!=AG_JOURNAL_OK&&count==99&&sids[0][0]==0xa5&&more);
    puts("PASS: journal session catalog sorted paging/empty/IO; no physical IO");return 0;
}
