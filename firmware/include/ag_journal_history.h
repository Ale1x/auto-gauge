#ifndef AG_JOURNAL_HISTORY_H
#define AG_JOURNAL_HISTORY_H
#include "ag_history.h"
#define AG_JOURNAL_SECTOR_BYTES 4096u
#define AG_JOURNAL_MAX_SECTORS 512u
#define AG_JOURNAL_ENTRIES_PER_SECTOR 30u
#define AG_JOURNAL_ENTRY_BYTES 128u
#define AG_JOURNAL_FOOTER_OFFSET 3968u
/* Trusted synchronous durable IO. Offsets bounded to configured geometry.
 * One owner task/lock, no reentry; no hardware adapter supplied. */
typedef struct {
    void *context;
    size_t (*read)(void *,uint32_t,uint8_t *,size_t);
    size_t (*program)(void *,uint32_t,const uint8_t *,size_t);
    bool (*erase_sector)(void *,uint32_t);
} ag_journal_io_t;
typedef enum {
    AG_JOURNAL_OK=0, AG_JOURNAL_EMPTY, AG_JOURNAL_INVALID, AG_JOURNAL_IO_ERROR,
    AG_JOURNAL_READ_ONLY, AG_JOURNAL_CORRUPT, AG_JOURNAL_EXHAUSTED,
    AG_JOURNAL_NEW_SESSION_REQUIRED, AG_JOURNAL_SESSION_REUSED,
    AG_JOURNAL_NOT_RETAINED, AG_JOURNAL_CURSOR_AHEAD, AG_JOURNAL_CLOCK_REGRESSION,
    AG_JOURNAL_OUTPUT_SHORT
} ag_journal_result_t;
typedef struct {
    uint64_t generation,next_serial,initial_serial,retained_first_serial,evicted_samples;
    uint64_t next_sequence,initial_sequence,last_ms;
    uint8_t session_id[16];
    bool serial_exhausted,sequence_exhausted,has_samples;
} ag_journal_info_t;
typedef struct {
    ag_journal_io_t io;ag_journal_info_t info;
    uint64_t generations[AG_JOURNAL_MAX_SECTORS];
    uint8_t states[AG_JOURNAL_MAX_SECTORS],counts[AG_JOURNAL_MAX_SECTORS],samples[AG_JOURNAL_MAX_SECTORS];
    uint64_t first_generation;
    uint16_t sectors,current_index;
    bool initialized,ready,writable,writer_attached,blank,pending_rollover;
} ag_journal_history_t;
/* 2..512 sectors4KiB (<=2MiB H); scan only, never erases/defaults. Open never
 * attaches a new boot to the old SID; fresh trusted SID is mandatory. */
ag_journal_result_t ag_journal_open(ag_journal_history_t *,const ag_journal_io_t *,uint16_t sectors);
/* Explicit blank-only bootstrap. Initial counters are internal test/recovery
 * metadata; production sequence starts1. Caller guarantees SID freshness. */
ag_journal_result_t ag_journal_bootstrap(ag_journal_history_t *,const uint8_t sid[16],uint64_t first_sequence,uint64_t first_serial);
/* Append a session-start control entry, preserving retained samples. Rejects
 * current/retained SID; freshness versus fully evicted history is caller's duty. */
ag_journal_result_t ag_journal_begin_session(ag_journal_history_t *,const uint8_t fresh_sid[16],uint64_t first_sequence);
ag_journal_result_t ag_journal_append(ag_journal_history_t *,uint8_t pid,uint32_t ecu,uint64_t ms,
    ag_sample_status_t,const uint8_t *raw,size_t length);
/* Copies a typed <=16-record window, no pointers into flash. READ_ONLY provides
 * no operational page/ACK; unknown torn/corrupt entries are never guessed away. */
ag_journal_result_t ag_journal_window(ag_journal_history_t *,const uint8_t sid[16],uint64_t after,ag_history_snapshot_t *);
ag_journal_result_t ag_journal_make_page(ag_journal_history_t *,const uint8_t sid[16],uint64_t after,size_t limit,
    char *,size_t,ag_history_page_info_t *);
/* Stateless: never erases records or stores a map of client cursors. */
ag_journal_result_t ag_journal_acknowledge(ag_journal_history_t *,const uint8_t sid[16],uint64_t through);
#endif
