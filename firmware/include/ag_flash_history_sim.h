#ifndef AG_FLASH_HISTORY_SIM_H
#define AG_FLASH_HISTORY_SIM_H
#include "ag_flash_history.h"
#define AG_FLASH_HISTORY_TRACE 512u
typedef struct {
    uint8_t *bytes;uint32_t length;
    size_t operations,fail_call,cut_after,reads,programs,erases;
    size_t trace_lengths[AG_FLASH_HISTORY_TRACE];
    uint8_t trace_kinds[AG_FLASH_HISTORY_TRACE]; /* 0 read,1 program,2 erase */
    bool fault,invalid_access;
} ag_flash_history_sim_t;
/* Caller owns bounded RAM storage. init erases SIMULATED bytes explicitly. */
bool ag_flash_history_sim_init(ag_flash_history_sim_t *,uint8_t *,uint32_t);
void ag_flash_history_sim_reboot(ag_flash_history_sim_t *);
void ag_flash_history_sim_fail_at(ag_flash_history_sim_t *,size_t call,size_t bytes);
ag_flash_history_io_t ag_flash_history_sim_io(ag_flash_history_sim_t *);
#endif
