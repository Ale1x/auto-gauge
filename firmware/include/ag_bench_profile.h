#ifndef AG_BENCH_PROFILE_H
#define AG_BENCH_PROFILE_H
#include "ag_application.h"
#include "ag_gauge_renderer.h"
#include "ag_flash_sim.h"
#include "ag_flash_history_sim.h"
/* Explicit volatile software bench. Never qualifies GPIO/radio/SDK boot health.
 * No physical flash, CAN, display, touch, credentials or real sensor input. */
typedef struct {
    ag_flash_sim_t config_flash;
    uint8_t log_bytes[8192],boot_bytes[AG_BOOT_RECORD_BYTES];
    ag_flash_history_sim_t log_flash;
    ag_config_store_t configuration;
    ag_journal_history_t journal;
    ag_connection_t connection;
    ag_factory_reset_t reset;
    ag_admission_t admission;
    ag_boot_health_t boot;
    ag_can_policy_t policy;
    ag_gauge_scene_t rendered;
    ag_gauge_renderer_t renderer;
    uint32_t pixel_digest;
    size_t rendered_rows;
    uint64_t now_ms;
    size_t can_steps,renders;
    bool boot_present;
} ag_bench_profile_t;
/* Intentionally initializes volatile blank fixtures only. The fresh nonzero SID
 * must come from host fixture/SDK random; never reuse for persistent hardware. */
bool ag_bench_profile_start(ag_bench_profile_t *,ag_application_t *,const uint8_t sid[16]);
#endif
