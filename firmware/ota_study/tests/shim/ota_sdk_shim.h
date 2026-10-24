/* Behavioral test doubles ONLY: not SDK ABI, cryptography or flash emulation. */
#ifndef AG_OTA_TEST_SDK_H
#define AG_OTA_TEST_SDK_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#define CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK 0
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 0x10
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_DATA_OTA 0
typedef struct {uint32_t address,size;uint8_t type,subtype;} esp_partition_t;
typedef uint32_t esp_ota_handle_t;
typedef enum {ESP_OTA_IMG_NEW=0,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID,ESP_OTA_IMG_INVALID} esp_ota_img_states_t;
typedef struct {uint32_t secure_version;char version[32];} esp_app_desc_t;
const esp_partition_t *esp_partition_verify(const esp_partition_t *);
const esp_partition_t *esp_partition_find_first(uint8_t,uint8_t,const char *);
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_boot_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
esp_err_t esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *);
typedef int psa_status_t;
typedef unsigned psa_algorithm_t;
typedef struct {size_t length;bool active;} psa_hash_operation_t;
#define PSA_SUCCESS 0
#define PSA_ALG_SHA_256 0x02000009u
#define PSA_HASH_OPERATION_INIT {0,false}
psa_status_t psa_crypto_init(void);
psa_status_t psa_hash_setup(psa_hash_operation_t *,psa_algorithm_t);
psa_status_t psa_hash_update(psa_hash_operation_t *,const uint8_t *,size_t);
psa_status_t psa_hash_finish(psa_hash_operation_t *,uint8_t *,size_t,size_t *);
psa_status_t psa_hash_abort(psa_hash_operation_t *);
/* Manifest-only doubles; installer harness does not call these declarations. */
typedef uint32_t psa_key_id_t;
typedef struct {unsigned lifetime,type,bits,usage;psa_algorithm_t algorithm;} psa_key_attributes_t;
#define PSA_KEY_ATTRIBUTES_INIT {0,0,0,0,0}
#define PSA_KEY_LIFETIME_VOLATILE 0u
#define PSA_KEY_TYPE_RSA_PUBLIC_KEY 0x4001u
#define PSA_KEY_USAGE_VERIFY_HASH 0x00002000u
#define PSA_ALG_RSA_PSS(hash) (0x06000300u|((hash)&0xffu))
psa_status_t psa_hash_compute(psa_algorithm_t,const uint8_t *,size_t,uint8_t *,size_t,size_t *);
void psa_set_key_lifetime(psa_key_attributes_t *,unsigned);
void psa_set_key_type(psa_key_attributes_t *,unsigned);
void psa_set_key_bits(psa_key_attributes_t *,size_t);
void psa_set_key_usage_flags(psa_key_attributes_t *,unsigned);
void psa_set_key_algorithm(psa_key_attributes_t *,psa_algorithm_t);
void psa_reset_key_attributes(psa_key_attributes_t *);
psa_status_t psa_import_key(const psa_key_attributes_t *,const uint8_t *,size_t,psa_key_id_t *);
psa_status_t psa_get_key_attributes(psa_key_id_t,psa_key_attributes_t *);
unsigned psa_get_key_type(const psa_key_attributes_t *);
size_t psa_get_key_bits(const psa_key_attributes_t *);
psa_status_t psa_verify_hash(psa_key_id_t,psa_algorithm_t,const uint8_t *,size_t,const uint8_t *,size_t);
psa_status_t psa_destroy_key(psa_key_id_t);
#endif
