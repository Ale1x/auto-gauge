/* Independent integration oracles. Host handoffs and explicit sink doubles;
 * no controller, hardware qualification, actual flash or radio. */
#include "ag_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    ag_runtime_t runtime;
    ag_runtime_store_result_t begin_result, append_result;
    unsigned begins, attempts, committed;
    bool commit_on_error;
    ag_history_record_t attempted[32];
} fixture_t;
static ag_runtime_store_result_t begin(void *context, const uint8_t sid[16]) {
    fixture_t *f = context;
    assert(sid[0] == 0x91);
    ++f->begins;
    return f->begin_result;
}
static ag_runtime_store_result_t append(void *context, const ag_history_record_t *record) {
    fixture_t *f = context;
    assert(f->attempts < 32);
    f->attempted[f->attempts++] = *record;
    if (f->append_result == AG_RUNTIME_STORE_OK || f->commit_on_error) ++f->committed;
    return f->append_result;
}
static ag_can_policy_t allowed(void) {
    return (ag_can_policy_t){.power = AG_POWER_OBD_AND_USB, .ignition = AG_IGN_QUALIFIED_ON,
        .hardware_validated = true, .rails_valid = true, .diagnostic_session_active = true};
}
static ag_config_t configuration(void) {
    ag_config_t result = {.schema_version = 1, .revision = 1, .dashboard_count = 1,
        .brightness_mode = AG_BRIGHTNESS_MANUAL, .manual_percent = 50};
    result.dashboards[0] = (ag_dashboard_t){.id = 1, .widget_count = 1};
    result.dashboards[0].widgets[0] = (ag_widget_t){.id = 1, .pid = 12, .rect = {0, 0, 10000, 10000}};
    return result;
}
static ag_runtime_job_t job(uint32_t ecu) {
    ag_runtime_job_t result = {.job = {.pid = 12,
        .profile = {AG_CAN_ID_11, 500000, ecu - 8, ecu},
        .interval_ms = 100, .response_timeout_ms = 50, .stale_after_ms = 5000}};
    ag_obd_capabilities_init(&result.capabilities, ecu);
    ag_pid_mask_set(&result.capabilities.advertised, 12);
    return result;
}
static void initialize(fixture_t *f, size_t jobs) {
    memset(f, 0, sizeof(*f));
    ag_config_t config = configuration();
    ag_runtime_job_t list[2] = {job(0x7e8), job(0x7e9)};
    ag_runtime_sink_t sink = {f, begin, append};
    uint8_t sid[16] = {0x91};
    assert(ag_runtime_init(&f->runtime, &config, list, jobs, sid, &sink));
    assert(ag_runtime_start(&f->runtime) && f->begins == 1);
}
static ag_simulated_dispatch_t dispatch(fixture_t *f, const ag_can_policy_t *policy, uint64_t now) {
    ag_simulated_dispatch_t out;
    assert(ag_runtime_prepare(&f->runtime, policy, now));
    assert(ag_runtime_dispatch(&f->runtime, policy, now, &out));
    assert(out.request.length == 2 && out.request.bytes[0] == 1 && out.request.bytes[1] == 12);
    return out;
}
static ag_can_frame_t sample_frame(uint32_t ecu) {
    return (ag_can_frame_t){.id = ecu, .length = 8, .data = {4, 0x41, 12, 0x1f, 0x40}};
}
static void acquire(fixture_t *f, const ag_can_policy_t *policy, uint64_t now) {
    ag_simulated_dispatch_t handoff = dispatch(f, policy, now);
    ag_can_frame_t frame = sample_frame(handoff.profile.response_id);
    assert(ag_runtime_receive(&f->runtime, policy, &frame, now + 1) == AG_SCHED_RX_ACCEPTED);
}
static void revoke(ag_can_policy_t *policy, unsigned variant) {
    switch (variant) {
    case 0: policy->power = AG_POWER_USB_ONLY; break;
    case 1: policy->power = AG_POWER_UNKNOWN; break;
    case 2: policy->power = AG_POWER_NONE; break;
    case 3: policy->ignition = AG_IGN_UNKNOWN; break;
    case 4: policy->ignition = AG_IGN_OFF; break;
    case 5: policy->hardware_validated = false; break;
    case 6: policy->rails_valid = false; break;
    case 7: policy->standby_requested = true; break;
    case 8: policy->bus_off = true; break;
    default: policy->diagnostic_session_active = false; break;
    }
}
static void test_all_policy_revocations(void) {
    for (unsigned variant = 0; variant < 10; ++variant) {
        fixture_t f;
        initialize(&f, 1);
        ag_can_policy_t policy = allowed();
        assert(ag_runtime_prepare(&f.runtime, &policy, 0));
        revoke(&policy, variant);
        ag_simulated_dispatch_t out, empty = {0};
        memset(&out, 0xaa, sizeof(out));
        assert(!ag_runtime_dispatch(&f.runtime, &policy, 1, &out));
        assert(memcmp(&out, &empty, sizeof(out)) == 0);
        assert(f.runtime.scheduler.pending_index == SIZE_MAX && f.runtime.scheduler.active_index == SIZE_MAX);
        policy = allowed();
        assert(!ag_runtime_dispatch(&f.runtime, &policy, 2, &out));

        initialize(&f, 1);
        policy = allowed();
        (void)dispatch(&f, &policy, 0);
        ag_can_frame_t first = {.id = 0x7e8, .length = 8, .data = {0x10, 9, 0x41, 12, 0x1f, 0x40}};
        assert(ag_runtime_receive(&f.runtime, &policy, &first, 1) == AG_SCHED_RX_WAITING);
        assert(ag_runtime_fc_prepare(&f.runtime, &policy, 1));
        revoke(&policy, variant);
        ag_can_frame_t fc, no_fc = {0};
        memset(&fc, 0xaa, sizeof(fc));
        assert(!ag_runtime_fc_dispatch(&f.runtime, &policy, 2, &fc));
        assert(memcmp(&fc, &no_fc, sizeof(fc)) == 0);
        assert(f.runtime.scheduler.active_index == SIZE_MAX && !f.runtime.scheduler.rx.active);
        policy = allowed();
        assert(!ag_runtime_fc_dispatch(&f.runtime, &policy, 3, &fc));
        assert(f.attempts == 0 && f.committed == 0);
    }
}
static void test_global_pid_budget_provenance_and_invalidity(void) {
    fixture_t f;
    initialize(&f, 2);
    ag_can_policy_t policy = allowed();
    acquire(&f, &policy, 0);
    acquire(&f, &policy, 2);
    assert(f.runtime.scheduler.jobs[0].sample.has_value && f.runtime.scheduler.jobs[1].sample.has_value);
    assert(f.runtime.count == 1 && f.runtime.rate_limited == 1);
    assert(f.runtime.queue[f.runtime.head].ecu_source == 0x7e8);
    for (unsigned n = 0; n < 20; ++n) assert(ag_runtime_tick(&f.runtime, &policy, 3));
    assert(f.runtime.count == 1);
    acquire(&f, &policy, 1000);
    assert(f.runtime.count == 2);
    assert(ag_runtime_drain(&f.runtime) && ag_runtime_drain(&f.runtime));
    assert(f.attempted[0].monotonic_ms == 1 && f.attempted[1].monotonic_ms == 1001);
    assert(f.attempted[0].pid == 12 && f.attempted[0].raw_length == 2);
    assert(f.attempted[0].raw[0] == 0x1f && f.attempted[0].raw[1] == 0x40);
    /* Same PID across two sources still has one budget, including stale quality. */
    assert(ag_runtime_tick(&f.runtime, &policy, 7000));
    assert(f.runtime.count == 1 && f.runtime.queue[f.runtime.head].status == AG_SAMPLE_STALE);
    assert(f.runtime.queue[f.runtime.head].raw_length == 0);
    for (size_t i = 0; i < sizeof(f.runtime.queue[f.runtime.head].raw); ++i)
        assert(f.runtime.queue[f.runtime.head].raw[i] == 0);
    assert(ag_runtime_tick(&f.runtime, &policy, 8000) && f.runtime.count == 1);

    initialize(&f, 1);
    ag_simulated_dispatch_t handoff = dispatch(&f, &policy, 0);
    ag_can_frame_t wrong = sample_frame(handoff.profile.response_id + 1);
    assert(ag_runtime_receive(&f.runtime, &policy, &wrong, 1) == AG_SCHED_RX_IGNORED);
    assert(f.runtime.count == 0 && !f.runtime.scheduler.jobs[0].sample.has_value);
    wrong = sample_frame(handoff.profile.response_id);
    wrong.data[2] = 13;
    assert(ag_runtime_receive(&f.runtime, &policy, &wrong, 2) == AG_SCHED_RX_IGNORED);
    assert(f.runtime.count == 0);
    ag_can_frame_t correct = sample_frame(handoff.profile.response_id);
    assert(ag_runtime_receive(&f.runtime, &policy, &correct, 3) == AG_SCHED_RX_ACCEPTED);
    assert(f.runtime.count == 1 && f.runtime.queue[f.runtime.head].monotonic_ms == 3);
}
static void test_busy_and_ambiguous_sink(void) {
    fixture_t f;
    initialize(&f, 1);
    ag_can_policy_t policy = allowed();
    acquire(&f, &policy, 0);
    f.append_result = AG_RUNTIME_STORE_BUSY;
    assert(!ag_runtime_drain(&f.runtime) && f.runtime.count == 1 && !f.runtime.fault);
    assert(f.attempts == 1 && f.committed == 0);
    f.append_result = AG_RUNTIME_STORE_OK;
    assert(ag_runtime_drain(&f.runtime) && f.runtime.count == 0 && f.runtime.persisted == 1);
    assert(memcmp(&f.attempted[0], &f.attempted[1], sizeof(f.attempted[0])) == 0);

    initialize(&f, 1);
    acquire(&f, &policy, 0);
    f.append_result = AG_RUNTIME_STORE_AMBIGUOUS;
    f.commit_on_error = true;
    assert(!ag_runtime_drain(&f.runtime) && f.runtime.fault && f.runtime.storage_ambiguous);
    assert(f.runtime.count == 1 && f.committed == 1 && f.runtime.persisted == 0);
    assert(!ag_runtime_drain(&f.runtime) && f.attempts == 1);
    ag_simulated_dispatch_t out;
    assert(!ag_runtime_prepare(&f.runtime, &policy, 2000));
    assert(!ag_runtime_dispatch(&f.runtime, &policy, 2000, &out) && out.request.length == 0);

    initialize(&f, 1);
    for (unsigned n = 0; n < 25; ++n) acquire(&f, &policy, n * 1000);
    assert(f.runtime.count == 16 && f.runtime.producer_dropped == 9);
    assert(f.runtime.queue[f.runtime.head].monotonic_ms == 1);
    for (unsigned n = 0; n < 16; ++n) assert(ag_runtime_drain(&f.runtime));
    assert(f.runtime.count == 0 && f.runtime.persisted == 16 && f.committed == 16);
    assert(!ag_runtime_tick(&f.runtime, &policy, 0) && f.runtime.fault);
}
int main(void) {
    test_all_policy_revocations();
    test_global_pid_budget_provenance_and_invalidity();
    test_busy_and_ambiguous_sink();
    puts("PASS: 3 independent runtime review groups (10 request + 10 FC guards, global PID budget, sink oracle)");
    return 0;
}
