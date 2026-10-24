#include "ag_diagnostics.h"
#include <string.h>

static void cancel_transaction(ag_scheduler_t *scheduler) {
    scheduler->pending_index = SIZE_MAX;
    scheduler->active_index = SIZE_MAX;
    ag_isotp_reset(&scheduler->rx);
}
static bool future_ms(uint64_t now_ms, uint32_t interval, uint64_t *future) {
    if (now_ms > UINT64_MAX - interval) return false;
    *future = now_ms + interval;
    return true;
}
static void clock_fault(ag_scheduler_t *scheduler) {
    scheduler->clock_fault = true;
    cancel_transaction(scheduler);
    for (size_t i = 0; i < scheduler->count; ++i)
        ag_measurement_unavailable(&scheduler->jobs[i].sample, AG_SAMPLE_ERROR);
}
static void finish(ag_scheduler_t *scheduler, ag_sample_status_t status, uint64_t now_ms) {
    if (scheduler->active_index >= scheduler->count) return;
    ag_poll_job_t *job = &scheduler->jobs[scheduler->active_index];
    if (status != AG_SAMPLE_VALID) ag_measurement_unavailable(&job->sample, status);
    if (!future_ms(now_ms, job->config.interval_ms, &job->next_due_ms)) clock_fault(scheduler);
    cancel_transaction(scheduler);
}

void ag_scheduler_init(ag_scheduler_t *scheduler) {
    if (scheduler == NULL) return;
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->pending_index = scheduler->active_index = SIZE_MAX;
}
bool ag_scheduler_add(ag_scheduler_t *scheduler, const ag_poll_job_config_t *config,
                       const ag_obd_capabilities_t *caps) {
    if (scheduler == NULL || config == NULL || caps == NULL
        || scheduler->count >= AG_POLL_JOB_CAPACITY
        || scheduler->active_index != SIZE_MAX || scheduler->pending_index != SIZE_MAX
        || !ag_can_profile_valid(&config->profile)
        || caps->ecu_source != config->profile.response_id
        || !ag_obd_pid_implemented(config->pid)
        || !ag_pid_mask_has(&caps->advertised, config->pid)
        || config->interval_ms == 0 || config->response_timeout_ms == 0
        || config->stale_after_ms == 0) return false;
    for (size_t i = 0; i < scheduler->count; ++i)
        if (scheduler->jobs[i].config.profile.bitrate != config->profile.bitrate
            || scheduler->jobs[i].config.profile.id_format != config->profile.id_format)
            return false;
        else if (scheduler->jobs[i].config.pid == config->pid
            && scheduler->jobs[i].config.profile.response_id == config->profile.response_id
            && scheduler->jobs[i].config.profile.id_format == config->profile.id_format)
            return false;
    ag_poll_job_t *job = &scheduler->jobs[scheduler->count++];
    memset(job, 0, sizeof(*job));
    job->config = *config;
    job->sample.pid = config->pid;
    job->sample.ecu_source = config->profile.response_id;
    ag_measurement_unavailable(&job->sample, AG_SAMPLE_UNAVAILABLE);
    return true;
}

bool ag_scheduler_tick(ag_scheduler_t *scheduler, const ag_can_policy_t *policy, uint64_t now_ms) {
    if (scheduler == NULL || scheduler->clock_fault) return false;
    if (scheduler->has_time && now_ms < scheduler->last_ms) { clock_fault(scheduler); return false; }
    scheduler->last_ms = now_ms;
    scheduler->has_time = true;
    if (!ag_can_tx_allowed(policy)) {
        cancel_transaction(scheduler);
        for (size_t i = 0; i < scheduler->count; ++i)
            ag_measurement_unavailable(&scheduler->jobs[i].sample, AG_SAMPLE_UNAVAILABLE);
        return true;
    }
    if (scheduler->active_index < scheduler->count) {
        ag_poll_job_t *job = &scheduler->jobs[scheduler->active_index];
        if (now_ms - scheduler->active_started_ms >= job->config.response_timeout_ms)
            finish(scheduler, AG_SAMPLE_TIMEOUT, now_ms);
    }
    for (size_t i = 0; i < scheduler->count; ++i) {
        ag_poll_job_t *job = &scheduler->jobs[i];
        if (job->sample.status == AG_SAMPLE_VALID
            && now_ms - job->sample.updated_ms >= job->config.stale_after_ms)
            ag_measurement_unavailable(&job->sample, AG_SAMPLE_STALE);
    }
    return !scheduler->clock_fault;
}
bool ag_scheduler_prepare(ag_scheduler_t *scheduler, const ag_can_policy_t *policy, uint64_t now_ms) {
    if (!ag_scheduler_tick(scheduler, policy, now_ms) || !ag_can_tx_allowed(policy)
        || scheduler->count == 0 || scheduler->active_index != SIZE_MAX
        || scheduler->pending_index != SIZE_MAX) return false;
    for (size_t offset = 0; offset < scheduler->count; ++offset) {
        const size_t index = (scheduler->cursor + offset) % scheduler->count;
        if (now_ms >= scheduler->jobs[index].next_due_ms) {
            scheduler->pending_index = index;
            return true;
        }
    }
    return false;
}
bool ag_scheduler_dispatch(ag_scheduler_t *scheduler, const ag_can_policy_t *policy,
                            uint64_t now_ms, ag_simulated_dispatch_t *dispatch) {
    if (dispatch == NULL) return false;
    memset(dispatch, 0, sizeof(*dispatch));
    if (!ag_scheduler_tick(scheduler, policy, now_ms) || !ag_can_tx_allowed(policy)
        || scheduler->pending_index >= scheduler->count || scheduler->active_index != SIZE_MAX) return false;
    ag_poll_job_t *job = &scheduler->jobs[scheduler->pending_index];
    uint64_t next_due;
    if (!future_ms(now_ms, job->config.interval_ms, &next_due)
        || now_ms > UINT64_MAX - job->config.response_timeout_ms) {
        clock_fault(scheduler); return false;
    }
    ag_pid_mask_t supported;
    ag_pid_mask_clear(&supported);
    ag_pid_mask_set(&supported, job->config.pid);
    if (!ag_obd_make_read_request(policy, &supported, AG_READ_MEASUREMENT, job->config.pid, &dispatch->request))
        return false;
    if (!ag_isotp_init(&scheduler->rx, &job->config.profile, job->config.response_timeout_ms)) {
        memset(dispatch, 0, sizeof(*dispatch));
        cancel_transaction(scheduler); return false;
    }
    dispatch->profile = job->config.profile;
    job->next_due_ms = next_due;
    scheduler->active_index = scheduler->pending_index;
    scheduler->pending_index = SIZE_MAX;
    scheduler->active_started_ms = now_ms;
    scheduler->cursor = (scheduler->active_index + 1) % scheduler->count;
    return true;
}

ag_scheduler_rx_result_t ag_scheduler_receive(ag_scheduler_t *scheduler,
                                              const ag_can_policy_t *policy,
                                              const ag_can_frame_t *frame, uint64_t now_ms) {
    if (!ag_scheduler_tick(scheduler, policy, now_ms) || !ag_can_tx_allowed(policy)) return AG_SCHED_RX_BLOCKED;
    if (scheduler->active_index >= scheduler->count) return AG_SCHED_RX_IGNORED;
    const ag_isotp_result_t result = ag_isotp_feed(&scheduler->rx, frame, now_ms);
    if (result == AG_ISOTP_IGNORED) return AG_SCHED_RX_IGNORED;
    if (result == AG_ISOTP_WAITING || result == AG_ISOTP_IDLE) return AG_SCHED_RX_WAITING;
    if (result != AG_ISOTP_COMPLETE) {
        finish(scheduler, result == AG_ISOTP_TIMEOUT ? AG_SAMPLE_TIMEOUT : AG_SAMPLE_ERROR, now_ms);
        return AG_SCHED_RX_ERROR;
    }
    uint8_t payload[AG_ISOTP_CAPACITY];
    size_t length;
    if (!ag_isotp_take(&scheduler->rx, payload, sizeof(payload), &length)) {
        finish(scheduler, AG_SAMPLE_ERROR, now_ms); return AG_SCHED_RX_ERROR;
    }
    ag_poll_job_t *job = &scheduler->jobs[scheduler->active_index];
    ag_measurement_t sample;
    const ag_obd_result_t parsed = ag_obd_parse_measurement(payload, length, job->config.pid,
                                                          job->config.profile.response_id, now_ms, &sample);
    if (parsed == AG_OBD_WRONG_PID || parsed == AG_OBD_WRONG_SERVICE) return AG_SCHED_RX_IGNORED;
    if (parsed != AG_OBD_OK) {
        job->sample = sample;
        /* Preserve NRC metadata while invalidating the value in the parser. */
        const uint8_t nrc = sample.negative_response_code;
        finish(scheduler, sample.status, now_ms);
        job->sample.negative_response_code = nrc;
        return AG_SCHED_RX_ERROR;
    }
    job->sample = sample;
    finish(scheduler, AG_SAMPLE_VALID, now_ms);
    return AG_SCHED_RX_ACCEPTED;
}
