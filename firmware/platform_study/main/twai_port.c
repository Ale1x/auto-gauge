#include "twai_port.h"
#include "esp_twai_onchip.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include <string.h>
_Static_assert(ATOMIC_BOOL_LOCK_FREE==2,"ISR fault flag must be lock-free");
enum {EVENT_TX=1,EVENT_RX,EVENT_BUS_OFF,EVENT_ERROR};
static bool IRAM_ATTR push_isr(ag_twai_port_t *p,const ag_twai_event_t *event){
    BaseType_t woken=pdFALSE;
    if(xQueueSendFromISR(p->events,event,&woken)!=pdTRUE)atomic_store_explicit(&p->irq_fault,true,memory_order_release);
    return woken==pdTRUE;
}
static bool IRAM_ATTR stamp(ag_twai_port_t *p,ag_twai_event_t *event){
    int64_t us=esp_timer_get_time();if(us<0){atomic_store_explicit(&p->irq_fault,true,memory_order_release);return false;}
    event->observed_ms=(uint64_t)us/1000u;return true;
}
static bool IRAM_ATTR tx_done(twai_node_handle_t node,const twai_tx_done_event_data_t *data,void *context){
    ag_twai_port_t *p=context;ag_twai_event_t event={.kind=EVENT_TX};
    if(node!=p->node||data==NULL||data->done_tx_frame!=&p->tx_frame){atomic_store_explicit(&p->irq_fault,true,memory_order_release);return false;}
    event.success=data->is_tx_success;if(!stamp(p,&event))return false;
    return push_isr(p,&event);
}
static bool IRAM_ATTR rx_done(twai_node_handle_t node,const twai_rx_done_event_data_t *data,void *context){
    (void)data;ag_twai_port_t *p=context;ag_twai_event_t event={.kind=EVENT_RX};uint8_t bytes[8]={0};
    twai_frame_t frame={.buffer=bytes,.buffer_len=sizeof(bytes)};
    if(node!=p->node||twai_node_receive_from_isr(node,&frame)!=ESP_OK){atomic_store_explicit(&p->irq_fault,true,memory_order_release);return false;}
    event.frame.id=frame.header.id;event.frame.extended=frame.header.ide;event.frame.rtr=frame.header.rtr;event.frame.fd=frame.header.fdf;
    event.frame.length=frame.header.dlc>8?9:(uint8_t)frame.header.dlc;memcpy(event.frame.data,bytes,sizeof(bytes));
    if(!stamp(p,&event))return false;
    return push_isr(p,&event);
}
static bool IRAM_ATTR state_changed(twai_node_handle_t node,const twai_state_change_event_data_t *data,void *context){
    ag_twai_port_t *p=context;if(node!=p->node||data==NULL){atomic_store_explicit(&p->irq_fault,true,memory_order_release);return false;}
    if(data->new_sta!=TWAI_ERROR_BUS_OFF)return false;
    ag_twai_event_t event={.kind=EVENT_BUS_OFF};if(!stamp(p,&event))return false;
    return push_isr(p,&event);
}
static bool IRAM_ATTR error_seen(twai_node_handle_t node,const twai_error_event_data_t *data,void *context){
    ag_twai_port_t *p=context;if(node!=p->node||data==NULL){atomic_store_explicit(&p->irq_fault,true,memory_order_release);return false;}
    /* Arbitration alone is normal bus arbitration, not a software retry. */
    if(!data->err_flags.bit_err&&!data->err_flags.form_err&&!data->err_flags.stuff_err&&!data->err_flags.ack_err)return false;
    ag_twai_event_t event={.kind=EVENT_ERROR};if(!stamp(p,&event))return false;
    return push_isr(p,&event);
}
static void cancel_core(ag_twai_port_t *p){
    ag_can_policy_t blocked={0};uint64_t now=p->last_ms;
    if(p->discovery!=NULL&&p->discovery->initialized&&p->discovery->phase!=AG_DISCOVERY_COMPLETE){
        if(now<p->discovery->last_ms)now=p->discovery->last_ms;
        (void)ag_discovery_tick(p->discovery,&blocked,now);
    }
    if(p->runtime!=NULL&&p->runtime->initialized){
        ag_runtime_t *r=p->runtime;if(now<r->last_ms)now=r->last_ms;if(now<r->scheduler.last_ms)now=r->scheduler.last_ms;
        (void)ag_runtime_tick(r,&blocked,now);
        /* Even an already-faulted core must not retain a prepared FC/response. */
        ag_isotp_reset(&r->scheduler.rx);r->scheduler.pending_index=SIZE_MAX;r->scheduler.active_index=SIZE_MAX;
        for(size_t i=0;i<r->scheduler.count;++i)ag_measurement_unavailable(&r->scheduler.jobs[i].sample,AG_SAMPLE_UNAVAILABLE);
    }
}
static void disable_once(ag_twai_port_t *p){
    if(!p->enabled||p->disable_attempted)return;
    p->disable_attempted=true;
    if(twai_node_disable(p->node)!=ESP_OK)p->cleanup_unknown=true;else p->enabled=false;
}
static ag_twai_result_t fail(ag_twai_port_t *p,ag_twai_result_t why){
    if(p->phase!=AG_TWAI_FAULT){p->fault_reason=why;p->phase=AG_TWAI_FAULT;}
    if(p->pending&&p->tx_state==AG_TWAI_TX_QUEUED)p->tx_state=AG_TWAI_TX_UNKNOWN;
    p->pending=false;p->deferred_count=0;cancel_core(p);disable_once(p);return why;
}
static ag_twai_result_t count_rx(ag_twai_port_t *p,bool accepted){
    uint64_t *counter=accepted?&p->rx_accepted:&p->rx_rejected;
    if(*counter==UINT64_MAX)return fail(p,AG_TWAI_IO);
    ++*counter;return AG_TWAI_OK;
}
static ag_twai_result_t enter(ag_twai_port_t *p,bool running){
    if(p==NULL||!p->initialized)return AG_TWAI_INVALID;
    if(p->busy)return AG_TWAI_BUSY;
    if(p->phase==AG_TWAI_FAULT)return p->fault_reason;
    if(p->phase==AG_TWAI_CLOSED||(running&&p->phase!=AG_TWAI_RUNNING))return AG_TWAI_NOT_READY;
    p->busy=true;return AG_TWAI_OK;
}
static ag_twai_result_t leave(ag_twai_port_t *p,ag_twai_result_t result){p->busy=false;return result;}
static ag_twai_result_t guard(ag_twai_port_t *p,ag_can_policy_t *policy,bool check_node){
    memset(policy,0,sizeof(*policy));bool valid=p->config.read_policy(p->config.policy_context,policy);
    int64_t us=esp_timer_get_time();
    if(us<0||(p->has_time&&(uint64_t)us/1000u<p->last_ms))return fail(p,AG_TWAI_CLOCK_FAULT);
    p->last_ms=(uint64_t)us/1000u;p->has_time=true;
    if(!valid||!ag_can_tx_allowed(policy))return fail(p,policy->bus_off?AG_TWAI_BUS_OFF:AG_TWAI_DENIED);
    if(atomic_load_explicit(&p->irq_fault,memory_order_acquire))return fail(p,AG_TWAI_IO);
    if(check_node&&p->enabled){twai_node_status_t status;
        if(twai_node_get_info(p->node,&status,NULL)!=ESP_OK)return fail(p,AG_TWAI_IO);
        if(status.state==TWAI_ERROR_BUS_OFF)return fail(p,AG_TWAI_BUS_OFF);
    }
    return AG_TWAI_OK;
}
bool ag_twai_port_init(ag_twai_port_t *p,const ag_twai_port_config_t *config){
    if(p==NULL||config==NULL||p->initialized||config->read_policy==NULL||config->tx_gpio==config->rx_gpio
        ||!GPIO_IS_VALID_OUTPUT_GPIO(config->tx_gpio)||!GPIO_IS_VALID_GPIO(config->rx_gpio)
        ||(config->bitrate!=250000&&config->bitrate!=500000)||(config->id_format!=AG_CAN_ID_11&&config->id_format!=AG_CAN_ID_29)
        ||config->tx_timeout_ms==0||config->maximum_rx_age_ms==0)return false;
    uint32_t mask=config->id_format==AG_CAN_ID_11?0x7ffu:0x1fffffffu;
    if(config->rx_filter_id>mask||config->rx_filter_mask>mask)return false;
    ag_twai_port_config_t copy=*config;memset(p,0,sizeof(*p));p->config=copy;atomic_init(&p->irq_fault,false);
    p->events=xQueueCreateStatic(AG_TWAI_EVENT_CAPACITY,sizeof(ag_twai_event_t),p->queue_bytes,&p->queue_storage);
    if(p->events==NULL)return false;
    p->initialized=true;return true;
}
ag_twai_result_t ag_twai_port_start(ag_twai_port_t *p){
    ag_twai_result_t result=enter(p,false);if(result!=AG_TWAI_OK)return result;
    if(p->phase!=AG_TWAI_INITIALIZED)return leave(p,AG_TWAI_NOT_READY);
    if(!p->config.hardware_profile_qualified)return leave(p,fail(p,AG_TWAI_DENIED));
    ag_can_policy_t policy;result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    twai_onchip_node_config_t node={0};node.io_cfg.tx=p->config.tx_gpio;node.io_cfg.rx=p->config.rx_gpio;
    node.io_cfg.quanta_clk_out=node.io_cfg.bus_off_indicator=-1;node.bit_timing.bitrate=p->config.bitrate;
    node.fail_retry_cnt=0;node.tx_queue_depth=1;node.flags.no_receive_rtr=true;
    if(twai_new_node_onchip(&node,&p->node)!=ESP_OK||p->node==NULL)return leave(p,fail(p,AG_TWAI_IO));
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    twai_event_callbacks_t callbacks={tx_done,rx_done,state_changed,error_seen};
    if(twai_node_register_event_callbacks(p->node,&callbacks,p)!=ESP_OK)return leave(p,fail(p,AG_TWAI_IO));
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    twai_mask_filter_config_t filter={.id=p->config.rx_filter_id,.mask=p->config.rx_filter_mask,.is_ext=p->config.id_format==AG_CAN_ID_29,.no_fd=true};
    if(twai_node_config_mask_filter(p->node,0,&filter)!=ESP_OK)return leave(p,fail(p,AG_TWAI_IO));
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    p->enabled=true;
    if(twai_node_enable(p->node)!=ESP_OK)return leave(p,fail(p,AG_TWAI_IO));
    result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return leave(p,result);
    p->phase=AG_TWAI_RUNNING;return leave(p,AG_TWAI_OK);
}
static bool profile_matches(const ag_twai_port_t *p,const ag_can_profile_t *profile){return ag_can_profile_valid(profile)&&profile->bitrate==p->config.bitrate&&profile->id_format==p->config.id_format;}
static ag_twai_result_t receive_event(ag_twai_port_t *p,const ag_twai_event_t *event){
    ag_can_policy_t policy;ag_twai_result_t result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return result;
    const ag_can_frame_t *frame=&event->frame;
    bool active=false;
    if(p->discovery!=NULL&&p->discovery->phase==AG_DISCOVERY_WAITING){
        const ag_can_profile_t *profile=&p->discovery->ecus[p->discovery->current].profile;
        active=frame->id==profile->response_id&&frame->extended==(profile->id_format==AG_CAN_ID_29);
    }else if(p->runtime!=NULL&&p->runtime->scheduler.active_index<p->runtime->scheduler.count){
        const ag_can_profile_t *profile=&p->runtime->scheduler.jobs[p->runtime->scheduler.active_index].config.profile;
        active=frame->id==profile->response_id&&frame->extended==(profile->id_format==AG_CAN_ID_29);
    }
    if(!active)return count_rx(p,false);
    if(event->observed_ms>p->last_ms)return fail(p,AG_TWAI_CLOCK_FAULT);
    if(p->last_ms-event->observed_ms>=p->config.maximum_rx_age_ms)return fail(p,AG_TWAI_TIMEOUT);
    if(frame->fd||frame->rtr||frame->length==0||frame->length>8||frame->extended!=(p->config.id_format==AG_CAN_ID_29)
        ||event->observed_ms<p->tx_completed_ms)return count_rx(p,false);
    if(p->runtime!=NULL&&p->runtime->scheduler.active_index<p->runtime->scheduler.count
        &&frame->id==p->runtime->scheduler.rx.profile.response_id){
        ag_isotp_rx_t *rx=&p->runtime->scheduler.rx;uint8_t type=frame->data[0]>>4;
        if(type==1)p->has_cf=false;
        if(type==2&&rx->fc_enabled){
            if(p->has_cf&&(event->observed_ms<p->last_cf_ms||event->observed_ms-p->last_cf_ms<rx->fc_stmin_ms))return fail(p,AG_TWAI_IO);
            p->has_cf=true;p->last_cf_ms=event->observed_ms;
        }
    }
    bool accepted=false;
    if(p->discovery!=NULL)accepted=ag_discovery_receive(p->discovery,&policy,frame,p->last_ms)==AG_DISCOVERY_RX_ACCEPTED;
    else if(p->runtime!=NULL){ag_scheduler_rx_result_t rx=ag_runtime_receive(p->runtime,&policy,frame,p->last_ms);accepted=rx==AG_SCHED_RX_ACCEPTED||rx==AG_SCHED_RX_WAITING;}
    result=count_rx(p,accepted);if(result!=AG_TWAI_OK)return result;
    return guard(p,&policy,false);
}
static ag_twai_result_t pump(ag_twai_port_t *p){
    ag_can_policy_t policy;ag_twai_result_t result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return result;
    bool completed=false;ag_twai_event_t event;
    for(size_t n=0;n<AG_TWAI_EVENT_CAPACITY&&xQueueReceive(p->events,&event,0)==pdTRUE;++n){
        result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return result;
        if(event.kind==EVENT_BUS_OFF)return fail(p,AG_TWAI_BUS_OFF);
        if(event.kind==EVENT_ERROR)return fail(p,AG_TWAI_IO);
        if(event.kind==EVENT_TX){
            if(!p->pending||event.observed_ms<p->tx_started_ms||event.observed_ms>p->last_ms)return fail(p,AG_TWAI_IO);
            if(!event.success){p->tx_state=AG_TWAI_TX_FAILED;return fail(p,AG_TWAI_IO);}
            if(event.observed_ms-p->tx_started_ms>=p->config.tx_timeout_ms)return fail(p,AG_TWAI_TIMEOUT);
            p->pending=false;p->tx_state=AG_TWAI_TX_COMPLETE;p->tx_completed_ms=event.observed_ms;
            if(p->tx_bytes[0]==2&&p->tx_bytes[1]==1){
                // Response deadlines begin at the observed TX completion,
                // never at SDK enqueue. FC completion must not renew them.
                if(p->discovery!=NULL&&p->discovery->phase==AG_DISCOVERY_WAITING)p->discovery->started_ms=event.observed_ms;
                if(p->runtime!=NULL&&p->runtime->scheduler.active_index<p->runtime->scheduler.count)p->runtime->scheduler.active_started_ms=event.observed_ms;
            }
            if(p->tx_completed==UINT64_MAX)return fail(p,AG_TWAI_IO);
            ++p->tx_completed;completed=true;
            for(size_t i=0;i<p->deferred_count;++i){result=receive_event(p,&p->deferred[i]);if(result!=AG_TWAI_OK)return result;}
            p->deferred_count=0;
        }else if(event.kind==EVENT_RX){
            if(p->pending){if(p->deferred_count==AG_TWAI_EVENT_CAPACITY)return fail(p,AG_TWAI_IO);p->deferred[p->deferred_count++]=event;}
            else{result=receive_event(p,&event);if(result!=AG_TWAI_OK)return result;}
        }else return fail(p,AG_TWAI_IO);
        result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return result;
    }
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return result;
    if(p->pending&&p->last_ms-p->tx_started_ms>=p->config.tx_timeout_ms)return fail(p,AG_TWAI_TIMEOUT);
    if(!p->pending){
        if(p->discovery!=NULL&&!ag_discovery_tick(p->discovery,&policy,p->last_ms))return fail(p,AG_TWAI_IO);
        if(p->runtime!=NULL&&!ag_runtime_tick(p->runtime,&policy,p->last_ms))return fail(p,AG_TWAI_IO);
    }
    return completed?AG_TWAI_COMPLETED:AG_TWAI_OK;
}
static ag_twai_result_t transmit(ag_twai_port_t *p,const ag_can_frame_t *frame){
    ag_can_policy_t policy;ag_twai_result_t result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return result;
    memset(&p->tx_frame,0,sizeof(p->tx_frame));memcpy(p->tx_bytes,frame->data,8);p->tx_frame.header.id=frame->id;
    p->tx_frame.header.dlc=8;p->tx_frame.header.ide=frame->extended;p->tx_frame.buffer=p->tx_bytes;p->tx_frame.buffer_len=8;
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return result; /* immediately before actual SDK enqueue */
    p->tx_started_ms=p->last_ms;p->pending=true;p->tx_state=AG_TWAI_TX_QUEUED;
    esp_err_t queued=twai_node_transmit(p->node,&p->tx_frame,0);
    if(queued!=ESP_OK)return fail(p,AG_TWAI_IO);
    result=guard(p,&policy,true);return result==AG_TWAI_OK?AG_TWAI_QUEUED:result;
}
static ag_twai_result_t send(ag_twai_port_t *p,ag_discovery_t *discovery,ag_runtime_t *runtime,bool fc){
    ag_twai_result_t result=enter(p,true);if(result!=AG_TWAI_OK)return result;
    result=pump(p);if(result!=AG_TWAI_OK&&result!=AG_TWAI_COMPLETED)return leave(p,result);
    if(p->pending)return leave(p,AG_TWAI_BUSY);
    if(discovery!=NULL){if(!discovery->initialized||discovery->count==0||!profile_matches(p,&discovery->ecus[0].profile))return leave(p,AG_TWAI_INVALID);}
    else if(runtime==NULL||!runtime->initialized||!runtime->started||runtime->fault||runtime->scheduler.count==0
        ||!profile_matches(p,&runtime->scheduler.jobs[0].config.profile))return leave(p,AG_TWAI_INVALID);
    if(p->discovery!=NULL&&p->discovery!=discovery&&p->discovery->phase!=AG_DISCOVERY_COMPLETE)return leave(p,AG_TWAI_BUSY);
    if(p->runtime!=NULL&&p->runtime!=runtime&&(p->runtime->scheduler.pending_index!=SIZE_MAX||p->runtime->scheduler.active_index!=SIZE_MAX))return leave(p,AG_TWAI_BUSY);
    p->discovery=discovery;p->runtime=runtime;
    ag_can_policy_t policy;result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return leave(p,result);
    bool prepared=discovery!=NULL?ag_discovery_prepare(discovery,&policy,p->last_ms):fc?ag_runtime_fc_prepare(runtime,&policy,p->last_ms):ag_runtime_prepare(runtime,&policy,p->last_ms);
    if(!prepared)return leave(p,AG_TWAI_NO_WORK);
    result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    ag_can_frame_t frame={0};
    if(fc){if(!ag_runtime_fc_dispatch(runtime,&policy,p->last_ms,&frame))return leave(p,AG_TWAI_NO_WORK);
        if(frame.rtr||frame.fd||frame.length!=8||frame.extended!=(p->config.id_format==AG_CAN_ID_29)
            ||(frame.data[0]!=0x30&&frame.data[0]!=0x32))return leave(p,fail(p,AG_TWAI_IO));
    }else{
        ag_simulated_dispatch_t out;bool dispatched=discovery!=NULL?ag_discovery_dispatch(discovery,&policy,p->last_ms,&out):ag_runtime_dispatch(runtime,&policy,p->last_ms,&out);
        if(!dispatched)return leave(p,AG_TWAI_NO_WORK);
        if(!profile_matches(p,&out.profile)||out.request.length!=2||out.request.bytes[0]!=1)return leave(p,fail(p,AG_TWAI_IO));
        frame.id=out.profile.request_id;frame.extended=out.profile.id_format==AG_CAN_ID_29;frame.length=8;
        frame.data[0]=2;frame.data[1]=1;frame.data[2]=out.request.bytes[1];p->has_cf=false;
    }
    return leave(p,transmit(p,&frame));
}
ag_twai_result_t ag_twai_port_send_discovery(ag_twai_port_t *p,ag_discovery_t *s){return send(p,s,NULL,false);}
ag_twai_result_t ag_twai_port_send_runtime(ag_twai_port_t *p,ag_runtime_t *r){return send(p,NULL,r,false);}
ag_twai_result_t ag_twai_port_send_flow_control(ag_twai_port_t *p,ag_runtime_t *r){return send(p,NULL,r,true);}
ag_twai_result_t ag_twai_port_poll(ag_twai_port_t *p){ag_twai_result_t result=enter(p,true);if(result!=AG_TWAI_OK)return result;return leave(p,pump(p));}
ag_twai_result_t ag_twai_port_quiesce(ag_twai_port_t *p){
    ag_twai_result_t result=enter(p,false);if(result!=AG_TWAI_OK)return result;
    if(p->pending)return leave(p,AG_TWAI_BUSY);
    int64_t us=esp_timer_get_time();
    if(us<0||(p->has_time&&(uint64_t)us/1000u<p->last_ms))return leave(p,fail(p,AG_TWAI_CLOCK_FAULT));
    p->last_ms=(uint64_t)us/1000u;p->has_time=true;
    if(atomic_load_explicit(&p->irq_fault,memory_order_acquire))return leave(p,fail(p,AG_TWAI_IO));
    cancel_core(p);p->discovery=NULL;p->runtime=NULL;p->deferred_count=0;p->has_cf=false;
    ag_twai_event_t event;
    for(size_t n=0;n<AG_TWAI_EVENT_CAPACITY&&xQueueReceive(p->events,&event,0)==pdTRUE;++n){
        if(event.kind==EVENT_BUS_OFF)return leave(p,fail(p,AG_TWAI_BUS_OFF));
        if(event.kind!=EVENT_RX)return leave(p,fail(p,AG_TWAI_IO));
        if(count_rx(p,false)!=AG_TWAI_OK)return leave(p,p->fault_reason);
    }
    return leave(p,AG_TWAI_OK);
}
ag_twai_result_t ag_twai_port_suspend(ag_twai_port_t *p){
    ag_twai_result_t result=ag_twai_port_quiesce(p);if(result!=AG_TWAI_OK)return result;
    result=enter(p,true);if(result!=AG_TWAI_OK)return result;
    if(p->node==NULL||!p->enabled||p->disable_attempted)return leave(p,fail(p,AG_TWAI_IO));
    p->disable_attempted=true;
    if(twai_node_disable(p->node)!=ESP_OK){p->cleanup_unknown=true;return leave(p,fail(p,AG_TWAI_IO));}
    p->enabled=false;p->phase=AG_TWAI_SUSPENDED;return leave(p,AG_TWAI_OK);
}
ag_twai_result_t ag_twai_port_resume(ag_twai_port_t *p){
    ag_twai_result_t result=enter(p,false);if(result!=AG_TWAI_OK)return result;
    if(p->phase!=AG_TWAI_SUSPENDED||p->enabled||p->pending||p->node==NULL||p->cleanup_unknown)return leave(p,AG_TWAI_NOT_READY);
    ag_can_policy_t policy;result=guard(p,&policy,false);if(result!=AG_TWAI_OK)return leave(p,result);
    /* A successful enable starts a new disable epoch. Set before SDK call so
     * ambiguous enable failure is cleaned at most once, then remains faulted. */
    p->disable_attempted=false;p->enabled=true;
    if(twai_node_enable(p->node)!=ESP_OK)return leave(p,fail(p,AG_TWAI_IO));
    result=guard(p,&policy,true);if(result!=AG_TWAI_OK)return leave(p,result);
    p->phase=AG_TWAI_RUNNING;return leave(p,AG_TWAI_OK);
}
ag_twai_result_t ag_twai_port_close(ag_twai_port_t *p){
    if(p==NULL||!p->initialized)return AG_TWAI_INVALID;
    if(p->busy)return AG_TWAI_BUSY;
    if(p->phase==AG_TWAI_CLOSED)return AG_TWAI_NOT_READY;
    p->busy=true;if(p->pending)p->tx_state=AG_TWAI_TX_UNKNOWN;p->pending=false;cancel_core(p);disable_once(p);
    if(p->node!=NULL&&!p->enabled&&!p->delete_attempted){p->delete_attempted=true;if(twai_node_delete(p->node)!=ESP_OK)p->cleanup_unknown=true;else p->node=NULL;}
    p->phase=AG_TWAI_CLOSED;return leave(p,p->cleanup_unknown?AG_TWAI_IO:AG_TWAI_OK);
}
