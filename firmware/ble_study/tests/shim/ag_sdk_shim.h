/* Test-only behavioral doubles. NOT SDK ABI definitions or radio emulation. */
#ifndef AG_TEST_SDK_SHIM_H
#define AG_TEST_SDK_SHIM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 1
#define CONFIG_BT_NIMBLE_SM_SC_ONLY 1
#define CONFIG_BT_NIMBLE_SM_LVL 3
#define CONFIG_BT_NIMBLE_NVS_PERSIST 1
#define CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS 0
#define CONFIG_BT_NIMBLE_HANDLE_REPEAT_PAIRING_DELETION 0
#define NIMBLE_HS_STACK_SIZE 32768
#define NIMBLE_CORE 0
#define configMAX_PRIORITIES 25
#define pdPASS 1
#define pdFAIL 0
typedef int esp_err_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define BLE_HS_ENOTCONN 7
#define BLE_HS_ENOENT 5
#define BLE_HS_EDONE 14
#define BLE_HS_EALREADY 2
#define BLE_HS_ESTORE_CAP 28
#define BLE_HS_CONN_HANDLE_NONE UINT16_MAX
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#define BLE_HS_FOREVER INT32_MAX
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN 5
#define BLE_ATT_ERR_WRITE_NOT_PERMITTED 3
#define BLE_ATT_ERR_READ_NOT_PERMITTED 2
#define BLE_ATT_ERR_UNLIKELY 14
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 13
#define BLE_GATT_ACCESS_OP_WRITE_CHR 2
#define BLE_GATT_SVC_TYPE_PRIMARY 1
#define BLE_GATT_CHR_F_WRITE 0x01
#define BLE_GATT_CHR_F_WRITE_ENC 0x02
#define BLE_GATT_CHR_F_WRITE_AUTHEN 0x04
#define BLE_GATT_CHR_F_INDICATE 0x08
#define BLE_HS_ADV_F_DISC_GEN 2
#define BLE_HS_ADV_F_BREDR_UNSUP 4
#define BLE_GAP_CONN_MODE_UND 2
#define BLE_GAP_DISC_MODE_GEN 2
#define BLE_SM_IOACT_DISP 3
#define BLE_HS_IO_DISPLAY_ONLY 0
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
enum { BLE_GAP_EVENT_CONNECT=1, BLE_GAP_EVENT_DISCONNECT,
    BLE_GAP_EVENT_PASSKEY_ACTION, BLE_GAP_EVENT_ENC_CHANGE,
    BLE_GAP_EVENT_SUBSCRIBE, BLE_GAP_EVENT_NOTIFY_TX, BLE_GAP_EVENT_REPEAT_PAIRING };
typedef struct {uint8_t type,val[6];} ble_addr_t;
static inline int ble_addr_cmp(const ble_addr_t *a,const ble_addr_t *b){return memcmp(a,b,sizeof(*a));}
typedef struct {uint8_t type;} ble_uuid_t;
typedef struct {ble_uuid_t u;uint8_t value[16];} ble_uuid128_t;
#define BLE_UUID128_INIT(...) { .u={128},.value={__VA_ARGS__} }
struct ble_gap_conn_desc {
    uint16_t conn_handle;
    ble_addr_t peer_id_addr;
    struct {bool encrypted,authenticated,bonded;uint8_t key_size;} sec_state;
};
struct ble_store_key_sec {ble_addr_t peer_addr;};
struct ble_store_value_sec {ble_addr_t peer_addr;bool ltk_present,sc,authenticated;uint8_t key_size;};
struct ble_store_status_event {int unused;};
struct ble_sm_io {uint8_t action;uint32_t passkey;};
struct ble_gap_event {
    int type;
    union {
        struct {int status;uint16_t conn_handle;} connect;
        struct {struct ble_gap_conn_desc conn;} disconnect;
        struct {uint16_t conn_handle;struct {uint8_t action;} params;} passkey;
        struct {uint16_t conn_handle;int status;} enc_change;
        struct {uint16_t conn_handle,attr_handle;bool cur_indicate;} subscribe;
        struct {uint16_t conn_handle,attr_handle;bool indication;int status;} notify_tx;
        struct {uint16_t conn_handle;} repeat_pairing;
    };
};
typedef int (*test_gap_fn)(struct ble_gap_event *,void *);
struct ble_gap_adv_params {uint8_t conn_mode,disc_mode;};
struct ble_hs_adv_fields {uint8_t flags;ble_uuid128_t *uuids128;uint8_t num_uuids128;bool uuids128_is_complete;};
struct os_mbuf {size_t length;uint8_t bytes[513];};
#define OS_MBUF_PKTLEN(m) ((m)->length)
struct ble_gatt_access_ctxt {uint8_t op;struct os_mbuf *om;};
typedef int (*test_access_fn)(uint16_t,uint16_t,struct ble_gatt_access_ctxt *,void *);
struct ble_gatt_chr_def {const ble_uuid_t *uuid;test_access_fn access_cb;uint16_t *val_handle;unsigned flags;};
struct ble_gatt_svc_def {uint8_t type;const ble_uuid_t *uuid;struct ble_gatt_chr_def *characteristics;};
struct ble_npl_event {int unused;};
struct ble_npl_callout {bool initialized,armed;void (*callback)(struct ble_npl_event *);};
struct test_hs_cfg {
    void (*reset_cb)(int);void (*sync_cb)(void);
    int (*store_status_cb)(struct ble_store_status_event *,void *);
    uint8_t sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_sc_only,sm_oob_data_flag,sm_our_key_dist,sm_their_key_dist;
};
extern struct test_hs_cfg ble_hs_cfg;
int64_t esp_timer_get_time(void);
int ble_gap_conn_find(uint16_t,struct ble_gap_conn_desc *);
int ble_store_read_peer_sec(const struct ble_store_key_sec *,struct ble_store_value_sec *);
int ble_gap_terminate(uint16_t,int);
int ble_npl_callout_reset(struct ble_npl_callout *,uint32_t);
uint32_t ble_npl_time_ms_to_ticks32(uint32_t);
int ble_npl_callout_init(struct ble_npl_callout *,void *,void (*)(struct ble_npl_event *),void *);
void ble_npl_callout_stop(struct ble_npl_callout *);
void ble_npl_callout_deinit(struct ble_npl_callout *);
uint16_t ble_att_mtu(uint16_t);
struct os_mbuf *ble_hs_mbuf_from_flat(const void *,uint16_t);
int ble_gatts_indicate_custom(uint16_t,uint16_t,struct os_mbuf *);
int os_mbuf_copydata(struct os_mbuf *,int,int,void *);
bool ble_gap_adv_active(void);
int ble_gap_adv_stop(void);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *);
int ble_gap_adv_start(uint8_t,const ble_addr_t *,int32_t,const struct ble_gap_adv_params *,test_gap_fn,void *);
int ble_gap_set_event_cb(uint16_t,test_gap_fn,void *);
int ble_gap_security_initiate(uint16_t);
int ble_sm_inject_io(uint16_t,const struct ble_sm_io *);
int ble_hs_util_ensure_addr(int);
int ble_hs_id_infer_auto(int,uint8_t *);
esp_err_t nimble_port_init(void);
esp_err_t nimble_port_deinit(void);
void nimble_port_run(void);
void *nimble_port_get_dflt_eventq(void);
void ble_svc_gap_init(void);
void ble_svc_gatt_init(void);
void ble_store_config_init(void);
int ble_gatts_count_cfg(const struct ble_gatt_svc_def *);
int ble_gatts_add_svcs(const struct ble_gatt_svc_def *);
BaseType_t xTaskCreatePinnedToCore(void (*)(void *),const char *,uint32_t,void *,unsigned,TaskHandle_t *,int);
void vTaskDelete(TaskHandle_t);
#endif
