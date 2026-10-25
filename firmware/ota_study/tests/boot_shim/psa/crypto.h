#ifndef BOOT_TEST_PSA_H
#define BOOT_TEST_PSA_H
#include <stddef.h>
#include <stdint.h>
typedef int psa_status_t;
#define PSA_SUCCESS 0
#define PSA_ALG_SHA_256 0x02000009u
typedef struct {uint8_t digest[32];size_t count;unsigned active;} psa_hash_operation_t;
#define PSA_HASH_OPERATION_INIT { {0},0,0 }
psa_status_t psa_crypto_init(void);
psa_status_t psa_hash_setup(psa_hash_operation_t *,uint32_t);
psa_status_t psa_hash_update(psa_hash_operation_t *,const uint8_t *,size_t);
psa_status_t psa_hash_finish(psa_hash_operation_t *,uint8_t *,size_t,size_t *);
psa_status_t psa_hash_abort(psa_hash_operation_t *);
#endif
