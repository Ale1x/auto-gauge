#ifndef AG_RUNTIME_H
#define AG_RUNTIME_H
#include "ag_diagnostics.h"
#include "ag_journal_history.h"
#define AG_RUNTIME_QUEUE 16u
/* BUSY guarantees no mutation. Every other failure is terminal for this runtime;
 * AMBIGUOUS may already have committed. Callbacks synchronous under owner lock. */
typedef enum { AG_RUNTIME_STORE_OK=0, AG_RUNTIME_STORE_BUSY,
               AG_RUNTIME_STORE_FAILED, AG_RUNTIME_STORE_AMBIGUOUS } ag_runtime_store_result_t;
typedef struct {
    void *context;
    ag_runtime_store_result_t (*begin)(void *,const uint8_t sid[16]);
    ag_runtime_store_result_t (*append)(void *,const ag_history_record_t *);
} ag_runtime_sink_t;
typedef struct {ag_poll_job_config_t job;ag_obd_capabilities_t capabilities;} ag_runtime_job_t;
typedef struct {
    ag_scheduler_t scheduler;
    ag_runtime_sink_t sink;
    ag_measurement_t observed[AG_POLL_JOB_CAPACITY];
    uint64_t last_logged[256];
    bool has_logged[256];
    ag_history_record_t queue[AG_RUNTIME_QUEUE];
    uint8_t session_id[16];
    uint64_t last_ms,producer_dropped,rate_limited,persisted,not_recorded;
    size_t head,count;
    bool initialized,started,has_time,fault,storage_ambiguous;
    bool allow_live_without_logging,logging_disabled;
} ag_runtime_t;
/* Init validates configuration against usable discovered capabilities, and each
 * job against the configuration. Never grants hardware qualification or IGN. */
bool ag_runtime_init(ag_runtime_t *,const ag_config_t *,const ag_runtime_job_t *,size_t,
                     const uint8_t fresh_sid[16],const ag_runtime_sink_t *);
/* Explicit session establishment; BUSY is terminal here, no blind begin retry. */
/* Explicit local policy before start only. A store fault then disables logging
 * permanently for this runtime while live polling/quality continue. No retry or
 * invented persistence/sequence: not_recorded is volatile diagnostics only. */
bool ag_runtime_allow_degraded_logging(ag_runtime_t *);
bool ag_runtime_start(ag_runtime_t *);
/* Caller has quiesced every transport reference to scheduler. Validates before
 * mutation, keeps journal SID/queue/counters and per-PID rate budgets across
 * removal/readdition. No new session or persistence callback. */
bool ag_runtime_reconfigure(ag_runtime_t *,const ag_config_t *,const ag_runtime_job_t *,size_t);
bool ag_runtime_tick(ag_runtime_t *,const ag_can_policy_t *,uint64_t);
bool ag_runtime_prepare(ag_runtime_t *,const ag_can_policy_t *,uint64_t);
bool ag_runtime_dispatch(ag_runtime_t *,const ag_can_policy_t *,uint64_t,ag_simulated_dispatch_t *);
ag_scheduler_rx_result_t ag_runtime_receive(ag_runtime_t *,const ag_can_policy_t *,const ag_can_frame_t *,uint64_t);
bool ag_runtime_fc_prepare(ag_runtime_t *,const ag_can_policy_t *,uint64_t);
bool ag_runtime_fc_dispatch(ag_runtime_t *,const ag_can_policy_t *,uint64_t,ag_can_frame_t *);
/* At most one synchronous append attempt per call. BUSY retains head unchanged.
 * No backend retry after failure/ambiguity. Read fresh monotonic time after IO. */
bool ag_runtime_drain(ag_runtime_t *);
ag_runtime_store_result_t ag_runtime_journal_begin(void *,const uint8_t sid[16]);
ag_runtime_store_result_t ag_runtime_journal_append(void *,const ag_history_record_t *);
#endif
