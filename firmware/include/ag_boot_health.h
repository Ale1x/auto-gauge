#ifndef AG_BOOT_HEALTH_H
#define AG_BOOT_HEALTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define AG_BOOT_RECORD_BYTES 96u
#define AG_BOOT_HEALTH_REQUIRED_MIN 0x0fu
#define AG_BOOT_HEALTH_KNOWN 0xffu
/* Trusted local evidence: config access, log access, CAN guard, runtime clock;
 * additional bits are platform checks (display, heap, peripherals, etc.). */
typedef enum { AG_BOOT_IMAGE_VALID=0,AG_BOOT_IMAGE_PENDING,AG_BOOT_IMAGE_INVALID,AG_BOOT_IMAGE_FACTORY } ag_boot_image_state_t;
typedef struct {uint32_t running_address,boot_address;ag_boot_image_state_t state;uint8_t digest[32];} ag_boot_observation_t;
typedef enum { AG_BOOT_LOAD_OK=0,AG_BOOT_LOAD_EMPTY,AG_BOOT_LOAD_IO } ag_boot_load_t;
typedef struct {
    void *context;
    ag_boot_load_t (*read)(void *,uint8_t out[AG_BOOT_RECORD_BYTES]);
    /* Atomic replacement of one durable blob; error MAY follow commit. No
     * rollback/erase on error. Readback must observe successful replacement. */
    bool (*write)(void *,const uint8_t bytes[AG_BOOT_RECORD_BYTES]);
    bool (*observe)(void *,uint32_t image_bytes,ag_boot_observation_t *);
    bool (*target_matches)(void *,uint32_t address,uint32_t image_bytes,const uint8_t digest[32]);
    bool (*mark_valid)(void *); /* SDK call, failure can be ambiguous. */
    bool allow_factory_bootstrap; /* Explicit trusted local maintenance profile. */
} ag_boot_hooks_t;
typedef enum { AG_BOOT_OK=0,AG_BOOT_EMPTY,AG_BOOT_INVALID,AG_BOOT_IO,AG_BOOT_POLICY,
    AG_BOOT_BUSY,AG_BOOT_NEEDS_HEALTH,AG_BOOT_AWAITING_REBOOT,AG_BOOT_RECOVERED_FALLBACK,
    AG_BOOT_UNKNOWN } ag_boot_result_t;
typedef struct {
    ag_boot_hooks_t hooks;
    uint64_t generation,accepted_floor,candidate_counter;
    uint32_t target_address,image_bytes,required_health,health_witness;
    uint8_t digest[32],pending; /* 0 stable,1 selected intent,2 health persisted */
    bool initialized,ready,busy,fault;
} ag_boot_health_t;
/* One static serialized owner/boot. No init formats or repairs storage. */
ag_boot_result_t ag_boot_health_open(ag_boot_health_t *,const ag_boot_hooks_t *,uint32_t required_health);
/* Explicit locally authorized provisioning only, on EMPTY store + stable VALID
 * running image. Never called automatically by open/recovery/wire commands. */
ag_boot_result_t ag_boot_health_provision(ag_boot_health_t *,uint64_t baseline_floor);
bool ag_boot_health_floor(const ag_boot_health_t *,uint64_t *);
/* Before ANY boot selection: persist counter+target+digest intent and readback.
 * Signature/slot hash must already be verified by installer; rechecks target. */
ag_boot_result_t ag_boot_health_stage(ag_boot_health_t *,uint64_t counter,uint32_t target,
                                      uint32_t image_bytes,const uint8_t digest[32]);
ag_boot_result_t ag_boot_health_reconcile(ag_boot_health_t *);
/* Local health evidence only, never a wire boolean. Persist health witness,
 * call SDK mark-valid, then advance floor. A failure stops until fresh open and
 * reconciliation; never blindly repeat a possibly committed SDK/storage call. */
ag_boot_result_t ag_boot_health_confirm(ag_boot_health_t *,uint32_t passed_health);
#endif
