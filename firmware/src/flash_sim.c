#include "ag_flash_sim.h"
#include <string.h>

void ag_flash_sim_init(ag_flash_sim_t *flash) {
    if (flash == NULL) return;
    memset(flash, 0, sizeof(*flash));
    memset(flash->bytes, 0xff, sizeof(flash->bytes));
}
void ag_flash_sim_reboot(ag_flash_sim_t *flash) {
    if (flash == NULL) return;
    flash->operations = flash->reads = flash->programs = flash->erases = 0;
    flash->fail_call = flash->cut_after = 0;
    flash->corrupt_program_call = 0;
    flash->last_program_offset = 0;
    flash->last_program_length = 0;
    memset(flash->trace_lengths, 0, sizeof(flash->trace_lengths));
    memset(flash->trace_operations, 0, sizeof(flash->trace_operations));
    flash->fault_latched = flash->invalid_access = false;
}
void ag_flash_sim_fail_at(ag_flash_sim_t *flash, size_t call, size_t cut_after_bytes) {
    if (flash == NULL) return;
    ag_flash_sim_reboot(flash);
    flash->fail_call = call;
    flash->cut_after = cut_after_bytes;
}
static size_t begin(ag_flash_sim_t *flash, uint32_t offset, size_t length, ag_flash_sim_operation_t operation) {
    if (flash == NULL) return 0;
    if (offset > AG_CONFIG_STORE_BYTES || length > AG_CONFIG_STORE_BYTES - offset) {
        flash->invalid_access = true;
        return 0;
    }
    if (flash->fault_latched) return 0;
    ++flash->operations;
    if (flash->operations <= 16) {
        flash->trace_lengths[flash->operations - 1] = length;
        flash->trace_operations[flash->operations - 1] = operation;
    }
    flash->last_operation = operation;
    flash->last_length = length;
    if (flash->fail_call != 0 && flash->operations == flash->fail_call) {
        flash->fault_latched = true;
        return flash->cut_after < length ? flash->cut_after : length;
    }
    return length;
}
static size_t read_sim(void *context, uint32_t offset, uint8_t *output, size_t length) {
    ag_flash_sim_t *flash = context;
    if (output == NULL || flash == NULL) return 0;
    ++flash->reads;
    const size_t amount = begin(flash, offset, length, AG_FLASH_SIM_READ);
    if (amount > 0) memcpy(output, flash->bytes + offset, amount);
    return amount;
}
static size_t program_sim(void *context, uint32_t offset, const uint8_t *data, size_t length) {
    ag_flash_sim_t *flash = context;
    if (data == NULL || flash == NULL) return 0;
    ++flash->programs;
    flash->last_program_offset = offset;
    flash->last_program_length = length;
    const size_t amount = begin(flash, offset, length, AG_FLASH_SIM_PROGRAM);
    for (size_t i = 0; i < amount; ++i)
        if ((flash->bytes[offset + i] & data[i]) != data[i]) { flash->invalid_access = true; return 0; }
    for (size_t i = 0; i < amount; ++i) flash->bytes[offset + i] &= data[i];
    if (amount > 0 && flash->programs == flash->corrupt_program_call) flash->bytes[offset] ^= 1;
    return amount;
}
static bool erase_sim(void *context, uint32_t offset, size_t length) {
    ag_flash_sim_t *flash = context;
    if (flash == NULL) return false;
    ++flash->erases;
    if (offset % AG_CONFIG_SLOT_BYTES != 0 || length != AG_CONFIG_SLOT_BYTES) {
        flash->invalid_access = true;
        return false;
    }
    const size_t amount = begin(flash, offset, length, AG_FLASH_SIM_ERASE);
    if (amount > 0) memset(flash->bytes + offset, 0xff, amount);
    return amount == length;
}
ag_config_flash_io_t ag_flash_sim_io(ag_flash_sim_t *flash) {
    return (ag_config_flash_io_t){flash, read_sim, program_sim, erase_sim};
}
