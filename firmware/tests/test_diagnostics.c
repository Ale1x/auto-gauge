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
/* Independent numeric endpoints, not computed from the decoder coefficients. */
typedef struct {uint8_t pid, bytes;ag_unit_t unit;double minimum,maximum;} endpoint_case_t;
static const endpoint_case_t scalar_endpoints[] = {
    {0x04,1,AG_UNIT_PERCENT,0,100}, {0x05,1,AG_UNIT_CELSIUS,-40,215},
    {0x06,1,AG_UNIT_PERCENT,-100,99.21875}, {0x07,1,AG_UNIT_PERCENT,-100,99.21875},
    {0x08,1,AG_UNIT_PERCENT,-100,99.21875}, {0x09,1,AG_UNIT_PERCENT,-100,99.21875},
    {0x0a,1,AG_UNIT_KILOPASCAL,0,765}, {0x0b,1,AG_UNIT_KILOPASCAL,0,255},
    {0x0c,2,AG_UNIT_RPM,0,16383.75}, {0x0d,1,AG_UNIT_KM_PER_HOUR,0,255},
    {0x0e,1,AG_UNIT_DEGREE,-64,63.5}, {0x0f,1,AG_UNIT_CELSIUS,-40,215},
    {0x10,2,AG_UNIT_GRAMS_PER_SECOND,0,655.35}, {0x11,1,AG_UNIT_PERCENT,0,100},
    {0x1f,2,AG_UNIT_SECOND,0,65535}, {0x21,2,AG_UNIT_KILOMETER,0,65535},
    {0x22,2,AG_UNIT_KILOPASCAL,0,5177.265}, {0x23,2,AG_UNIT_KILOPASCAL,0,655350},
    {0x2c,1,AG_UNIT_PERCENT,0,100}, {0x2d,1,AG_UNIT_PERCENT,-100,99.21875},
    {0x2e,1,AG_UNIT_PERCENT,0,100}, {0x2f,1,AG_UNIT_PERCENT,0,100},
    {0x30,1,AG_UNIT_COUNT,0,255}, {0x31,2,AG_UNIT_KILOMETER,0,65535},
    {0x33,1,AG_UNIT_KILOPASCAL,0,255},
    {0x3c,2,AG_UNIT_CELSIUS,-40,6513.5}, {0x3d,2,AG_UNIT_CELSIUS,-40,6513.5},
    {0x3e,2,AG_UNIT_CELSIUS,-40,6513.5}, {0x3f,2,AG_UNIT_CELSIUS,-40,6513.5},
    {0x42,2,AG_UNIT_VOLT,0,65.535}, {0x43,2,AG_UNIT_PERCENT,0,25700},
    {0x44,2,AG_UNIT_RATIO,0,1.999969482421875}, {0x45,1,AG_UNIT_PERCENT,0,100},
    {0x46,1,AG_UNIT_CELSIUS,-40,215}, {0x47,1,AG_UNIT_PERCENT,0,100},
    {0x48,1,AG_UNIT_PERCENT,0,100}, {0x49,1,AG_UNIT_PERCENT,0,100},
    {0x4a,1,AG_UNIT_PERCENT,0,100}, {0x4b,1,AG_UNIT_PERCENT,0,100},
    {0x4c,1,AG_UNIT_PERCENT,0,100}, {0x4d,2,AG_UNIT_MINUTE,0,65535},
    {0x4e,2,AG_UNIT_MINUTE,0,65535}, {0x52,1,AG_UNIT_PERCENT,0,100},
    {0x59,2,AG_UNIT_KILOPASCAL,0,655350}, {0x5a,1,AG_UNIT_PERCENT,0,100},
    {0x5b,1,AG_UNIT_PERCENT,0,100}, {0x5c,1,AG_UNIT_CELSIUS,-40,215},
    {0x5d,2,AG_UNIT_DEGREE,-210,301.9921875}, {0x5e,2,AG_UNIT_LITERS_PER_HOUR,0,3276.75},
    {0x61,1,AG_UNIT_PERCENT,-125,130}, {0x62,1,AG_UNIT_PERCENT,-125,130},
    {0x63,2,AG_UNIT_NEWTON_METER,0,65535}, {0xa6,4,AG_UNIT_KILOMETER,0,429496729.5}
};
_Static_assert(sizeof(scalar_endpoints)/sizeof(scalar_endpoints[0])==53,"Explicit53PID coverage");
_Static_assert(AG_UNIT_UNKNOWN==0&&AG_UNIT_PERCENT==1&&AG_UNIT_CELSIUS==2&&AG_UNIT_RPM==3
    &&AG_UNIT_KM_PER_HOUR==4&&AG_UNIT_GRAMS_PER_SECOND==5,"Preserve existing unit enum values");
static void assert_numeric(const ag_measurement_t *sample,double expected,ag_unit_t unit) {
    assert(sample->has_value&&sample->status==AG_SAMPLE_VALID&&sample->unit==unit);
    assert(isfinite(sample->value)&&fabs(sample->value-expected)<=1e-9+fabs(expected)*1e-14);
    assert(sample->negative_response_code==0);
}
static void assert_empty(const ag_measurement_t *sample,ag_sample_status_t status,uint8_t pid,uint64_t time) {
    assert(sample->status==status&&!sample->has_value&&isnan(sample->value));
    assert(sample->pid==pid&&sample->ecu_source==0x7e8&&sample->updated_ms==time);
    assert(sample->unit==AG_UNIT_UNKNOWN&&sample->raw_length==0);
    for(size_t i=0;i<AG_RAW_CAPACITY;++i)assert(sample->raw[i]==0);
}
static void test_expanded_scalar_endpoints_lengths_and_metadata(void) {
    for(size_t i=0;i<sizeof(scalar_endpoints)/sizeof(scalar_endpoints[0]);++i) {
        const endpoint_case_t *c=&scalar_endpoints[i];
        uint8_t payload[AG_RAW_CAPACITY+3]={0x41,c->pid};
        ag_measurement_t sample;
        for(unsigned maximum=0;maximum<2;++maximum) {
            memset(payload+2,maximum?0xff:0,sizeof(payload)-2);
            memset(&sample,0xa5,sizeof(sample));
            assert(ag_obd_parse_measurement(payload,c->bytes+2,c->pid,0x7e8,UINT64_MAX,&sample)==AG_OBD_OK);
            assert_numeric(&sample,maximum?c->maximum:c->minimum,c->unit);
            assert(sample.pid==c->pid&&sample.ecu_source==0x7e8&&sample.updated_ms==UINT64_MAX);
            assert(sample.raw_length==c->bytes&&memcmp(sample.raw,payload+2,c->bytes)==0);
            for(size_t tail=c->bytes;tail<AG_RAW_CAPACITY;++tail)assert(sample.raw[tail]==0);
        }
        for(size_t length=0;length<=sizeof(payload);++length) {
            if(length==c->bytes+2u)continue;
            assert(ag_obd_parse_measurement(payload,length,c->pid,0x7e8,77,&sample)==AG_OBD_MALFORMED);
            assert_empty(&sample,AG_SAMPLE_ERROR,c->pid,77);
        }
        assert(ag_obd_parse_measurement(payload,SIZE_MAX,c->pid,0x7e8,78,&sample)==AG_OBD_MALFORMED);
        assert_empty(&sample,AG_SAMPLE_ERROR,c->pid,78);
        payload[1]^=0x80;
        assert(ag_obd_parse_measurement(payload,c->bytes+2,c->pid,0x7e8,79,&sample)==AG_OBD_WRONG_PID);
        assert_empty(&sample,AG_SAMPLE_ERROR,c->pid,79);
        const uint8_t negative[]={0x7f,1,0x78};
        assert(ag_obd_parse_measurement(negative,3,c->pid,0x7e8,80,&sample)==AG_OBD_NEGATIVE_RESPONSE);
        assert_empty(&sample,AG_SAMPLE_ERROR,c->pid,80);assert(sample.negative_response_code==0x78);
    }
    puts("PASS: 53 scalar PID endpoint/strict-length/raw/metadata cases");
}
static void test_expanded_scalar_independent_midpoints(void) {
    const struct {uint8_t pid,bytes,data[4];double expected;ag_unit_t unit;} values[]={
        {0x06,1,{128},0,AG_UNIT_PERCENT}, {0x06,1,{129},0.78125,AG_UNIT_PERCENT},
        {0x0a,1,{100},300,AG_UNIT_KILOPASCAL}, {0x0b,1,{100},100,AG_UNIT_KILOPASCAL},
        {0x0e,1,{129},0.5,AG_UNIT_DEGREE}, {0x1f,2,{0x0e,0x10},3600,AG_UNIT_SECOND},
        {0x22,2,{0x03,0xe8},79,AG_UNIT_KILOPASCAL}, {0x23,2,{0x13,0x88},50000,AG_UNIT_KILOPASCAL},
        {0x2d,1,{0x80},0,AG_UNIT_PERCENT}, {0x30,1,{17},17,AG_UNIT_COUNT},
        {0x31,2,{0x04,0xd2},1234,AG_UNIT_KILOMETER},
        {0x3c,2,{0x03,0xe9},60.1,AG_UNIT_CELSIUS}, /* Fraction catches integer /10 truncation. */
        {0x42,2,{0x36,0xb0},14,AG_UNIT_VOLT},
        {0x43,2,{0x01,0xff},200.3921568627451,AG_UNIT_PERCENT}, /* Never clamp absolute load to100. */
        {0x44,2,{0x80,0x00},1,AG_UNIT_RATIO}, /* Exact2/65536, not rounded0.0000305. */
        {0x46,1,{65},25,AG_UNIT_CELSIUS}, {0x4d,2,{0x01,0x02},258,AG_UNIT_MINUTE},
        {0x59,2,{0x13,0x88},50000,AG_UNIT_KILOPASCAL},
        {0x5c,1,{130},90,AG_UNIT_CELSIUS},
        {0x5d,2,{0x69,0x01},0.0078125,AG_UNIT_DEGREE}, /* Fraction catches integer /128 truncation. */
        {0x5e,2,{0,33},1.65,AG_UNIT_LITERS_PER_HOUR},
        {0x61,1,{125},0,AG_UNIT_PERCENT}, {0x62,1,{100},-25,AG_UNIT_PERCENT},
        {0x63,2,{0x01,0xf4},500,AG_UNIT_NEWTON_METER},
        {0xa6,4,{0x00,0x12,0xd6,0x87},123456.7,AG_UNIT_KILOMETER},
        {0xa6,4,{0xff,0xff,0xff,0xfe},429496729.4,AG_UNIT_KILOMETER}
    };
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);++i) {
        uint8_t payload[6]={0x41,values[i].pid};memcpy(payload+2,values[i].data,values[i].bytes);
        ag_measurement_t sample;
        assert(ag_obd_parse_measurement(payload,values[i].bytes+2,values[i].pid,0x18daf110,321,&sample)==AG_OBD_OK);
        assert_numeric(&sample,values[i].expected,values[i].unit);
        assert(sample.ecu_source==0x18daf110&&sample.updated_ms==321);
    }
    printf("PASS: %zu independent expanded scalar midpoint vectors\n",sizeof(values)/sizeof(values[0]));
}
static void test_decoder_allowlist_capabilities_and_scope_stay_separate(void) {
    ag_obd_capabilities_t caps;ag_obd_capabilities_init(&caps,0x7e8);
    ag_pid_mask_t usable;ag_obd_usable_pids(&caps,&usable);
    ag_can_policy_t policy=active_policy();
    for(unsigned pid=0;pid<256;++pid) {
        const endpoint_case_t *entry=NULL;
        for(size_t i=0;i<sizeof(scalar_endpoints)/sizeof(scalar_endpoints[0]);++i)
            if(scalar_endpoints[i].pid==pid)entry=&scalar_endpoints[i];
        assert(ag_obd_pid_implemented((uint8_t)pid)==(entry!=NULL));
        uint8_t payload[6]={0x41,(uint8_t)pid,0,0,0,0};ag_measurement_t sample;
        const ag_obd_capabilities_t before=caps;
        assert(ag_obd_parse_measurement(payload,entry?entry->bytes+2u:6u,(uint8_t)pid,0x7e8,1,&sample)
            ==(entry?AG_OBD_OK:AG_OBD_UNSUPPORTED_PID));
        assert(memcmp(&before,&caps,sizeof(caps))==0);
        ag_obd_request_t request;
        assert(!ag_obd_make_read_request(&policy,&usable,AG_READ_MEASUREMENT,(uint8_t)pid,&request));
        assert(request.length==0); /* Decoder success alone never creates a vehicle capability. */
    }
    for(unsigned page=0;page<=0xe0;page+=0x20) {
        const uint8_t payload[]={0x41,(uint8_t)page,255,255,255,255};
        assert(ag_obd_capabilities_apply(&caps,0x7e8,payload,sizeof(payload)));
    }
    assert(caps.pages_seen==255);ag_obd_usable_pids(&caps,&usable);unsigned count=0;
    for(unsigned pid=0;pid<256;++pid) {
        assert(ag_pid_mask_has(&usable,(uint8_t)pid)==ag_obd_pid_implemented((uint8_t)pid));
        if(!ag_obd_pid_implemented((uint8_t)pid))continue;
        ++count;ag_obd_request_t request;
        assert(ag_obd_make_read_request(&policy,&usable,AG_READ_MEASUREMENT,(uint8_t)pid,&request));
        assert(request.length==2&&request.bytes[0]==1&&request.bytes[1]==pid);
        ag_can_policy_t blocked=policy;blocked.power=AG_POWER_USB_ONLY;
        assert(!ag_obd_make_read_request(&blocked,&usable,AG_READ_MEASUREMENT,(uint8_t)pid,&request));
        blocked=policy;blocked.ignition=AG_IGN_UNKNOWN;
        assert(!ag_obd_make_read_request(&blocked,&usable,AG_READ_MEASUREMENT,(uint8_t)pid,&request));
    }
    assert(count==53);
    const uint8_t not_service01[]={0x62,0x0b,0};ag_measurement_t sample;
    assert(ag_obd_parse_measurement(not_service01,3,0x0b,0x7e8,81,&sample)==AG_OBD_WRONG_SERVICE);
    assert_empty(&sample,AG_SAMPLE_ERROR,0x0b,81);
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
    test_expanded_scalar_endpoints_lengths_and_metadata();
    test_expanded_scalar_independent_midpoints();
    test_decoder_allowlist_capabilities_and_scope_stay_separate();
    test_decoder_malformed_and_unavailable();
    test_capability_bit_order_and_ecu_isolation();
    test_profiles_and_isotp_single_frame();
    test_isotp_multiframe_and_sequence_wrap();
    test_isotp_errors_timeout_and_wrong_ecu();
    test_isotp_bounded_random_inputs();
    test_scheduler_safety_recheck_and_timeout();
    test_scheduler_round_robin_clock_and_profile_bounds();
    test_every_dispatch_gate_can_revoke_prepared_work();
    printf("PASS: 13 diagnostics groups, 10000 malformed/random frame trials; RX=%zu bytes, scheduler=%zu bytes; no physical TX\n",
           sizeof(ag_isotp_rx_t), sizeof(ag_scheduler_t));
    return 0;
}
