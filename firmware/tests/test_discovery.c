#include "ag_discovery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ag_can_policy_t enabled(void){ag_can_policy_t p={0};p.power=AG_POWER_OBD;p.ignition=AG_IGN_QUALIFIED_ON;p.hardware_validated=p.rails_valid=p.diagnostic_session_active=true;return p;}
static ag_can_profile_t profile(bool extended,uint32_t bitrate,unsigned ecu){return (ag_can_profile_t){extended?AG_CAN_ID_29:AG_CAN_ID_11,bitrate,extended?0x18da10f1u+ecu*0x100u:0x7e0u+ecu,extended?0x18daf110u+ecu:0x7e8u+ecu};}
static ag_can_frame_t bitmap(const ag_can_profile_t *p,uint8_t page,uint32_t bits){
    ag_can_frame_t frame={.id=p->response_id,.extended=p->id_format==AG_CAN_ID_29,.length=8,
        .data={6,0x41,page,(uint8_t)(bits>>24),(uint8_t)(bits>>16),(uint8_t)(bits>>8),(uint8_t)bits,0}};return frame;
}
static void handoff(ag_discovery_t *s,ag_can_policy_t *policy,uint64_t now,uint8_t page){
    assert(ag_discovery_prepare(s,policy,now));assert(!ag_discovery_prepare(s,policy,now));
    ag_simulated_dispatch_t out;assert(ag_discovery_dispatch(s,policy,now,&out));
    assert(out.request.length==2&&out.request.bytes[0]==1&&out.request.bytes[1]==page);
    assert(out.profile.request_id==s->ecus[s->current].profile.request_id&&out.profile.response_id==s->ecus[s->current].profile.response_id);
    assert(!ag_discovery_dispatch(s,policy,now,&out)&&out.request.length==0);
}
static void full_discovery(ag_discovery_t *s,bool extended,uint32_t bitrate){
    ag_can_policy_t policy=enabled();ag_can_profile_t profiles[2]={profile(extended,bitrate,0),profile(extended,bitrate,1)};
    assert(ag_discovery_init(s,profiles,2,100));uint64_t now=0;
    for(unsigned ecu=0;ecu<2;++ecu)for(unsigned page=0;page<=0xe0;page+=32){
        handoff(s,&policy,now,(uint8_t)page);ag_can_frame_t frame=bitmap(&profiles[ecu],(uint8_t)page,UINT32_MAX);
        assert(ag_discovery_receive(s,&policy,&frame,++now)==AG_DISCOVERY_RX_ACCEPTED);
    }
    assert(s->phase==AG_DISCOVERY_COMPLETE&&s->current==2);
}
static void test_init_and_four_variants(void){
    ag_discovery_t s={0};ag_can_profile_t profiles[2]={profile(false,500000,0),profile(false,500000,1)};
    assert(!ag_discovery_init(NULL,profiles,1,10));assert(!ag_discovery_init(&s,NULL,1,10));assert(!ag_discovery_init(&s,profiles,0,10));
    assert(!ag_discovery_init(&s,profiles,9,10));assert(!ag_discovery_init(&s,profiles,1,0));
    profiles[1]=profiles[0];assert(!ag_discovery_init(&s,profiles,2,10));profiles[1]=profile(true,500000,1);assert(!ag_discovery_init(&s,profiles,2,10));
    profiles[1]=profile(false,250000,1);assert(!ag_discovery_init(&s,profiles,2,10));profiles[0].bitrate=125000;assert(!ag_discovery_init(&s,profiles,1,10));
    profiles[0]=profile(false,500000,0);profiles[0].request_id=0x7df;assert(!ag_discovery_init(&s,profiles,1,10));
    profiles[0]=profile(true,500000,0);profiles[0].request_id=0x18db33f1;assert(!ag_discovery_init(&s,profiles,1,10));
    for(unsigned extended=0;extended<2;++extended)for(unsigned rate=0;rate<2;++rate){
        full_discovery(&s,extended!=0,rate?500000:250000);ag_pid_mask_t usable;assert(ag_discovery_usable(&s,&usable));size_t count=0;
        for(unsigned pid=0;pid<=255;++pid){assert(ag_pid_mask_has(&usable,(uint8_t)pid)==ag_obd_pid_implemented((uint8_t)pid));if(ag_pid_mask_has(&usable,(uint8_t)pid))++count;}
        assert(count==53);for(size_t ecu=0;ecu<2;++ecu){ag_obd_capabilities_t caps;assert(ag_discovery_capabilities(&s,ecu,&caps));assert(caps.pages_seen==255&&!ag_pid_mask_has(&caps.advertised,0));}
        ag_can_policy_t policy=enabled();ag_simulated_dispatch_t out;assert(!ag_discovery_prepare(&s,&policy,100)&&!ag_discovery_dispatch(&s,&policy,100,&out));
    }
}
static void test_continuation_and_isolation(void){
    ag_discovery_t s;ag_can_policy_t policy=enabled();ag_can_profile_t profiles[2]={profile(false,500000,0),profile(false,500000,1)};
    assert(ag_discovery_init(&s,profiles,2,100));handoff(&s,&policy,0,0);
    ag_can_frame_t other=bitmap(&profiles[1],0,UINT32_MAX);assert(ag_discovery_receive(&s,&policy,&other,50)==AG_DISCOVERY_RX_IGNORED&&s.started_ms==0);
    ag_can_frame_t first=bitmap(&profiles[0],0,(UINT32_C(1)<<20)|1u);assert(ag_discovery_receive(&s,&policy,&first,51)==AG_DISCOVERY_RX_ACCEPTED&&s.page==0x20);
    handoff(&s,&policy,52,0x20);assert(ag_discovery_receive(&s,&policy,&first,53)==AG_DISCOVERY_RX_IGNORED&&s.page==0x20);
    ag_can_frame_t last=bitmap(&profiles[0],0x20,UINT32_C(1)<<13);assert(ag_discovery_receive(&s,&policy,&last,54)==AG_DISCOVERY_RX_ACCEPTED&&s.current==1&&s.page==0);
    handoff(&s,&policy,55,0);ag_can_frame_t second=bitmap(&profiles[1],0,UINT32_C(1)<<27);assert(ag_discovery_receive(&s,&policy,&second,56)==AG_DISCOVERY_RX_ACCEPTED);
    ag_obd_capabilities_t a,b;assert(ag_discovery_capabilities(&s,0,&a)&&ag_discovery_capabilities(&s,1,&b));
    assert(ag_pid_mask_has(&a.advertised,0x0c)&&!ag_pid_mask_has(&b.advertised,0x0c));assert(ag_pid_mask_has(&b.advertised,5)&&!ag_pid_mask_has(&a.advertised,5));
    assert(a.pages_seen==3&&b.pages_seen==1);
}
static void test_timeout_negative_and_tx_failure(void){
    ag_discovery_t s;ag_can_policy_t policy=enabled();ag_can_profile_t profiles[3]={profile(false,500000,0),profile(false,500000,1),profile(false,500000,2)};
    assert(ag_discovery_init(&s,profiles,3,10));handoff(&s,&policy,0,0);assert(ag_discovery_tick(&s,&policy,9)&&s.phase==AG_DISCOVERY_WAITING);
    assert(ag_discovery_tick(&s,&policy,10)&&s.current==1&&s.ecus[0].result==AG_DISCOVERY_ECU_TIMEOUT);
    handoff(&s,&policy,10,0);ag_can_frame_t negative={.id=profiles[1].response_id,.length=8,.data={3,0x7f,1,0x78}};
    assert(ag_discovery_receive(&s,&policy,&negative,11)==AG_DISCOVERY_RX_ERROR&&s.ecus[1].result==AG_DISCOVERY_ECU_NEGATIVE&&s.ecus[1].negative_response_code==0x78);
    handoff(&s,&policy,12,0);assert(ag_discovery_tx_failed(&s,&policy,13)&&s.phase==AG_DISCOVERY_COMPLETE&&s.ecus[2].result==AG_DISCOVERY_ECU_TX_FAILED);
    assert(!ag_discovery_tx_failed(&s,&policy,14));ag_pid_mask_t usable;assert(ag_discovery_usable(&s,&usable));for(unsigned pid=0;pid<256;++pid)assert(!ag_pid_mask_has(&usable,(uint8_t)pid));
    assert(ag_discovery_init(&s,profiles,1,10));handoff(&s,&policy,UINT64_MAX-10,0);assert(ag_discovery_tick(&s,&policy,UINT64_MAX)&&s.phase==AG_DISCOVERY_COMPLETE);
}
static void test_malformed_and_partial_pages(void){
    ag_can_policy_t policy=enabled();ag_can_profile_t p=profile(false,500000,0);ag_discovery_t s;
    for(unsigned fault=0;fault<8;++fault){
        assert(ag_discovery_init(&s,&p,1,100));handoff(&s,&policy,0,0);ag_can_frame_t f=bitmap(&p,0,0);
        switch(fault){case 0:f.length=0;break;case 1:f.length=9;break;case 2:f.rtr=true;break;case 3:f.fd=true;break;
            case 4:f.data[0]=0x10;break;case 5:f.data[0]=7;break;case 6:f.data[1]=0x42;break;case 7:f.data[0]=2;break;}
        assert(ag_discovery_receive(&s,&policy,&f,1)==AG_DISCOVERY_RX_ERROR&&s.phase==AG_DISCOVERY_COMPLETE&&s.ecus[0].result==AG_DISCOVERY_ECU_MALFORMED);
    }
    assert(ag_discovery_init(&s,&p,1,10));handoff(&s,&policy,0,0);ag_can_frame_t f=bitmap(&p,0,(UINT32_C(1)<<20)|1u);
    assert(ag_discovery_receive(&s,&policy,&f,1)==AG_DISCOVERY_RX_ACCEPTED);handoff(&s,&policy,2,0x20);
    assert(ag_discovery_tick(&s,&policy,12)&&s.phase==AG_DISCOVERY_COMPLETE&&s.ecus[0].result==AG_DISCOVERY_ECU_TIMEOUT);
    ag_pid_mask_t usable;assert(ag_discovery_usable(&s,&usable)&&ag_pid_mask_has(&usable,0x0c)&&!ag_pid_mask_has(&usable,0x42));
}
static void revoke(ag_can_policy_t *p,unsigned which){switch(which){case 0:p->power=AG_POWER_USB_ONLY;break;case 1:p->power=AG_POWER_UNKNOWN;break;case 2:p->ignition=AG_IGN_OFF;break;case 3:p->ignition=AG_IGN_UNKNOWN;break;case 4:p->hardware_validated=false;break;case 5:p->rails_valid=false;break;case 6:p->standby_requested=true;break;case 7:p->bus_off=true;break;case 8:p->diagnostic_session_active=false;break;}}
static void test_revocation_and_clock(void){
    ag_can_profile_t p=profile(false,500000,0);ag_discovery_t s;
    for(unsigned guard=0;guard<9;++guard)for(unsigned when=0;when<2;++when){
        ag_can_policy_t policy=enabled();assert(ag_discovery_init(&s,&p,1,10));assert(ag_discovery_prepare(&s,&policy,0));
        ag_simulated_dispatch_t out;if(when)assert(ag_discovery_dispatch(&s,&policy,0,&out));revoke(&policy,guard);
        assert(!ag_discovery_dispatch(&s,&policy,1,&out)&&out.request.length==0&&s.phase==AG_DISCOVERY_ABORTED);
        policy=enabled();assert(!ag_discovery_prepare(&s,&policy,2)&&!ag_discovery_tick(&s,&policy,2));
        ag_can_frame_t f=bitmap(&p,0,UINT32_MAX);assert(ag_discovery_receive(&s,&policy,&f,2)==AG_DISCOVERY_RX_BLOCKED);
    }
    ag_can_policy_t policy=enabled();assert(ag_discovery_init(&s,&p,1,10));handoff(&s,&policy,8,0);
    assert(!ag_discovery_tick(&s,&policy,7)&&s.phase==AG_DISCOVERY_CLOCK_FAULT);assert(!ag_discovery_prepare(&s,&policy,9));
}
static void test_defaults_are_supported_and_transactional(void){
    ag_discovery_t s;full_discovery(&s,false,500000);ag_default_config_options_t options={3,5,60};ag_config_t config;
    assert(ag_discovery_build_defaults(&s,&options,&config)==AG_DEFAULT_CONFIG_OK&&config.dashboard_count==3&&config.revision==1);
    ag_pid_mask_t usable;assert(ag_discovery_usable(&s,&usable)&&ag_config_validate(&config,&usable)==AG_CONFIG_OK);
    const unsigned wanted[]={0x0c,0x0d,0x05,0x42,0x0f,0x10,0x11,0x04,0x0b,0x23,0x5c,0x46,0x2f,0x5e,0xa6};
    size_t index=0;for(size_t d=0;d<3;++d){assert(config.dashboards[d].widget_count==5);for(size_t w=0;w<5;++w){
        const ag_widget_t *widget=&config.dashboards[d].widgets[w];assert(widget->pid==(int32_t)wanted[index++]);
        assert(widget->rect.x>=2000&&widget->rect.x+widget->rect.width<=8000&&widget->rect.y>=2000&&widget->rect.y+widget->rect.height<=8000);
    }}
    ag_config_t before=config;options.maximum_dashboards=0;assert(ag_discovery_build_defaults(&s,&options,&config)==AG_DEFAULT_CONFIG_INVALID&&!memcmp(&before,&config,sizeof(config)));
    options=(ag_default_config_options_t){3,5,0};ag_can_profile_t p=profile(false,500000,0);ag_can_policy_t policy=enabled();
    assert(ag_discovery_init(&s,&p,1,10));assert(ag_discovery_build_defaults(&s,&options,&config)==AG_DEFAULT_CONFIG_NOT_READY&&!memcmp(&before,&config,sizeof(config)));
    handoff(&s,&policy,0,0);ag_can_frame_t f=bitmap(&p,0,0);assert(ag_discovery_receive(&s,&policy,&f,1)==AG_DISCOVERY_RX_ACCEPTED);
    assert(ag_discovery_build_defaults(&s,&options,&config)==AG_DEFAULT_CONFIG_EMPTY&&!memcmp(&before,&config,sizeof(config)));
    assert(ag_discovery_init(&s,&p,1,10));handoff(&s,&policy,0,0);f=bitmap(&p,0,(UINT32_C(1)<<27));assert(ag_discovery_receive(&s,&policy,&f,1)==AG_DISCOVERY_RX_ACCEPTED);
    assert(ag_discovery_build_defaults(&s,&options,&config)==AG_DEFAULT_CONFIG_OK&&config.dashboard_count==1&&config.dashboards[0].widget_count==1&&config.dashboards[0].widgets[0].pid==5);
}
int main(void){test_init_and_four_variants();test_continuation_and_isolation();test_timeout_negative_and_tx_failure();test_malformed_and_partial_pages();test_revocation_and_clock();test_defaults_are_supported_and_transactional();puts("PASS: 6 discovery/default groups; host handoff only");return 0;}
