#include "ag_journal_endpoint.h"
#include "ag_flash_history_sim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t bytes[8192],sid_a[16]={1},sid_b[16]={2},unknown[16]={3};
static ag_flash_history_sim_t sim;
static ag_journal_history_t journal;
static void init(void) {
    assert(ag_flash_history_sim_init(&sim,bytes,sizeof(bytes)));
    ag_flash_history_io_t base=ag_flash_history_sim_io(&sim);
    ag_journal_io_t io={base.context,base.read,base.program,base.erase_sector};
    assert(ag_journal_open(&journal,&io,2)==AG_JOURNAL_EMPTY);
    assert(ag_journal_bootstrap(&journal,sid_a,1,1)==AG_JOURNAL_OK);
}
static void append(unsigned number) {
    uint8_t raw[2]={0x1a,(uint8_t)number};
    assert(ag_journal_append(&journal,12,0x7e8,number,AG_SAMPLE_VALID,raw,2)==AG_JOURNAL_OK);
}
int main(void) {
    init();for(unsigned n=1;n<=75;++n)append(n);
    ag_history_snapshot_t snapshot;
    ag_device_history_read_t adapter=ag_journal_endpoint_read;
    size_t programs=sim.programs,erases=sim.erases;
    assert(adapter(&journal,sid_a,0,&snapshot)==AG_DEVICE_HISTORY_OK);
    assert(snapshot.count==16 && snapshot.records[0].sequence==31);
    char page[4097];ag_history_page_info_t info;
    assert(ag_history_make_page(&snapshot,sid_a,0,16,page,sizeof(page),&info)==AG_HISTORY_OK);
    assert(info.has_loss && info.lost_first==1 && info.lost_last==30);
    assert(adapter(&journal,sid_a,70,&snapshot)==AG_DEVICE_HISTORY_OK);
    assert(snapshot.count==5 && snapshot.records[0].sequence==71);
    assert(adapter(&journal,sid_a,75,&snapshot)==AG_DEVICE_HISTORY_OK);
    assert(ag_history_make_page(&snapshot,sid_a,75,16,page,sizeof(page),&info)==AG_HISTORY_OK);
    assert(info.record_count==0 && !info.has_loss && info.through_sequence==75);
    assert(adapter(&journal,sid_a,76,&snapshot)==AG_DEVICE_HISTORY_CURSOR_AHEAD);
    assert(snapshot.count==0 && snapshot.next_sequence==0);
    assert(adapter(&journal,unknown,0,&snapshot)==AG_DEVICE_HISTORY_NOT_RETAINED);
    assert(sim.programs==programs && sim.erases==erases);

    ag_journal_io_t io=journal.io;
    assert(ag_journal_open(&journal,&io,2)==AG_JOURNAL_OK);
    assert(!journal.writer_attached);
    assert(adapter(&journal,sid_a,70,&snapshot)==AG_DEVICE_HISTORY_OK && snapshot.count==5);
    assert(ag_journal_begin_session(&journal,sid_b,1)==AG_JOURNAL_OK);
    assert(adapter(&journal,sid_b,0,&snapshot)==AG_DEVICE_HISTORY_OK && snapshot.count==0);
    append(1);
    assert(adapter(&journal,sid_a,70,&snapshot)==AG_DEVICE_HISTORY_OK && snapshot.count==5);
    assert(adapter(&journal,sid_b,0,&snapshot)==AG_DEVICE_HISTORY_OK && snapshot.count==1);

    ag_flash_history_sim_fail_at(&sim,1,0);
    assert(adapter(&journal,sid_b,0,&snapshot)==AG_DEVICE_HISTORY_UNAVAILABLE);
    assert(snapshot.count==0 && snapshot.next_sequence==0);
    ag_flash_history_sim_reboot(&sim);
    assert(ag_journal_open(&journal,&io,2)==AG_JOURNAL_OK);
    bytes[journal.current_index*4096u+128u+52u]^=1;
    assert(adapter(&journal,sid_b,0,&snapshot)==AG_DEVICE_HISTORY_READ_ONLY);
    assert(snapshot.count==0 && !journal.writable);
    assert(adapter(NULL,sid_a,0,&snapshot)==AG_DEVICE_HISTORY_UNAVAILABLE);
    assert(adapter(&journal,sid_a,0,NULL)==AG_DEVICE_HISTORY_UNAVAILABLE);
    assert(!sim.invalid_access);
    puts("PASS: journal typed endpoint adapter, paging/reopen/errors/no mutation");
    return 0;
}
