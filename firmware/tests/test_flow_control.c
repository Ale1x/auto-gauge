#include "ag_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ag_can_profile_t profile(bool extended) {
    return (ag_can_profile_t){extended ? AG_CAN_ID_29 : AG_CAN_ID_11,
        extended ? 250000 : 500000, extended ? 0x18da10f1 : 0x7e0,
        extended ? 0x18daf110 : 0x7e8};
}
static ag_can_policy_t policy(void) {
    ag_can_policy_t p; ag_can_policy_init(&p);
    p.power = AG_POWER_OBD; p.ignition = AG_IGN_QUALIFIED_ON;
    p.hardware_validated = p.rails_valid = p.diagnostic_session_active = true;
    return p;
}
static ag_can_frame_t ff(const ag_can_profile_t *p, unsigned length) {
    return (ag_can_frame_t){.id=p->response_id,.extended=p->id_format==AG_CAN_ID_29,
        .length=8,.data={(uint8_t)(0x10 | (length >> 8)),(uint8_t)length,0x41,0x0c,2,3,4,5}};
}
static void cts(ag_isotp_rx_t *rx, ag_can_policy_t *p, uint64_t now, uint8_t bs, uint8_t st) {
    ag_can_frame_t output;
    assert(!ag_isotp_fc_dispatch(rx,p,now,&output));
    assert(ag_isotp_fc_prepare(rx,p,now));
    assert(ag_isotp_fc_dispatch(rx,p,now,&output));
    assert(output.id==rx->profile.request_id && output.extended==(rx->profile.id_format==AG_CAN_ID_29));
    assert(output.length==8 && output.data[0]==0x30 && output.data[1]==bs && output.data[2]==st);
    for (unsigned i=3;i<8;++i) assert(output.data[i]==0);
    assert(!ag_isotp_fc_dispatch(rx,p,now,&output) && output.length==0);
}
static void test_multiframe_blocks_wrap_and_addresses(void) {
    const unsigned sizes[]={0,1,2,3,255};
    for (unsigned ext=0;ext<2;++ext) for (size_t b=0;b<sizeof(sizes)/sizeof(sizes[0]);++b) {
        const unsigned bs=sizes[b];
        ag_can_profile_t pr=profile(ext!=0); ag_can_policy_t p=policy(); ag_isotp_rx_t rx;
        assert(ag_isotp_init(&rx,&pr,100)); assert(ag_isotp_enable_flow_control(&rx,(uint8_t)bs,2));
        ag_can_frame_t f=ff(&pr,256);
        assert(ag_isotp_feed(&rx,&f,0)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
        cts(&rx,&p,0,(uint8_t)bs,2);
        size_t received=6; unsigned number=1; uint64_t now=2;
        while (received<256) {
            f.data[0]=(uint8_t)(0x20 | (number & 15)); f.length=8;
            for (unsigned i=1;i<8;++i) f.data[i]=(uint8_t)(received+i-1);
            const size_t amount=256-received<7 ? 256-received : 7;
            const ag_isotp_result_t result=ag_isotp_feed(&rx,&f,now);
            received+=amount;
            if (received==256) assert(result==AG_ISOTP_COMPLETE);
            else if (bs!=0 && number%bs==0) {
                assert(result==AG_ISOTP_FLOW_CONTROL_REQUIRED); cts(&rx,&p,now,(uint8_t)bs,2);
            } else assert(result==AG_ISOTP_WAITING);
            ++number; now+=2;
        }
        uint8_t payload[256];size_t length;
        assert(ag_isotp_take(&rx,payload,sizeof(payload),&length) && length==256);
        assert(payload[0]==0x41 && payload[1]==12);
        for (unsigned i=6;i<256;++i) assert(payload[i]==(uint8_t)i);
        assert(!rx.fc_pending);
    }
}
static void test_overflow_timeout_and_invalid_inputs(void) {
    ag_can_profile_t pr=profile(false);ag_isotp_rx_t rx;ag_can_policy_t p=policy();ag_can_frame_t out;
    assert(ag_isotp_init(&rx,&pr,10));
    for(unsigned st=128;st<256;++st) assert(!ag_isotp_enable_flow_control(&rx,8,(uint8_t)st));
    assert(ag_isotp_enable_flow_control(&rx,0,127));
    ag_can_frame_t f=ff(&pr,257);
    assert(ag_isotp_feed(&rx,&f,0)==AG_ISOTP_OVERFLOW);
    assert(ag_isotp_fc_prepare(&rx,&p,0)); assert(ag_isotp_fc_dispatch(&rx,&p,0,&out));
    assert(out.data[0]==0x32 && out.data[1]==0 && out.data[2]==0 && !rx.active);
    f=ff(&pr,20);assert(ag_isotp_feed(&rx,&f,1)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    assert(ag_isotp_poll(&rx,11)==AG_ISOTP_TIMEOUT && !rx.fc_pending);
    assert(!ag_isotp_fc_prepare(&rx,&p,11));
    assert(ag_isotp_feed(&rx,&f,12)==AG_ISOTP_FLOW_CONTROL_REQUIRED);cts(&rx,&p,12,0,127);
    assert(ag_isotp_poll(&rx,22)==AG_ISOTP_TIMEOUT);
    f.data[2]=0x62; assert(ag_isotp_feed(&rx,&f,23)==AG_ISOTP_UNSUPPORTED && !rx.fc_pending);
    f=ff(&pr,7); assert(ag_isotp_feed(&rx,&f,24)==AG_ISOTP_MALFORMED);
    f=ff(&pr,20); f.length=7; assert(ag_isotp_feed(&rx,&f,25)==AG_ISOTP_MALFORMED);
    f=ff(&pr,20); assert(ag_isotp_feed(&rx,&f,26)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    f.data[0]=0x21; assert(ag_isotp_feed(&rx,&f,27)==AG_ISOTP_MALFORMED && !rx.active);
    f=ff(&pr,20); assert(ag_isotp_feed(&rx,&f,28)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    cts(&rx,&p,28,0,127); f.data[0]=0x22;
    assert(ag_isotp_feed(&rx,&f,29)==AG_ISOTP_SEQUENCE_ERROR);
    f=ff(&pr,20);assert(ag_isotp_feed(&rx,&f,30)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    assert(ag_isotp_fc_prepare(&rx,&p,30));ag_isotp_reset(&rx);
    assert(ag_isotp_feed(&rx,&f,31)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    assert(!ag_isotp_fc_dispatch(&rx,&p,31,&out)); /* Old preparation was invalidated. */
    cts(&rx,&p,31,0,127);f.data[0]=0x21;f.length=2;
    assert(ag_isotp_feed(&rx,&f,32)==AG_ISOTP_MALFORMED);
    f=ff(&pr,20);f.data[2]=0x7f;f.data[3]=0x2e;
    assert(ag_isotp_feed(&rx,&f,33)==AG_ISOTP_UNSUPPORTED && !rx.fc_pending);
    f=ff(&pr,20);assert(ag_isotp_feed(&rx,&f,34)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    f.data[0]=0x30;assert(ag_isotp_feed(&rx,&f,35)==AG_ISOTP_MALFORMED);
    pr.request_id=0x7df; assert(ag_isotp_init(&rx,&pr,10)); assert(!ag_isotp_enable_flow_control(&rx,0,0));
    pr=profile(true);pr.request_id=0x18db33f1;
    assert(ag_isotp_init(&rx,&pr,10));assert(!ag_isotp_enable_flow_control(&rx,0,0));
}
static void test_separation_and_time_limits(void) {
    ag_can_profile_t pr=profile(false);ag_isotp_rx_t rx;ag_can_policy_t p=policy();ag_can_frame_t f;
    assert(ag_isotp_init(&rx,&pr,100));assert(ag_isotp_enable_flow_control(&rx,1,10));
    f=ff(&pr,20);assert(ag_isotp_feed(&rx,&f,0)==AG_ISOTP_FLOW_CONTROL_REQUIRED);cts(&rx,&p,0,1,10);
    f.data[0]=0x21;assert(ag_isotp_feed(&rx,&f,1)==AG_ISOTP_FLOW_CONTROL_REQUIRED);cts(&rx,&p,1,1,10);
    f.data[0]=0x22;assert(ag_isotp_feed(&rx,&f,10)==AG_ISOTP_SEPARATION_ERROR);
    f=ff(&pr,8);assert(ag_isotp_feed(&rx,&f,20)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    assert(ag_isotp_fc_prepare(&rx,&p,20));ag_can_frame_t out;
    assert(!ag_isotp_fc_dispatch(&rx,&p,19,&out) && !rx.active && out.length==0);
    ag_isotp_reset(&rx);f=ff(&pr,8);
    assert(ag_isotp_feed(&rx,&f,UINT64_MAX-1)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    cts(&rx,&p,UINT64_MAX-1,1,10);f.data[0]=0x21;
    assert(ag_isotp_feed(&rx,&f,UINT64_MAX)==AG_ISOTP_COMPLETE);
    ag_isotp_reset(&rx);f=ff(&pr,8);
    assert(ag_isotp_feed(&rx,&f,UINT64_MAX)==AG_ISOTP_FLOW_CONTROL_REQUIRED);
    assert(!ag_isotp_fc_prepare(&rx,&p,0) && !rx.active);
}
static void block_policy(ag_can_policy_t *p,unsigned which) {
    switch(which){
    case 0:p->power=AG_POWER_USB_ONLY;break;case 1:p->ignition=AG_IGN_UNKNOWN;break;
    case 2:p->ignition=AG_IGN_OFF;break;case 3:p->standby_requested=true;break;
    case 4:p->hardware_validated=false;break;case 5:p->rails_valid=false;break;
    case 6:p->diagnostic_session_active=false;break;case 7:p->bus_off=true;break;
    default:p->power=AG_POWER_UNKNOWN;break;
    }
}
static void test_policy_change_and_other_ecu(void) {
    for(unsigned mode=0;mode<9;++mode) for(unsigned overflow=0;overflow<2;++overflow) {
        ag_can_profile_t pr=profile(false);ag_isotp_rx_t rx;ag_can_policy_t p=policy();ag_can_frame_t out;
        assert(ag_isotp_init(&rx,&pr,100));assert(ag_isotp_enable_flow_control(&rx,8,0));
        ag_can_frame_t f=ff(&pr,overflow?257:20);
        assert(ag_isotp_feed(&rx,&f,0)==(overflow?AG_ISOTP_OVERFLOW:AG_ISOTP_FLOW_CONTROL_REQUIRED));
        ++f.id;assert(ag_isotp_feed(&rx,&f,1)==AG_ISOTP_IGNORED && rx.fc_pending);
        assert(ag_isotp_fc_prepare(&rx,&p,1));block_policy(&p,mode);
        assert(!ag_isotp_fc_dispatch(&rx,&p,2,&out) && out.length==0 && !rx.fc_pending && !rx.active);
        p=policy();assert(!ag_isotp_fc_dispatch(&rx,&p,3,&out));
    }
}
static void test_scheduler_flow_guard_and_overflow(void) {
    for(unsigned blocked=0;blocked<2;++blocked) {
        ag_scheduler_t s;ag_scheduler_init(&s);ag_can_policy_t p=policy();
        ag_poll_job_config_t job={.pid=12,.profile=profile(false),.interval_ms=200,.response_timeout_ms=100,.stale_after_ms=50};
        ag_obd_capabilities_t caps;ag_obd_capabilities_init(&caps,job.profile.response_id);ag_pid_mask_set(&caps.advertised,12);
        assert(ag_scheduler_add(&s,&job,&caps));assert(ag_scheduler_prepare(&s,&p,0));
        ag_simulated_dispatch_t request;assert(ag_scheduler_dispatch(&s,&p,0,&request));
        ag_can_frame_t f=ff(&job.profile,257),out;
        assert(ag_scheduler_receive(&s,&p,&f,1)==AG_SCHED_RX_WAITING);
        assert(ag_scheduler_fc_prepare(&s,&p,1));if(blocked)p.ignition=AG_IGN_OFF;
        assert(ag_scheduler_fc_dispatch(&s,&p,2,&out)==!blocked);
        if(blocked) assert(out.length==0 && s.jobs[0].sample.status==AG_SAMPLE_UNAVAILABLE);
        else assert(out.data[0]==0x32 && s.jobs[0].sample.status==AG_SAMPLE_ERROR);
        assert(s.active_index==SIZE_MAX);
    }
    /* A transport-valid long response is not promoted to a decoded PID value. */
    ag_scheduler_t s;ag_scheduler_init(&s);ag_can_policy_t p=policy();
    ag_poll_job_config_t job={.pid=12,.profile=profile(false),.interval_ms=200,.response_timeout_ms=100,.stale_after_ms=50};
    ag_obd_capabilities_t caps;ag_obd_capabilities_init(&caps,job.profile.response_id);ag_pid_mask_set(&caps.advertised,12);
    assert(ag_scheduler_add(&s,&job,&caps));assert(ag_scheduler_prepare(&s,&p,0));
    ag_simulated_dispatch_t request;assert(ag_scheduler_dispatch(&s,&p,0,&request));
    ag_can_frame_t f=ff(&job.profile,8),out;
    assert(ag_scheduler_receive(&s,&p,&f,1)==AG_SCHED_RX_WAITING);
    assert(ag_scheduler_fc_prepare(&s,&p,1) && ag_scheduler_fc_dispatch(&s,&p,1,&out));
    assert(out.data[0]==0x30 && out.data[1]==8);
    f.data[0]=0x21;
    assert(ag_scheduler_receive(&s,&p,&f,2)==AG_SCHED_RX_ERROR);
    assert(!s.jobs[0].sample.has_value && s.active_index==SIZE_MAX);
}
int main(void) {
    test_multiframe_blocks_wrap_and_addresses();test_overflow_timeout_and_invalid_inputs();
    test_separation_and_time_limits();test_policy_change_and_other_ecu();test_scheduler_flow_guard_and_overflow();
    puts("PASS: 5 FlowControl groups, CTS/overflow, 11/29-bit, BS0..3/255, STmin, timeout/wrap, live TX guards; no CAN driver");
    return 0;
}
