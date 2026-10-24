#include "ag_diagnostics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static ag_can_profile_t profile11(void) {
    return (ag_can_profile_t){AG_CAN_ID_11, 500000, 0x7e0, 0x7e8};
}
static ag_can_policy_t active_policy(void) {
    ag_can_policy_t policy;
    ag_can_policy_init(&policy);
    policy.power = AG_POWER_OBD;
    policy.ignition = AG_IGN_QUALIFIED_ON;
    policy.hardware_validated = policy.rails_valid = policy.diagnostic_session_active = true;
    return policy;
}
static ag_can_frame_t rpm_frame(void) {
    return (ag_can_frame_t){.id = 0x7e8, .length = 8, .data = {4, 0x41, 0x0c, 0x1f, 0x40}};
}
static ag_can_frame_t first_frame(void) {
    return (ag_can_frame_t){.id = 0x7e8, .length = 8, .data = {0x10, 8, 0, 1, 2, 3, 4, 5}};
}

static void test_decoder_vectors(void) {
    const struct {uint8_t pid, a, b; size_t length; double value; ag_unit_t unit;} vectors[] = {
        {4, 0, 0, 3, 0, AG_UNIT_PERCENT}, {4, 255, 0, 3, 100, AG_UNIT_PERCENT},
        {5, 0, 0, 3, -40, AG_UNIT_CELSIUS}, {5, 255, 0, 3, 215, AG_UNIT_CELSIUS},
        {12, 0x1f, 0x40, 4, 2000, AG_UNIT_RPM}, {12, 255, 255, 4, 16383.75, AG_UNIT_RPM},
        {13, 0, 0, 3, 0, AG_UNIT_KM_PER_HOUR}, {13, 255, 0, 3, 255, AG_UNIT_KM_PER_HOUR},
        {15, 40, 0, 3, 0, AG_UNIT_CELSIUS}, {15, 0, 0, 3, -40, AG_UNIT_CELSIUS},
        {16, 1, 56, 4, 3.12, AG_UNIT_GRAMS_PER_SECOND},
        {16, 255, 255, 4, 655.35, AG_UNIT_GRAMS_PER_SECOND},
        {17, 128, 0, 3, 50.19607843137255, AG_UNIT_PERCENT}
    };
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
        const uint8_t payload[] = {0x41, vectors[i].pid, vectors[i].a, vectors[i].b};
        ag_measurement_t sample;
        assert(ag_obd_parse_measurement(payload, vectors[i].length, vectors[i].pid, 0x7e8, 1234, &sample) == AG_OBD_OK);
        assert(sample.status == AG_SAMPLE_VALID && sample.has_value && sample.unit == vectors[i].unit);
        assert(fabs(sample.value - vectors[i].value) < 1e-9);
        assert(sample.raw_length == vectors[i].length - 2 && sample.raw[0] == vectors[i].a);
        assert(sample.ecu_source == 0x7e8 && sample.updated_ms == 1234);
    }
    printf("PASS: %zu independent decoder vectors\n", sizeof(vectors) / sizeof(vectors[0]));
}
static void test_decoder_malformed_and_unavailable(void) {
    ag_measurement_t sample;
    const uint8_t good[] = {0x41, 0x0c, 0x1f, 0x40};
    assert(ag_obd_parse_measurement(good, 4, 12, 0x7e8, 1, &sample) == AG_OBD_OK);
    assert(ag_obd_parse_measurement(good, 3, 12, 0x7e8, 2, &sample) == AG_OBD_MALFORMED);
    assert(!sample.has_value && isnan(sample.value) && sample.raw_length == 0);
    assert(ag_obd_parse_measurement(good, 4, 5, 0x7e8, 3, &sample) == AG_OBD_WRONG_PID);
    const uint8_t wrong_service[] = {0x43, 12, 0, 0};
    assert(ag_obd_parse_measurement(wrong_service, 4, 12, 0x7e8, 4, &sample) == AG_OBD_WRONG_SERVICE);
    const uint8_t negative[] = {0x7f, 1, 0x12};
    assert(ag_obd_parse_measurement(negative, 3, 12, 0x7e8, 5, &sample) == AG_OBD_NEGATIVE_RESPONSE);
    assert(sample.status == AG_SAMPLE_ERROR && !sample.has_value && sample.negative_response_code == 0x12);
    const uint8_t unsupported[] = {0x41, 0xff, 0};
    assert(ag_obd_parse_measurement(unsupported, 3, 255, 0x7e8, 6, &sample) == AG_OBD_UNSUPPORTED_PID);
    assert(sample.status == AG_SAMPLE_UNSUPPORTED && isnan(sample.value));
    assert(ag_obd_parse_measurement(NULL, 0, 12, 0x7e8, 7, &sample) == AG_OBD_MALFORMED);
    ag_measurement_unavailable(&sample, AG_SAMPLE_STALE);
    assert(sample.status == AG_SAMPLE_STALE && !sample.has_value && isnan(sample.value));
    ag_measurement_unavailable(&sample, AG_SAMPLE_UNAVAILABLE);
    assert(sample.status == AG_SAMPLE_UNAVAILABLE && sample.raw_length == 0);
}
static void test_capability_bit_order_and_ecu_isolation(void) {
    ag_obd_capabilities_t caps;
    ag_obd_capabilities_init(&caps, 0x7e8);
    const uint8_t page0[] = {0x41, 0, 0x08, 0x18, 0, 1};
    assert(ag_obd_capabilities_apply(&caps, 0x7e8, page0, 6));
    assert(caps.pages_seen == 1);
    assert(ag_pid_mask_has(&caps.advertised, 5) && ag_pid_mask_has(&caps.advertised, 12));
    assert(ag_pid_mask_has(&caps.advertised, 13) && ag_pid_mask_has(&caps.advertised, 32));
    assert(!ag_pid_mask_has(&caps.advertised, 4));
    ag_pid_mask_t usable;
    ag_obd_usable_pids(&caps, &usable);
    assert(ag_pid_mask_has(&usable, 12) && !ag_pid_mask_has(&usable, 32));
    const ag_obd_capabilities_t before = caps;
    assert(!ag_obd_capabilities_apply(&caps, 0x7e9, page0, 6));
    assert(memcmp(&before, &caps, sizeof(caps)) == 0);
    assert(!ag_obd_capabilities_apply(&caps, 0x7e8, page0, 5));
    const uint8_t page20[] = {0x41, 0x20, 0x80, 0, 0, 0};
    assert(ag_obd_capabilities_apply(&caps, 0x7e8, page20, 6));
    assert(ag_pid_mask_has(&caps.advertised, 33));
    const uint8_t empty0[] = {0x41, 0, 0, 0, 0, 0};
    assert(ag_obd_capabilities_apply(&caps, 0x7e8, empty0, 6));
    assert(!ag_pid_mask_has(&caps.advertised, 12) && ag_pid_mask_has(&caps.advertised, 33));
    const uint8_t last_page[] = {0x41, 0xe0, 0xff, 0xff, 0xff, 0xff};
    assert(ag_obd_capabilities_apply(&caps, 0x7e8, last_page, 6));
    assert(ag_pid_mask_has(&caps.advertised, 255) && !ag_pid_mask_has(&caps.advertised, 0));
}
static void test_profiles_and_isotp_single_frame(void) {
    ag_isotp_rx_t rx;
    ag_can_profile_t profile = profile11();
    assert(ag_can_profile_valid(&profile));
    assert(ag_isotp_init(&rx, &profile, 50));
    ag_can_frame_t frame = rpm_frame();
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_COMPLETE);
    assert(ag_isotp_feed(&rx, &frame, 1) == AG_ISOTP_BUSY);
    uint8_t payload[AG_ISOTP_CAPACITY]; size_t length;
    assert(!ag_isotp_take(&rx, payload, 3, &length));
    assert(ag_isotp_take(&rx, payload, sizeof(payload), &length));
    assert(length == 4 && memcmp(payload, frame.data + 1, 4) == 0);
    assert(!ag_isotp_take(&rx, payload, sizeof(payload), &length));
    profile = (ag_can_profile_t){AG_CAN_ID_29, 250000, 0x18da10f1, 0x18daf110};
    assert(ag_can_profile_valid(&profile) && ag_isotp_init(&rx, &profile, 50));
    frame.id = profile.response_id;
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_IGNORED);
    frame.extended = true;
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_COMPLETE);
    profile.bitrate = 125000;
    assert(!ag_can_profile_valid(&profile));
    profile = profile11(); profile.response_id = 0x800;
    assert(!ag_can_profile_valid(&profile));
    assert(!ag_isotp_init(&rx, &profile, 50));
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_NOT_INITIALIZED);
}
static void test_isotp_multiframe_and_sequence_wrap(void) {
    ag_isotp_rx_t rx;
    const ag_can_profile_t profile = profile11();
    assert(ag_isotp_init(&rx, &profile, 50));
    uint8_t expected[AG_ISOTP_CAPACITY], actual[AG_ISOTP_CAPACITY];
    for (size_t i = 0; i < sizeof(expected); ++i) expected[i] = (uint8_t)i;
    ag_can_frame_t frame = first_frame();
    frame.data[0] = 0x11; frame.data[1] = 0;
    memcpy(frame.data + 2, expected, 6);
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_WAITING);
    size_t position = 6; unsigned sequence = 1;
    while (position < sizeof(expected)) {
        frame.data[0] = (uint8_t)(0x20 | (sequence & 15));
        const size_t remaining = sizeof(expected) - position;
        const size_t amount = remaining < 7 ? remaining : 7;
        memset(frame.data + 1, 0xaa, 7);
        memcpy(frame.data + 1, expected + position, amount);
        position += amount;
        const ag_isotp_result_t result = ag_isotp_feed(&rx, &frame, sequence);
        assert(result == (position == sizeof(expected) ? AG_ISOTP_COMPLETE : AG_ISOTP_WAITING));
        ++sequence;
    }
    size_t length;
    assert(ag_isotp_take(&rx, actual, sizeof(actual), &length));
    assert(length == sizeof(expected) && memcmp(actual, expected, length) == 0);
}

static void test_isotp_errors_timeout_and_wrong_ecu(void) {
    ag_isotp_rx_t rx;
    const ag_can_profile_t profile = profile11();
    assert(ag_isotp_init(&rx, &profile, 50));
    ag_can_frame_t frame = first_frame();
    assert(ag_isotp_feed(&rx, &frame, 100) == AG_ISOTP_WAITING);
    ag_can_frame_t other = {.id = 0x7e9, .length = 8, .data = {0x21, 6, 7}};
    assert(ag_isotp_feed(&rx, &other, 140) == AG_ISOTP_IGNORED);
    assert(ag_isotp_poll(&rx, 149) == AG_ISOTP_WAITING);
    assert(ag_isotp_poll(&rx, 150) == AG_ISOTP_TIMEOUT);
    assert(!rx.active && !rx.complete);
    ag_isotp_reset(&rx);
    assert(ag_isotp_feed(&rx, &frame, 0) == AG_ISOTP_WAITING);
    frame.data[0] = 0x22;
    assert(ag_isotp_feed(&rx, &frame, 1) == AG_ISOTP_SEQUENCE_ERROR);
    assert(!rx.active);
    frame = first_frame(); frame.data[0] = 0x11; frame.data[1] = 1;
    assert(ag_isotp_feed(&rx, &frame, 2) == AG_ISOTP_OVERFLOW);
    frame = first_frame(); frame.data[1] = 7;
    assert(ag_isotp_feed(&rx, &frame, 3) == AG_ISOTP_MALFORMED);
    frame = first_frame(); frame.data[1] = 20;
    assert(ag_isotp_feed(&rx, &frame, 4) == AG_ISOTP_WAITING);
    frame.data[0] = 0x21; frame.length = 2;
    assert(ag_isotp_feed(&rx, &frame, 5) == AG_ISOTP_MALFORMED);
    frame = rpm_frame(); frame.fd = true;
    assert(ag_isotp_feed(&rx, &frame, 6) == AG_ISOTP_UNSUPPORTED);
    frame.fd = false; frame.length = 9;
    assert(ag_isotp_feed(&rx, &frame, 7) == AG_ISOTP_MALFORMED);
    frame = rpm_frame(); frame.data[0] = 0;
    assert(ag_isotp_feed(&rx, &frame, 8) == AG_ISOTP_MALFORMED);
    frame = first_frame();
    assert(ag_isotp_feed(&rx, &frame, 9) == AG_ISOTP_WAITING);
    assert(ag_isotp_poll(&rx, 8) == AG_ISOTP_CLOCK_REGRESSION);
    assert(!rx.active && !rx.complete);
}

static void test_isotp_bounded_random_inputs(void) {
    ag_isotp_rx_t rx;
    const ag_can_profile_t profile = profile11();
    assert(ag_isotp_init(&rx, &profile, 50));
    uint32_t random = 0x51a70001;
    for (unsigned i = 0; i < 10000; ++i) {
        ag_can_frame_t frame = {.id = 0x7e8};
        for (size_t byte = 0; byte < 8; ++byte) {
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            frame.data[byte] = (uint8_t)(random >> 24);
        }
        frame.length = (uint8_t)(random % 11);
        frame.rtr = (random & 8) != 0;
        frame.fd = (random & 16) != 0;
        if (i % 7 == 0) frame.id = 0x7e9;
        (void)ag_isotp_feed(&rx, &frame, i);
        assert(rx.received_length <= AG_ISOTP_CAPACITY && rx.expected_length <= AG_ISOTP_CAPACITY);
        if (rx.complete) {
            uint8_t payload[AG_ISOTP_CAPACITY]; size_t length;
            assert(ag_isotp_take(&rx, payload, sizeof(payload), &length));
            assert(length > 0 && length <= AG_ISOTP_CAPACITY);
        }
    }
}

static ag_poll_job_config_t rpm_job(void) {
    return (ag_poll_job_config_t){.pid = 12, .profile = profile11(), .interval_ms = 200,
                                 .response_timeout_ms = 100, .stale_after_ms = 50};
}
static ag_obd_capabilities_t caps_for_job(const ag_poll_job_config_t *job) {
    ag_obd_capabilities_t caps;
    ag_obd_capabilities_init(&caps, job->profile.response_id);
    ag_pid_mask_set(&caps.advertised, job->pid);
    return caps;
}
static void test_scheduler_safety_recheck_and_timeout(void) {
    ag_scheduler_t scheduler;
    ag_scheduler_init(&scheduler);
    const ag_poll_job_config_t job = rpm_job();
    ag_obd_capabilities_t caps = caps_for_job(&job);
    assert(ag_scheduler_add(&scheduler, &job, &caps));
    assert(scheduler.jobs[0].sample.status == AG_SAMPLE_UNAVAILABLE);
    ag_can_policy_t policy = active_policy();
    assert(ag_scheduler_prepare(&scheduler, &policy, 0));
    policy.ignition = AG_IGN_OFF;
    ag_simulated_dispatch_t dispatch;
    assert(!ag_scheduler_dispatch(&scheduler, &policy, 1, &dispatch));
    assert(dispatch.request.length == 0 && scheduler.active_index == SIZE_MAX);
    policy = active_policy();
    assert(ag_scheduler_prepare(&scheduler, &policy, 2));
    assert(ag_scheduler_dispatch(&scheduler, &policy, 2, &dispatch));
    assert(dispatch.request.bytes[0] == 1 && dispatch.request.bytes[1] == 12);
    assert(!ag_scheduler_prepare(&scheduler, &policy, 2));
    ag_can_frame_t frame = rpm_frame(); frame.id = 0x7e9;
    assert(ag_scheduler_receive(&scheduler, &policy, &frame, 3) == AG_SCHED_RX_IGNORED);
    frame = rpm_frame(); frame.data[2] = 5;
    assert(ag_scheduler_receive(&scheduler, &policy, &frame, 3) == AG_SCHED_RX_IGNORED);
    frame = rpm_frame();
    assert(ag_scheduler_receive(&scheduler, &policy, &frame, 4) == AG_SCHED_RX_ACCEPTED);
    assert(scheduler.jobs[0].sample.has_value && scheduler.jobs[0].sample.value == 2000);
    assert(!ag_scheduler_prepare(&scheduler, &policy, 4));
    assert(ag_scheduler_tick(&scheduler, &policy, 54));
    assert(scheduler.jobs[0].sample.status == AG_SAMPLE_STALE && isnan(scheduler.jobs[0].sample.value));
    assert(ag_scheduler_prepare(&scheduler, &policy, 204));
    assert(ag_scheduler_dispatch(&scheduler, &policy, 204, &dispatch));
    assert(ag_scheduler_tick(&scheduler, &policy, 304));
    assert(scheduler.jobs[0].sample.status == AG_SAMPLE_TIMEOUT && !scheduler.jobs[0].sample.has_value);
    assert(!ag_scheduler_prepare(&scheduler, &policy, 304));
    assert(ag_scheduler_prepare(&scheduler, &policy, 504));
    policy.power = AG_POWER_USB_ONLY;
    assert(!ag_scheduler_dispatch(&scheduler, &policy, 505, &dispatch));
    assert(dispatch.request.length == 0);
}
static void test_scheduler_round_robin_clock_and_profile_bounds(void) {
    ag_scheduler_t scheduler;
    ag_scheduler_init(&scheduler);
    ag_poll_job_config_t job = rpm_job();
    ag_obd_capabilities_t caps = caps_for_job(&job);
    ag_pid_mask_set(&caps.advertised, 5);
    assert(ag_scheduler_add(&scheduler, &job, &caps));
    assert(!ag_scheduler_add(&scheduler, &job, &caps));
    job.pid = 5;
    assert(ag_scheduler_add(&scheduler, &job, &caps));
    ag_can_policy_t policy = active_policy();
    ag_simulated_dispatch_t dispatch;
    assert(ag_scheduler_prepare(&scheduler, &policy, 0));
    assert(ag_scheduler_dispatch(&scheduler, &policy, 0, &dispatch));
    assert(dispatch.request.bytes[1] == 12);
    ag_can_frame_t frame = rpm_frame();
    assert(ag_scheduler_receive(&scheduler, &policy, &frame, 1) == AG_SCHED_RX_ACCEPTED);
    assert(ag_scheduler_prepare(&scheduler, &policy, 1));
    assert(ag_scheduler_dispatch(&scheduler, &policy, 1, &dispatch));
    assert(dispatch.request.bytes[1] == 5);
    policy.standby_requested = true;
    assert(ag_scheduler_receive(&scheduler, &policy, &frame, 2) == AG_SCHED_RX_BLOCKED);
    assert(scheduler.active_index == SIZE_MAX && scheduler.jobs[0].sample.status == AG_SAMPLE_UNAVAILABLE);
    policy = active_policy();
    assert(!ag_scheduler_tick(&scheduler, &policy, 1) && scheduler.clock_fault);
    assert(!ag_scheduler_prepare(&scheduler, &policy, 3));
    ag_scheduler_init(&scheduler);
    assert(ag_scheduler_add(&scheduler, &job, &caps));
    assert(ag_scheduler_prepare(&scheduler, &policy, UINT64_MAX - 99));
    assert(!ag_scheduler_dispatch(&scheduler, &policy, UINT64_MAX - 99, &dispatch));
    assert(scheduler.clock_fault && dispatch.request.length == 0);
}

static void test_every_dispatch_gate_can_revoke_prepared_work(void) {
    for (unsigned gate = 0; gate < 8; ++gate) {
        ag_scheduler_t scheduler;
        ag_scheduler_init(&scheduler);
        const ag_poll_job_config_t job = rpm_job();
        const ag_obd_capabilities_t caps = caps_for_job(&job);
        assert(ag_scheduler_add(&scheduler, &job, &caps));
        ag_can_policy_t policy = active_policy();
        assert(ag_scheduler_prepare(&scheduler, &policy, 0));
        switch (gate) {
            case 0: policy.power = AG_POWER_USB_ONLY; break;
            case 1: policy.ignition = AG_IGN_UNKNOWN; break;
            case 2: policy.ignition = AG_IGN_OFF; break;
            case 3: policy.standby_requested = true; break;
            case 4: policy.hardware_validated = false; break;
            case 5: policy.rails_valid = false; break;
            case 6: policy.bus_off = true; break;
            case 7: policy.diagnostic_session_active = false; break;
        }
        ag_simulated_dispatch_t dispatch;
        memset(&dispatch, 0xff, sizeof(dispatch));
        assert(!ag_scheduler_dispatch(&scheduler, &policy, 1, &dispatch));
        assert(dispatch.request.length == 0 && dispatch.request.bytes[0] == 0);
        assert(scheduler.pending_index == SIZE_MAX && scheduler.active_index == SIZE_MAX);
    }
}

int main(void) {
    test_decoder_vectors();
    test_decoder_malformed_and_unavailable();
    test_capability_bit_order_and_ecu_isolation();
    test_profiles_and_isotp_single_frame();
    test_isotp_multiframe_and_sequence_wrap();
    test_isotp_errors_timeout_and_wrong_ecu();
    test_isotp_bounded_random_inputs();
    test_scheduler_safety_recheck_and_timeout();
    test_scheduler_round_robin_clock_and_profile_bounds();
    test_every_dispatch_gate_can_revoke_prepared_work();
    printf("PASS: 10 diagnostics groups, 10000 malformed/random frame trials; RX=%zu bytes, scheduler=%zu bytes; no physical TX\n",
           sizeof(ag_isotp_rx_t), sizeof(ag_scheduler_t));
    return 0;
}
