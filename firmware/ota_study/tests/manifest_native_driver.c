/* Actual manifest verifier + actual portable TF-PSA-Crypto, not PSA doubles.
 * Only SDK partition/handle types are opaque host declarations: no flash API.
 * The generated data contains PUBLIC ephemeral vectors, never private keys. */
#include "manifest_verify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const uint8_t *envelope,*canonical,*signature,*der,*digest,*image_hash;
    size_t envelope_length,canonical_length,signature_length,der_length;
    const char *hardware,*key_id,*version;
    uint32_t capacity,image_bytes;
    uint64_t floor,release;
    bool expected,crypto_valid;
} manifest_vector_t;
#include "ota_manifest_vectors.h"

static size_t lookup(void *context,const char *key,uint8_t *out,size_t capacity) {
    const manifest_vector_t *v=context;
    if(strcmp(key,v->key_id)||v->der_length>capacity)return 0;
    memcpy(out,v->der,v->der_length);return v->der_length;
}
int main(void) {
    size_t accepted=0;
    for(size_t i=0;i<kManifestVectorCount;++i) {
        const manifest_vector_t *v=&kManifestVectors[i];
        ag_manifest_trust_t trust={0};
        assert(strlen(v->hardware)<sizeof(trust.hardware_id));
        memcpy(trust.hardware_id,v->hardware,strlen(v->hardware)+1);
        trust.inactive_capacity=v->capacity;trust.accepted_release_floor=v->floor;
        trust.lookup_key=lookup;trust.key_context=(void *)v;trust.minimum_secure_version=1;
        ag_manifest_verified_t result,before;memset(&result,0xa5,sizeof(result));before=result;
        ag_manifest_result_t status=ag_manifest_verify(&trust,v->envelope,v->envelope_length,&result);
        if((status==AG_MANIFEST_OK)!=v->expected) {
            fprintf(stderr,"Native PSA vector failed: %s, status=%u\n",v->name,(unsigned)status);return 1;
        }
        if(v->expected) {
            ++accepted;
            assert(result.release_counter==v->release&&result.plan.image_bytes==v->image_bytes);
            assert(strcmp(result.plan.firmware_version,v->version)==0);
            assert(memcmp(result.plan.sha256,v->image_hash,32)==0);
        } else assert(memcmp(&result,&before,sizeof(result))==0);
    }
    printf("PASS: %zu native PSA vectors, %zu accepted; actual RSA-PSS/SHA256\n",kManifestVectorCount,accepted);
    return 0;
}
