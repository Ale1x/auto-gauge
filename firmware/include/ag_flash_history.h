#ifndef AG_FLASH_HISTORY_H
#define AG_FLASH_HISTORY_H
#include "ag_history.h"
#define AG_FLASH_HISTORY_SECTOR 4096u
#define AG_FLASH_HISTORY_MAX_BYTES (2u*1024u*1024u)
#define AG_FLASH_HISTORY_HEADER 128u
#define AG_FLASH_HISTORY_RECORD 128u
/* Logical store offsets; trusted synchronous durable IO, serialized/no reentry.
 * No hardware adapter. Short reads/programs return actual bytes completed. */
typedef struct {
    void *context;
    size_t (*read)(void *,uint32_t,uint8_t *,size_t);
    size_t (*program)(void *,uint32_t,const uint8_t *,size_t);
    bool (*erase_sector)(void *,uint32_t);
} ag_flash_history_io_t;
typedef enum {
    AG_FLASH_HISTORY_OK=0, AG_FLASH_HISTORY_EMPTY, AG_FLASH_HISTORY_INVALID,
    AG_FLASH_HISTORY_READ_ONLY, AG_FLASH_HISTORY_IO_ERROR, AG_FLASH_HISTORY_CORRUPT,
    AG_FLASH_HISTORY_CONFLICT, AG_FLASH_HISTORY_NOT_RETAINED, AG_FLASH_HISTORY_SESSION_REUSED,
    AG_FLASH_HISTORY_NEW_SESSION_REQUIRED, AG_FLASH_HISTORY_EXHAUSTED,
    AG_FLASH_HISTORY_CLOCK_REGRESSION, AG_FLASH_HISTORY_CURSOR_AHEAD,
    AG_FLASH_HISTORY_OUTPUT_SHORT
} ag_flash_history_result_t;
typedef struct {
    uint64_t generation, first_serial, next_serial, initial_serial, overwritten;
    uint64_t current_next_sequence, current_initial_sequence, current_last_ms;
    uint8_t current_session[AG_SESSION_ID_BYTES];
    uint32_t capacity,count;
    bool serial_exhausted, current_exhausted, current_has_records;
} ag_flash_history_info_t;
typedef struct {
    ag_flash_history_io_t io;
    ag_flash_history_info_t info;
    uint32_t total_bytes,bank_bytes,capacity;
    unsigned active_bank;
    bool initialized,ready,writable,writer_attached,blank;
} ag_flash_history_t;
/* Total geometry even sectors, 2..512; capacity<=bank available record slots.
 * Open never erases/defaults and never attaches this boot to the old session. */
ag_flash_history_result_t ag_flash_history_open(ag_flash_history_t *,const ag_flash_history_io_t *,
                                                uint32_t total_bytes,uint32_t capacity);
/* Explicit blank-media-only startup; trusted fresh nonzero SID. first_sequence /
 * first_serial are internal recovery/test metadata, never remote commands. */
ag_flash_history_result_t ag_flash_history_bootstrap(ag_flash_history_t *,const uint8_t sid[16],
                                                     uint64_t first_sequence,uint64_t first_serial);
/* New boot uses a trusted fresh SID, never inferred from clock reset. Rejects
 * current/retained SID; fully evicted IDs still MUST NOT be reused by caller.
 * Retains old records. No erase-all, owner change or cloud dependency. */
ag_flash_history_result_t ag_flash_history_begin_session(ag_flash_history_t *,const uint8_t sid[16],
                                                         uint64_t first_sequence);
ag_flash_history_result_t ag_flash_history_append(ag_flash_history_t *,uint8_t pid,uint32_t ecu,
                                                   uint64_t now_ms,ag_sample_status_t,const uint8_t *,size_t);
/* A READ_ONLY result may still provide DIAGNOSTIC page data from an older bank;
 * caller must not interpret that as a complete synchronization of newest data.
 * No retained record for a noncurrent SID => NOT_RETAINED, never empty success. */
ag_flash_history_result_t ag_flash_history_make_page(ag_flash_history_t *,const uint8_t sid[16],
                                                      uint64_t after,size_t limit,char *,size_t,
                                                      ag_history_page_info_t *);
ag_flash_history_result_t ag_flash_history_acknowledge(ag_flash_history_t *,const uint8_t sid[16],uint64_t through);
#endif
