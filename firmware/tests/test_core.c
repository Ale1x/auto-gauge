#include "ag_core.h"
#include "ag_hal.h"
#include "shared_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t session_id[AG_SESSION_ID_BYTES] = {0x10, 0x20, 0x30, 0x40};

static ag_pid_mask_t demo_supported(void) {
    ag_pid_mask_t mask;
    ag_pid_mask_clear(&mask);
    ag_pid_mask_set(&mask, 5);
    ag_pid_mask_set(&mask, 12);
    return mask;
}
static ag_can_policy_t active_policy(void) {
    ag_can_policy_t policy;
    ag_can_policy_init(&policy);
    policy.power = AG_POWER_OBD;
    policy.ignition = AG_IGN_QUALIFIED_ON;
    policy.hardware_validated = true;
    policy.rails_valid = true;
    policy.diagnostic_session_active = true;
    return policy;
}

static void test_shared_config_cases(void) {
    ag_pid_mask_t mask;
    for (size_t i = 0; i < AG_PID_MASK_WORDS; ++i) mask.words[i] = UINT32_MAX;
    for (size_t i = 0; i < sizeof(kSharedCases) / sizeof(kSharedCases[0]); ++i) {
        const bool valid = ag_config_validate(&kSharedCases[i].configuration, &mask) == AG_CONFIG_OK;
        if (valid != kSharedCases[i].valid) fprintf(stderr, "Contract mismatch: %s\n", kSharedCases[i].name);
        assert(valid == kSharedCases[i].valid);
    }
    printf("PASS: %zu shared C-representable contract cases\n", sizeof(kSharedCases) / sizeof(kSharedCases[0]));
}

static void test_config_atomicity_and_boundaries(void) {
    ag_pid_mask_t supported = demo_supported();
    ag_config_t active = kSharedFixture;
    ag_config_t candidate = active;
    assert(ag_config_validate(&active, &supported) == AG_CONFIG_OK);
    candidate.revision = 2;
    candidate.dashboards[0].widgets[0].rect.x = INT32_MAX;
    const ag_config_t before = active;
    assert(ag_config_apply(&active, &candidate, &supported) == AG_CONFIG_RECTANGLE);
    assert(memcmp(&active, &before, sizeof(active)) == 0);
    candidate = before;
    candidate.revision = 2;
    candidate.manual_percent = 100;
    assert(ag_config_apply(&active, &candidate, &supported) == AG_CONFIG_OK);
    assert(active.revision == 2 && active.manual_percent == 100);
    ag_pid_mask_clear(&supported);
    assert(ag_config_validate(&active, &supported) == AG_CONFIG_PID_UNSUPPORTED);
    assert(ag_config_validate(NULL, &supported) == AG_CONFIG_NULL);
    candidate = kSharedFixture;
    candidate.dashboard_count = 3;
    for (size_t d = 0; d < 3; ++d) {
        candidate.dashboards[d].id = (int32_t)d + 1;
        candidate.dashboards[d].widget_count = 5;
        for (size_t w = 0; w < 5; ++w) {
            candidate.dashboards[d].widgets[w] = kSharedFixture.dashboards[0].widgets[0];
            candidate.dashboards[d].widgets[w].id = (int32_t)w + 1;
        }
    }
    supported = demo_supported();
    assert(ag_config_validate(&candidate, &supported) == AG_CONFIG_OK);
}

static void test_can_fail_closed(void) {
    ag_can_policy_t policy;
    ag_can_policy_init(&policy);
    assert(!ag_can_tx_allowed(&policy));
    assert(!ag_can_tx_allowed(NULL));
    policy = active_policy();
    assert(ag_can_tx_allowed(&policy));
    policy.power = AG_POWER_USB_ONLY;
    assert(!ag_can_tx_allowed(&policy));
    policy.power = AG_POWER_UNKNOWN;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.ignition = AG_IGN_UNKNOWN;
    assert(!ag_can_tx_allowed(&policy));
    policy.ignition = AG_IGN_OFF;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.standby_requested = true;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.bus_off = true;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.rails_valid = false;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.hardware_validated = false;
    assert(!ag_can_tx_allowed(&policy));
    policy = active_policy(); policy.diagnostic_session_active = false;
    assert(!ag_can_tx_allowed(&policy));
}

static void test_service01_only(void) {
    ag_can_policy_t policy = active_policy();
    ag_pid_mask_t supported = demo_supported();
    ag_obd_request_t request;
    for (unsigned pid = 0; pid <= 255; ++pid) {
        const bool accepted = ag_obd_make_read_request(&policy, &supported, AG_READ_MEASUREMENT, (uint8_t)pid, &request);
        assert(accepted == (pid == 5 || pid == 12));
        if (accepted) assert(request.length == 2 && request.bytes[0] == 1 && request.bytes[1] == pid);
        else assert(request.length == 0);
    }
    ag_pid_mask_set(&supported, 255);
    assert(!ag_obd_make_read_request(&policy, &supported, AG_READ_MEASUREMENT, 255, &request));
    const uint8_t implemented[] = {0x04, 0x05, 0x0c, 0x0d, 0x0f, 0x10, 0x11};
    for (size_t i = 0; i < sizeof(implemented); ++i) {
        ag_pid_mask_set(&supported, implemented[i]);
        assert(ag_obd_make_read_request(&policy, &supported, AG_READ_MEASUREMENT, implemented[i], &request));
        assert(request.bytes[0] == 1 && request.length == 2);
    }
    assert(ag_obd_make_read_request(&policy, &supported, AG_READ_DISCOVERY, 0, &request));
    assert(request.bytes[0] == 1 && request.bytes[1] == 0);
    assert(!ag_obd_make_read_request(&policy, &supported, (ag_read_kind_t)99, 4, &request));
    policy.ignition = AG_IGN_UNKNOWN;
    assert(!ag_obd_make_read_request(&policy, &supported, AG_READ_DISCOVERY, 0, &request));
    assert(request.length == 0);
}

static void test_logging_rate_and_unavailable(void) {
    ag_log_queue_t queue;
    ag_pid_mask_t mask = demo_supported();
    const uint8_t raw[] = {0x20, 0x40};
    assert(ag_log_queue_init(&queue, session_id, &mask));
    assert(ag_log_queue_append(&queue, 12, 0x7e8, 0, AG_SAMPLE_VALID, raw, 2) == AG_LOG_ACCEPTED);
    assert(ag_log_queue_append(&queue, 12, 0x7e8, 999, AG_SAMPLE_VALID, raw, 2) == AG_LOG_RATE_LIMITED);
    /* Same PID, distinct ECU is independently decimated. */
    assert(ag_log_queue_append(&queue, 12, 0x7e9, 0, AG_SAMPLE_VALID, raw, 2) == AG_LOG_ACCEPTED);
    assert(ag_log_queue_append(&queue, 12, 0x7e8, 1000, AG_SAMPLE_TIMEOUT, NULL, 0) == AG_LOG_ACCEPTED);
    assert(ag_log_queue_append(&queue, 12, 0x7e8, 999, AG_SAMPLE_VALID, raw, 2) == AG_LOG_CLOCK_REGRESSION);
    assert(ag_log_queue_append(&queue, 5, 0x7e8, 0, AG_SAMPLE_TIMEOUT, raw, 2) == AG_LOG_INVALID);
    assert(ag_log_queue_append(&queue, 5, 0x7e8, 0, AG_SAMPLE_VALID, raw, 9) == AG_LOG_INVALID);
    assert(ag_log_queue_append(&queue, 99, 0x7e8, 0, AG_SAMPLE_VALID, raw, 2) == AG_LOG_NOT_CONFIGURED);
    ag_log_record_t record;
    assert(ag_log_queue_pop(&queue, &record));
    assert(record.sequence == 1 && record.raw_length == 2);
    assert(ag_log_queue_pop(&queue, &record));
    assert(record.ecu_source == 0x7e9);
    assert(ag_log_queue_pop(&queue, &record));
    assert(record.status == AG_SAMPLE_TIMEOUT && record.raw_length == 0);
    for (size_t i = 0; i < AG_RAW_CAPACITY; ++i) assert(record.raw[i] == 0);
    assert(!ag_log_queue_pop(&queue, &record));
    ag_log_queue_stop(&queue);
    assert(ag_log_queue_append(&queue, 5, 0x7e8, 1000, AG_SAMPLE_VALID, raw, 2) == AG_LOG_DISABLED);
}

static void test_logging_bounds_and_overflow(void) {
    ag_log_queue_t queue;
    ag_pid_mask_t mask = demo_supported();
    const uint8_t raw[] = {0x40};
    assert(ag_log_queue_init(&queue, session_id, &mask));
    for (unsigned i = 0; i < AG_LOG_CAPACITY + 3; ++i) {
        const ag_log_result_t result = ag_log_queue_append(&queue, 12, 0x7e8, (uint64_t)i * 1000, AG_SAMPLE_VALID, raw, 1);
        assert(result == (i < AG_LOG_CAPACITY ? AG_LOG_ACCEPTED : AG_LOG_OVERWROTE_OLDEST));
    }
    assert(queue.count == AG_LOG_CAPACITY && queue.dropped_records == 3);
    assert(queue.first_dropped_sequence == 1 && queue.last_dropped_sequence == 3);
    ag_log_record_t record;
    for (unsigned seq = 4; seq <= AG_LOG_CAPACITY + 3; ++seq) {
        assert(ag_log_queue_pop(&queue, &record));
        assert(record.sequence == seq);
    }
    assert(!ag_log_queue_pop(&queue, &record));
    assert(ag_log_queue_init(&queue, session_id, &mask));
    for (unsigned ecu = 0; ecu < AG_LOG_MAX_SERIES; ++ecu)
        assert(ag_log_queue_append(&queue, 12, ecu, 0, AG_SAMPLE_VALID, raw, 1) == AG_LOG_ACCEPTED);
    assert(ag_log_queue_append(&queue, 12, 99, 0, AG_SAMPLE_VALID, raw, 1) == AG_LOG_SERIES_FULL);
    queue.next_sequence = UINT64_MAX;
    assert(ag_log_queue_append(&queue, 12, 0, 1000, AG_SAMPLE_VALID, raw, 1) == AG_LOG_SEQUENCE_EXHAUSTED);
}

static void test_clock_sessions_and_regression(void) {
    ag_session_clock_t clock;
    uint64_t ms = 999;
    const uint8_t empty[AG_SESSION_ID_BYTES] = {0};
    assert(!ag_session_clock_init(&clock, empty, 100));
    assert(!ag_session_clock_stamp(&clock, 100, &ms));
    assert(ag_session_clock_init(&clock, session_id, 500));
    assert(ag_session_clock_stamp(&clock, 500, &ms) && ms == 0);
    assert(ag_session_clock_stamp(&clock, 1000500, &ms) && ms == 1000);
    assert(!ag_session_clock_stamp(&clock, 100, &ms));
    assert(!ag_session_clock_stamp(&clock, 2000500, &ms));
    assert(ag_session_clock_init(&clock, session_id, UINT64_MAX - 1000));
    assert(ag_session_clock_stamp(&clock, UINT64_MAX, &ms) && ms == 1);
    assert(!ag_session_clock_stamp(&clock, 0, &ms));
}

static void test_unimplemented_hal_is_not_success(void) {
    ag_can_policy_t policy = active_policy();
    assert(ag_hal_read_power(&policy) == AG_HAL_NOT_IMPLEMENTED);
    assert(!ag_can_tx_allowed(&policy));
    assert(ag_hal_request_low_power() == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_display_apply(&kSharedFixture) == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_log_persist(NULL) == AG_HAL_NOT_IMPLEMENTED);
    assert(ag_hal_ble_start() == AG_HAL_BLOCKED);
    assert(ag_hal_ota_begin() == AG_HAL_BLOCKED);
}

int main(void) {
    test_shared_config_cases();
    test_config_atomicity_and_boundaries();
    test_can_fail_closed();
    test_service01_only();
    test_logging_rate_and_unavailable();
    test_logging_bounds_and_overflow();
    test_clock_sessions_and_regression();
    test_unimplemented_hal_is_not_success();
    printf("PASS: config, CAN guard, Service01, log rate/overflow, session clock; queue=%zu bytes\n", sizeof(ag_log_queue_t));
    return 0;
}
