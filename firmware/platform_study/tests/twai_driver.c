/* Actual core + TWAI port. Behavioral SDK/queue/timer; never physical CAN. */
#include "twai_port.h"
#include "esp_twai_onchip.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct mock_twai_node {bool enabled;};
static struct mock_twai_node node;
static ag_twai_port_t port;
static ag_runtime_t runtime;
static ag_discovery_t discovery;
static struct {
    int64_t us;ag_can_policy_t policy;twai_onchip_node_config_t config;twai_event_callbacks_t callbacks;void *context;
    twai_frame_header_t rx_header,tx_header;uint8_t rx[8],tx[8];const twai_frame_t *tx_pointer;
    twai_error_state_t node_state;unsigned new_calls,enable_calls,transmit_calls,disable_calls,delete_calls,policy_calls,info_calls;
    int new_rc,enable_rc,transmit_rc,disable_rc,delete_rc,receive_rc,info_rc,register_rc,filter_rc;
    unsigned revoke_policy_call;bool revoke_on_info,revoke_on_transmit,reenter,complete_in_transmit,allocated_on_error;
} mock;
int64_t esp_timer_get_time(void){return mock.us;}
QueueHandle_t xQueueCreateStatic(size_t capacity,size_t size,uint8_t *storage,StaticQueue_t *queue){*queue=(StaticQueue_t){capacity,size,0,0,storage};return queue;}
BaseType_t xQueueSendFromISR(QueueHandle_t queue,const void *item,BaseType_t *woken){*woken=pdFALSE;if(queue->count==queue->capacity)return pdFALSE;memcpy(queue->storage+((queue->head+queue->count)%queue->capacity)*queue->size,item,queue->size);++queue->count;return pdTRUE;}
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t ticks){assert(ticks==0);if(queue->count==0)return pdFALSE;memcpy(item,queue->storage+queue->head*queue->size,queue->size);queue->head=(queue->head+1)%queue->capacity;--queue->count;return pdTRUE;}
esp_err_t twai_new_node_onchip(const twai_onchip_node_config_t *config,twai_node_handle_t *out){
    ++mock.new_calls;mock.config=*config;assert(config->fail_retry_cnt==0&&config->tx_queue_depth==1&&!config->flags.enable_self_test&&!config->flags.enable_loopback&&!config->flags.enable_listen_only&&config->flags.no_receive_rtr);
    if(mock.new_rc==ESP_OK||mock.allocated_on_error)*out=&node;return mock.new_rc;
}
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t handle,const twai_event_callbacks_t *callbacks,void *context){assert(handle==&node);mock.callbacks=*callbacks;mock.context=context;return mock.register_rc;}
esp_err_t twai_node_config_mask_filter(twai_node_handle_t handle,uint8_t id,const twai_mask_filter_config_t *filter){assert(handle==&node&&id==0&&filter->no_fd&&!filter->no_classic&&!filter->dual_filter);return mock.filter_rc;}
esp_err_t twai_node_enable(twai_node_handle_t handle){assert(handle==&node);++mock.enable_calls;if(mock.enable_rc==ESP_OK)node.enabled=true;return mock.enable_rc;}
esp_err_t twai_node_disable(twai_node_handle_t handle){assert(handle==&node);++mock.disable_calls;if(mock.disable_rc==ESP_OK)node.enabled=false;return mock.disable_rc;}
esp_err_t twai_node_delete(twai_node_handle_t handle){assert(handle==&node&&!node.enabled);++mock.delete_calls;return mock.delete_rc;}
esp_err_t twai_node_get_info(twai_node_handle_t handle,twai_node_status_t *status,twai_node_record_t *record){assert(handle==&node&&record==NULL);++mock.info_calls;status->state=mock.node_state;if(mock.revoke_on_info)mock.policy.ignition=AG_IGN_OFF;return mock.info_rc;}
static void tx_callback(bool success){twai_tx_done_event_data_t event={success,mock.tx_pointer};unsigned policies=mock.policy_calls;mock.callbacks.on_tx_done(&node,&event,mock.context);assert(mock.policy_calls==policies);}
esp_err_t twai_node_transmit(twai_node_handle_t handle,const twai_frame_t *frame,int timeout){
    assert(handle==&node&&node.enabled&&timeout==0&&ag_can_tx_allowed(&mock.policy));++mock.transmit_calls;
    assert(frame->header.dlc==8&&!frame->header.rtr&&!frame->header.fdf&&!frame->header.brs&&frame->buffer_len==8);
    assert((frame->buffer[0]==2&&frame->buffer[1]==1)||frame->buffer[0]==0x30||frame->buffer[0]==0x32);
    mock.tx_header=frame->header;memcpy(mock.tx,frame->buffer,8);mock.tx_pointer=frame;
    if(mock.revoke_on_transmit)mock.policy.ignition=AG_IGN_OFF;
    if(mock.complete_in_transmit)tx_callback(true);return mock.transmit_rc;
}
esp_err_t twai_node_receive_from_isr(twai_node_handle_t handle,twai_frame_t *frame){assert(handle==&node&&frame->buffer_len==8);frame->header=mock.rx_header;memcpy(frame->buffer,mock.rx,8);return mock.receive_rc;}
static bool policy(void *context,ag_can_policy_t *out){
    assert(context==&mock);++mock.policy_calls;if(mock.revoke_policy_call==mock.policy_calls)mock.policy.ignition=AG_IGN_OFF;
    if(mock.reenter){assert(ag_twai_port_poll(&port)==AG_TWAI_BUSY);assert(ag_twai_port_close(&port)==AG_TWAI_BUSY);}
    *out=mock.policy;return true;
}
static ag_can_profile_t profile(bool extended,uint32_t bitrate){return (ag_can_profile_t){extended?AG_CAN_ID_29:AG_CAN_ID_11,bitrate,extended?0x18da10f1:0x7e0,extended?0x18daf110:0x7e8};}
static ag_twai_port_config_t config(bool extended,uint32_t bitrate){return (ag_twai_port_config_t){.tx_gpio=4,.rx_gpio=5,.bitrate=bitrate,.tx_timeout_ms=20,.maximum_rx_age_ms=100,.id_format=extended?AG_CAN_ID_29:AG_CAN_ID_11,.hardware_profile_qualified=true,.policy_context=&mock,.read_policy=policy};}
static void boot(bool extended,uint32_t bitrate){
    memset(&port,0,sizeof(port));memset(&runtime,0,sizeof(runtime));memset(&discovery,0,sizeof(discovery));memset(&mock,0,sizeof(mock));memset(&node,0,sizeof(node));
    mock.us=1000;mock.policy=(ag_can_policy_t){.power=AG_POWER_OBD,.ignition=AG_IGN_QUALIFIED_ON,.hardware_validated=true,.rails_valid=true,.diagnostic_session_active=true};
    ag_twai_port_config_t options=config(extended,bitrate);assert(ag_twai_port_init(&port,&options));assert(mock.new_calls==0);
}
static ag_runtime_store_result_t begin_store(void *context,const uint8_t sid[16]){(void)context;assert(sid[0]==1);return AG_RUNTIME_STORE_OK;}
static ag_runtime_store_result_t append_store(void *context,const ag_history_record_t *record){(void)context;(void)record;return AG_RUNTIME_STORE_OK;}
static void init_runtime(bool extended,uint32_t bitrate){
    ag_runtime_job_t job={0};job.job=(ag_poll_job_config_t){0x0c,profile(extended,bitrate),200,100,500};
    ag_obd_capabilities_init(&job.capabilities,job.job.profile.response_id);ag_pid_mask_set(&job.capabilities.advertised,0x0c);
    ag_config_t configuration={.schema_version=1,.revision=1,.dashboard_count=1,.manual_percent=50};
    configuration.dashboards[0].id=1;configuration.dashboards[0].widget_count=1;configuration.dashboards[0].widgets[0]=(ag_widget_t){1,0x0c,{2000,2000,6000,6000}};
    const uint8_t sid[16]={1};ag_runtime_sink_t sink={NULL,begin_store,append_store};
    assert(ag_runtime_init(&runtime,&configuration,&job,1,sid,&sink)&&ag_runtime_start(&runtime));
}
static void start_runtime(void){boot(false,500000);assert(ag_twai_port_start(&port)==AG_TWAI_OK);init_runtime(false,500000);}
static void rx(const uint8_t bytes[8],uint16_t length,bool rtr,bool fd){
    mock.rx_header=(twai_frame_header_t){.id=runtime.initialized?runtime.scheduler.jobs[0].config.profile.response_id:discovery.ecus[discovery.current].profile.response_id,
        .dlc=length,.ide=port.config.id_format==AG_CAN_ID_29,.rtr=rtr,.fdf=fd};memcpy(mock.rx,bytes,8);
    unsigned policies=mock.policy_calls;mock.callbacks.on_rx_done(&node,NULL,mock.context);assert(mock.policy_calls==policies);
}
static void test_init_start_and_variants(void){
    for(unsigned ext=0;ext<2;++ext)for(unsigned rate=0;rate<2;++rate){
        uint32_t bitrate=rate?250000:500000;boot(ext!=0,bitrate);assert(ag_twai_port_start(&port)==AG_TWAI_OK);
        assert(mock.config.io_cfg.tx==4&&mock.config.io_cfg.rx==5&&mock.config.bit_timing.bitrate==bitrate);
        ag_can_profile_t known=profile(ext!=0,bitrate);assert(ag_discovery_init(&discovery,&known,1,100));
        assert(ag_twai_port_send_discovery(&port,&discovery)==AG_TWAI_QUEUED&&mock.tx[0]==2&&mock.tx[1]==1&&mock.tx[2]==0);
        assert(port.pending&&port.tx_state==AG_TWAI_TX_QUEUED&&port.tx_completed==0);
        tx_callback(true);assert(port.pending);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED&&!port.pending&&port.tx_completed==1);
        uint8_t response[8]={6,0x41,0,0,0x10,0,0,0};rx(response,8,false,false);
        assert(discovery.phase==AG_DISCOVERY_WAITING);assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&discovery.phase==AG_DISCOVERY_COMPLETE);
        assert(ag_twai_port_close(&port)==AG_TWAI_OK&&mock.disable_calls==1&&mock.delete_calls==1);
    }
    boot(false,500000);port.config.hardware_profile_qualified=false;assert(ag_twai_port_start(&port)==AG_TWAI_DENIED&&mock.new_calls==0);
    ag_twai_port_t empty={0};ag_twai_port_config_t options=config(false,500000);options.tx_gpio=options.rx_gpio;assert(!ag_twai_port_init(&empty,&options));
    options=config(false,125000);assert(!ag_twai_port_init(&empty,&options));options=config(false,500000);options.tx_timeout_ms=0;assert(!ag_twai_port_init(&empty,&options));
}
static void test_runtime_completion_and_receive(void){
    start_runtime();mock.reenter=true;assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED&&runtime.scheduler.active_index==0);
    assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_BUSY&&mock.transmit_calls==1);
    tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    uint8_t response[8]={4,0x41,0x0c,0x1f,0x40,0,0,0};++mock.us;rx(response,8,false,false);assert(!runtime.scheduler.jobs[0].sample.has_value);
    assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&runtime.scheduler.jobs[0].sample.has_value&&runtime.scheduler.jobs[0].sample.value==2000&&port.rx_accepted==1);
    assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_NO_WORK&&mock.transmit_calls==1);assert(ag_twai_port_close(&port)==AG_TWAI_OK);
    start_runtime();mock.complete_in_transmit=true;assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED&&port.pending);
    assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
}
static void test_revocation_hops_and_ambiguous_enqueue(void){
    for(unsigned hop=1;hop<=8;++hop){
        start_runtime();mock.policy_calls=0;mock.revoke_policy_call=hop;
        ag_twai_result_t result=ag_twai_port_send_runtime(&port,&runtime);
        if(port.phase!=AG_TWAI_FAULT){mock.policy.ignition=AG_IGN_OFF;result=ag_twai_port_poll(&port);}
        assert(result==AG_TWAI_DENIED&&port.phase==AG_TWAI_FAULT&&mock.transmit_calls<=1&&mock.disable_calls==1);
        assert(runtime.scheduler.active_index==SIZE_MAX&&runtime.scheduler.pending_index==SIZE_MAX&&!runtime.scheduler.rx.active);
        unsigned before=mock.transmit_calls;mock.policy.ignition=AG_IGN_QUALIFIED_ON;
        assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_DENIED&&mock.transmit_calls==before);
    }
    start_runtime();mock.revoke_on_info=true;assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_DENIED&&mock.transmit_calls==0);
    start_runtime();mock.revoke_on_transmit=true;assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_DENIED&&mock.transmit_calls==1&&port.tx_state==AG_TWAI_TX_UNKNOWN);
    start_runtime();mock.transmit_rc=ESP_FAIL;assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_IO&&port.tx_state==AG_TWAI_TX_UNKNOWN&&runtime.scheduler.active_index==SIZE_MAX);
}
static void test_fail_timeout_busoff_and_cleanup(void){
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&port.tx_state==AG_TWAI_TX_FAILED&&runtime.scheduler.active_index==SIZE_MAX);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);mock.us+=20000;
    assert(ag_twai_port_poll(&port)==AG_TWAI_TIMEOUT&&port.tx_state==AG_TWAI_TX_UNKNOWN&&mock.disable_calls==1);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);mock.node_state=TWAI_ERROR_BUS_OFF;
    assert(ag_twai_port_poll(&port)==AG_TWAI_BUS_OFF&&runtime.scheduler.active_index==SIZE_MAX);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);mock.disable_rc=ESP_FAIL;mock.us+=20000;
    assert(ag_twai_port_poll(&port)==AG_TWAI_TIMEOUT&&port.cleanup_unknown&&mock.disable_calls==1);
    assert(ag_twai_port_close(&port)==AG_TWAI_IO&&mock.disable_calls==1&&mock.delete_calls==0);
    boot(false,500000);mock.new_rc=ESP_FAIL;mock.allocated_on_error=true;assert(ag_twai_port_start(&port)==AG_TWAI_IO);assert(ag_twai_port_close(&port)==AG_TWAI_OK&&mock.delete_calls==1);
    for(unsigned error=0;error<3;++error){boot(false,500000);if(error==0)mock.register_rc=ESP_FAIL;else if(error==1)mock.filter_rc=ESP_FAIL;else mock.info_rc=ESP_FAIL;
        assert(ag_twai_port_start(&port)==AG_TWAI_IO&&port.phase==AG_TWAI_FAULT);
        assert(ag_twai_port_close(&port)==AG_TWAI_OK&&mock.delete_calls==1);
    }
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);
    twai_state_change_event_data_t state={TWAI_ERROR_ACTIVE,TWAI_ERROR_BUS_OFF};mock.callbacks.on_state_change(&node,&state,mock.context);
    assert(ag_twai_port_poll(&port)==AG_TWAI_BUS_OFF&&runtime.scheduler.active_index==SIZE_MAX);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);
    twai_error_event_data_t error={0};error.err_flags.ack_err=true;mock.callbacks.on_error(&node,&error,mock.context);
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&runtime.scheduler.active_index==SIZE_MAX);
}
static void test_rx_faults_and_no_early_response(void){
    const uint8_t response[8]={4,0x41,0x0c,0x1f,0x40,0,0,0};
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);rx(response,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&port.deferred_count==1&&!runtime.scheduler.jobs[0].sample.has_value);
    mock.us+=1000;tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED&&!runtime.scheduler.jobs[0].sample.has_value&&port.rx_rejected==1);
    for(unsigned bad=0;bad<3;++bad){
        start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
        rx(response,bad==2?9:8,bad==0,bad==1);assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&!runtime.scheduler.jobs[0].sample.has_value&&port.rx_rejected==1);
    }
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);
    for(unsigned i=0;i<AG_TWAI_EVENT_CAPACITY+1;++i)rx(response,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&runtime.scheduler.active_index==SIZE_MAX);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    rx(response,8,false,false);mock.us+=100000;assert(ag_twai_port_poll(&port)==AG_TWAI_TIMEOUT&&!runtime.scheduler.jobs[0].sample.has_value);
    start_runtime();mock.us=0;assert(ag_twai_port_poll(&port)==AG_TWAI_CLOCK_FAULT);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);mock.receive_rc=ESP_FAIL;rx(response,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&runtime.scheduler.active_index==SIZE_MAX);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);
    twai_frame_t wrong={0};twai_tx_done_event_data_t wrong_tx={true,&wrong};mock.callbacks.on_tx_done(&node,&wrong_tx,mock.context);
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&port.tx_completed==0);
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    port.rx_rejected=UINT64_MAX;rx(response,8,true,false);assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&port.rx_rejected==UINT64_MAX);
}
static void test_flow_control_only_and_failure_cancels_rx(void){
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    uint8_t first[8]={0x10,8,0x41,0x0c,0,0,0,0};rx(first,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&runtime.scheduler.rx.fc_pending);
    assert(ag_twai_port_send_flow_control(&port,&runtime)==AG_TWAI_QUEUED&&mock.tx[0]==0x30&&mock.tx_header.id==0x7e0&&mock.transmit_calls==2);
    tx_callback(false);assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&runtime.scheduler.active_index==SIZE_MAX&&!runtime.scheduler.rx.fc_pending&&!runtime.scheduler.rx.active);
    assert(ag_twai_port_send_flow_control(&port,&runtime)==AG_TWAI_IO&&mock.transmit_calls==2);
    /* ISR observation times, not delayed task processing gaps, enforce STmin. */
    start_runtime();assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    first[1]=20;rx(first,8,false,false);assert(ag_twai_port_poll(&port)==AG_TWAI_OK&&runtime.scheduler.rx.fc_pending);
    runtime.scheduler.rx.fc_stmin_ms=5;
    assert(ag_twai_port_send_flow_control(&port,&runtime)==AG_TWAI_QUEUED&&mock.tx[2]==5);tx_callback(true);assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED);
    uint8_t cf[8]={0x21,0,0,0,0,0,0,0};mock.us=2000;rx(cf,8,false,false);assert(ag_twai_port_poll(&port)==AG_TWAI_OK);
    cf[0]=0x22;mock.us=3000;rx(cf,8,false,false);mock.us=20000;
    assert(ag_twai_port_poll(&port)==AG_TWAI_IO&&runtime.scheduler.active_index==SIZE_MAX);
}
static void test_response_deadline_begins_at_completion(void){
    const uint8_t response[8]={4,0x41,0x0c,0x1f,0x40,0,0,0};
    start_runtime();runtime.scheduler.jobs[0].config.response_timeout_ms=10;
    assert(ag_twai_port_send_runtime(&port,&runtime)==AG_TWAI_QUEUED);
    mock.us+=15000;tx_callback(true);mock.us+=1000;rx(response,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED&&port.rx_accepted==1&&runtime.scheduler.jobs[0].sample.status==AG_SAMPLE_VALID);
    boot(false,500000);assert(ag_twai_port_start(&port)==AG_TWAI_OK);ag_can_profile_t known=profile(false,500000);
    assert(ag_discovery_init(&discovery,&known,1,10)&&ag_twai_port_send_discovery(&port,&discovery)==AG_TWAI_QUEUED);
    mock.us+=15000;tx_callback(true);mock.us+=1000;const uint8_t bitmap[8]={6,0x41,0,0,0x10,0,0,0};rx(bitmap,8,false,false);
    assert(ag_twai_port_poll(&port)==AG_TWAI_COMPLETED&&discovery.ecus[0].result==AG_DISCOVERY_ECU_COMPLETE);
}
int main(void){test_init_start_and_variants();test_runtime_completion_and_receive();test_revocation_hops_and_ambiguous_enqueue();test_fail_timeout_busoff_and_cleanup();test_rx_faults_and_no_early_response();test_flow_control_only_and_failure_cancels_rx();test_response_deadline_begins_at_completion();puts("PASS: 7 modern TWAI port groups; behavioral SDK, no physical CAN");return 0;}
