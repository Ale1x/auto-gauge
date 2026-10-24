#ifndef AG_FLASH_SIM_H
#define AG_FLASH_SIM_H
#include "ag_config_store.h"

typedef enum { AG_FLASH_SIM_READ = 0, AG_FLASH_SIM_PROGRAM, AG_FLASH_SIM_ERASE } ag_flash_sim_operation_t;
typedef struct {
    uint8_t bytes[AG_CONFIG_STORE_BYTES];
    size_t operations, reads, programs, erases;
    size_t fail_call, cut_after;
    size_t corrupt_program_call;
    uint32_t last_program_offset;
    size_t last_program_length;
    size_t trace_lengths[16];
    ag_flash_sim_operation_t trace_operations[16];
    ag_flash_sim_operation_t last_operation;
    size_t last_length;
    bool fault_latched, invalid_access;
} ag_flash_sim_t;
void ag_flash_sim_init(ag_flash_sim_t *);
/* Clear injected power-loss state/counters, retaining simulated flash bytes. */
void ag_flash_sim_reboot(ag_flash_sim_t *);
void ag_flash_sim_fail_at(ag_flash_sim_t *, size_t call, size_t cut_after_bytes);
ag_config_flash_io_t ag_flash_sim_io(ag_flash_sim_t *);
#endif
