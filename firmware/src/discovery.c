#include "ag_discovery.h"
#include <string.h>

static bool terminal(const ag_discovery_t *s){return s->phase==AG_DISCOVERY_ABORTED||s->phase==AG_DISCOVERY_CLOCK_FAULT;}
static void abort_discovery(ag_discovery_t *s,ag_discovery_phase_t phase){ag_isotp_reset(&s->rx);s->phase=phase;}
static void advance(ag_discovery_t *s,ag_discovery_ecu_result_t result){
    s->ecus[s->current].result=result;++s->current;s->page=0;ag_isotp_reset(&s->rx);
    s->phase=s->current==s->count?AG_DISCOVERY_COMPLETE:AG_DISCOVERY_READY;
}
bool ag_discovery_init(ag_discovery_t *s,const ag_can_profile_t *profiles,size_t count,uint32_t timeout){
    if(s==NULL||profiles==NULL||count==0||count>AG_DISCOVERY_ECU_MAX||timeout==0)return false;
    for(size_t i=0;i<count;++i){
        if(!ag_can_profile_valid(&profiles[i])||profiles[i].request_id==0x7dfu||profiles[i].request_id==0x18db33f1u
            ||profiles[i].bitrate!=profiles[0].bitrate||profiles[i].id_format!=profiles[0].id_format)return false;
        for(size_t j=0;j<i;++j)if(profiles[i].request_id==profiles[j].request_id||profiles[i].response_id==profiles[j].response_id)return false;
    }
    memset(s,0,sizeof(*s));s->count=count;s->response_timeout_ms=timeout;s->initialized=true;
    for(size_t i=0;i<count;++i){s->ecus[i].profile=profiles[i];ag_obd_capabilities_init(&s->ecus[i].capabilities,profiles[i].response_id);}
    return true;
}
bool ag_discovery_tick(ag_discovery_t *s,const ag_can_policy_t *policy,uint64_t now){
    if(s==NULL||!s->initialized||terminal(s))return false;
    if(s->has_time&&now<s->last_ms){abort_discovery(s,AG_DISCOVERY_CLOCK_FAULT);return false;}
    s->last_ms=now;s->has_time=true;
    if(!ag_can_tx_allowed(policy)){abort_discovery(s,AG_DISCOVERY_ABORTED);return false;}
    if(s->phase==AG_DISCOVERY_WAITING&&now-s->started_ms>=s->response_timeout_ms)advance(s,AG_DISCOVERY_ECU_TIMEOUT);
    return true;
}
bool ag_discovery_prepare(ag_discovery_t *s,const ag_can_policy_t *policy,uint64_t now){
    if(!ag_discovery_tick(s,policy,now)||s->phase!=AG_DISCOVERY_READY)return false;
    s->phase=AG_DISCOVERY_PREPARED;return true;
}
bool ag_discovery_dispatch(ag_discovery_t *s,const ag_can_policy_t *policy,uint64_t now,ag_simulated_dispatch_t *out){
    if(out==NULL)return false;
    memset(out,0,sizeof(*out));
    if(!ag_discovery_tick(s,policy,now)||s->phase!=AG_DISCOVERY_PREPARED)return false;
    ag_simulated_dispatch_t candidate={0};candidate.profile=s->ecus[s->current].profile;
    if(!ag_obd_make_read_request(policy,NULL,AG_READ_DISCOVERY,s->page,&candidate.request)
        ||!ag_isotp_init(&s->rx,&candidate.profile,s->response_timeout_ms)){abort_discovery(s,AG_DISCOVERY_ABORTED);return false;}
    s->started_ms=now;s->phase=AG_DISCOVERY_WAITING;*out=candidate;return true;
}
ag_discovery_rx_result_t ag_discovery_receive(ag_discovery_t *s,const ag_can_policy_t *policy,const ag_can_frame_t *frame,uint64_t now){
    if(!ag_discovery_tick(s,policy,now))return AG_DISCOVERY_RX_BLOCKED;
    if(s->phase!=AG_DISCOVERY_WAITING||frame==NULL)return AG_DISCOVERY_RX_IGNORED;
    const ag_can_profile_t *profile=&s->ecus[s->current].profile;
    if(frame->id!=profile->response_id||frame->extended!=(profile->id_format==AG_CAN_ID_29))return AG_DISCOVERY_RX_IGNORED;
    /* Bitmap responses fit one classic-CAN SF. No FF/FC or arbitrary PDU path. */
    if(frame->length==0||frame->length>8||(frame->data[0]>>4)!=0){advance(s,AG_DISCOVERY_ECU_MALFORMED);return AG_DISCOVERY_RX_ERROR;}
    ag_isotp_result_t rx=ag_isotp_feed(&s->rx,frame,now);
    if(rx!=AG_ISOTP_COMPLETE){advance(s,AG_DISCOVERY_ECU_MALFORMED);return AG_DISCOVERY_RX_ERROR;}
    uint8_t bytes[AG_ISOTP_CAPACITY];size_t length=0;
    if(!ag_isotp_take(&s->rx,bytes,sizeof(bytes),&length)){advance(s,AG_DISCOVERY_ECU_MALFORMED);return AG_DISCOVERY_RX_ERROR;}
    if(length==3&&bytes[0]==0x7f&&bytes[1]==1){
        s->ecus[s->current].negative_response_code=bytes[2];advance(s,AG_DISCOVERY_ECU_NEGATIVE);return AG_DISCOVERY_RX_ERROR;
    }
    if(length==6&&bytes[0]==0x41&&bytes[1]!=s->page)return AG_DISCOVERY_RX_IGNORED;
    if(length!=6||bytes[0]!=0x41||bytes[1]!=s->page
        ||!ag_obd_capabilities_apply(&s->ecus[s->current].capabilities,profile->response_id,bytes,length)){
        advance(s,AG_DISCOVERY_ECU_MALFORMED);return AG_DISCOVERY_RX_ERROR;
    }
    /* The final bitmap bit advertises the next supported-PID page. E0 cannot
     * wrap to00, even if its last bit is set by the ECU. */
    if(s->page<0xe0&&(bytes[5]&1u)){s->page=(uint8_t)(s->page+32u);s->phase=AG_DISCOVERY_READY;}
    else advance(s,AG_DISCOVERY_ECU_COMPLETE);
    return AG_DISCOVERY_RX_ACCEPTED;
}
bool ag_discovery_tx_failed(ag_discovery_t *s,const ag_can_policy_t *policy,uint64_t now){
    if(!ag_discovery_tick(s,policy,now)||s->phase!=AG_DISCOVERY_WAITING)return false;
    advance(s,AG_DISCOVERY_ECU_TX_FAILED);return true;
}
bool ag_discovery_capabilities(const ag_discovery_t *s,size_t index,ag_obd_capabilities_t *out){
    if(s==NULL||out==NULL||!s->initialized||s->phase!=AG_DISCOVERY_COMPLETE||index>=s->count)return false;
    *out=s->ecus[index].capabilities;return true;
}
bool ag_discovery_usable(const ag_discovery_t *s,ag_pid_mask_t *out){
    if(s==NULL||out==NULL||!s->initialized||s->phase!=AG_DISCOVERY_COMPLETE)return false;
    ag_pid_mask_t combined={0};
    for(size_t i=0;i<s->count;++i){ag_pid_mask_t usable;ag_obd_usable_pids(&s->ecus[i].capabilities,&usable);
        for(size_t word=0;word<AG_PID_MASK_WORDS;++word)combined.words[word]|=usable.words[word];}
    *out=combined;return true;
}
ag_default_config_result_t ag_discovery_build_defaults(const ag_discovery_t *s,const ag_default_config_options_t *options,ag_config_t *out){
    if(options==NULL||out==NULL||options->maximum_dashboards==0||options->maximum_dashboards>AG_MAX_DASHBOARDS
        ||options->widgets_per_dashboard==0||options->widgets_per_dashboard>AG_MAX_WIDGETS||options->manual_percent>100)return AG_DEFAULT_CONFIG_INVALID;
    ag_pid_mask_t usable;if(!ag_discovery_usable(s,&usable))return AG_DEFAULT_CONFIG_NOT_READY;
    static const uint8_t priority[]={0x0c,0x0d,0x05,0x42,0x0f,0x10,0x11,0x04,0x0b,0x23,0x5c,0x46,0x2f,0x5e,0xa6};
    uint8_t chosen[AG_MAX_DASHBOARDS*AG_MAX_WIDGETS];size_t count=0;
    const size_t maximum=(size_t)options->maximum_dashboards*options->widgets_per_dashboard;
    for(size_t i=0;i<sizeof(priority)&&count<maximum;++i)if(ag_pid_mask_has(&usable,priority[i]))chosen[count++]=priority[i];
    for(unsigned pid=1;pid<=255&&count<maximum;++pid){
        if(!ag_pid_mask_has(&usable,(uint8_t)pid))continue;
        bool seen=false;for(size_t i=0;i<count;++i)if(chosen[i]==pid)seen=true;
        if(!seen)chosen[count++]=(uint8_t)pid;
    }
    if(count==0)return AG_DEFAULT_CONFIG_EMPTY;
    ag_config_t candidate={0};candidate.schema_version=AG_CONFIG_VERSION;candidate.revision=1;
    candidate.brightness_mode=AG_BRIGHTNESS_MANUAL;candidate.manual_percent=options->manual_percent;
    candidate.dashboard_count=(count+options->widgets_per_dashboard-1)/options->widgets_per_dashboard;
    size_t at=0;
    for(size_t d=0;d<candidate.dashboard_count;++d){
        ag_dashboard_t *dashboard=&candidate.dashboards[d];dashboard->id=(int32_t)d+1;
        dashboard->widget_count=count-at;if(dashboard->widget_count>options->widgets_per_dashboard)dashboard->widget_count=options->widgets_per_dashboard;
        for(size_t w=0;w<dashboard->widget_count;++w){
            ag_widget_t *widget=&dashboard->widgets[w];widget->id=(int32_t)w+1;widget->pid=chosen[at++];
            int32_t top=(int32_t)(6000*w/dashboard->widget_count),bottom=(int32_t)(6000*(w+1)/dashboard->widget_count);
            widget->rect=(ag_rect_t){2000,2000+top,6000,bottom-top};
        }
    }
    if(ag_config_validate(&candidate,&usable)!=AG_CONFIG_OK)return AG_DEFAULT_CONFIG_INVALID;
    *out=candidate;return AG_DEFAULT_CONFIG_OK;
}
