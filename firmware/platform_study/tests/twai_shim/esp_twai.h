#ifndef AG_TEST_TWAI_H
#define AG_TEST_TWAI_H
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct mock_twai_node *twai_node_handle_t;
typedef enum {TWAI_ERROR_ACTIVE=0,TWAI_ERROR_WARNING,TWAI_ERROR_PASSIVE,TWAI_ERROR_BUS_OFF} twai_error_state_t;
typedef struct {uint32_t id;uint16_t dlc;uint32_t ide:1,rtr:1,fdf:1,brs:1,esi:1;uint64_t timestamp;} twai_frame_header_t;
typedef struct {twai_frame_header_t header;uint8_t *buffer;size_t buffer_len;uint8_t tx_queue_priority;} twai_frame_t;
typedef struct {twai_error_state_t state;uint16_t tx_error_count,rx_error_count;uint32_t tx_queue_remaining;} twai_node_status_t;
typedef struct {uint32_t bus_err_num;} twai_node_record_t;
typedef struct {bool is_tx_success;const twai_frame_t *done_tx_frame;} twai_tx_done_event_data_t;
typedef struct {int reserved;} twai_rx_done_event_data_t;
typedef struct {twai_error_state_t old_sta,new_sta;} twai_state_change_event_data_t;
typedef union {struct {uint32_t arb_lost:1,bit_err:1,form_err:1,stuff_err:1,ack_err:1;};uint32_t val;} twai_error_flags_t;
typedef struct {twai_error_flags_t err_flags;} twai_error_event_data_t;
typedef struct {
    bool (*on_tx_done)(twai_node_handle_t,const twai_tx_done_event_data_t *,void *);
    bool (*on_rx_done)(twai_node_handle_t,const twai_rx_done_event_data_t *,void *);
    bool (*on_state_change)(twai_node_handle_t,const twai_state_change_event_data_t *,void *);
    bool (*on_error)(twai_node_handle_t,const twai_error_event_data_t *,void *);
} twai_event_callbacks_t;
typedef struct {uint32_t id,mask;uint32_t is_ext:1,no_classic:1,no_fd:1,dual_filter:1;} twai_mask_filter_config_t;
esp_err_t twai_node_enable(twai_node_handle_t);
esp_err_t twai_node_disable(twai_node_handle_t);
esp_err_t twai_node_delete(twai_node_handle_t);
esp_err_t twai_node_register_event_callbacks(twai_node_handle_t,const twai_event_callbacks_t *,void *);
esp_err_t twai_node_config_mask_filter(twai_node_handle_t,uint8_t,const twai_mask_filter_config_t *);
esp_err_t twai_node_get_info(twai_node_handle_t,twai_node_status_t *,twai_node_record_t *);
esp_err_t twai_node_transmit(twai_node_handle_t,const twai_frame_t *,int);
esp_err_t twai_node_receive_from_isr(twai_node_handle_t,twai_frame_t *);
#endif
