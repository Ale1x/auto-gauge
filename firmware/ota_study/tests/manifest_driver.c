/* Actual target parser/verifier, behavioral PSA doubles. The generated bundle
 * is independently signed/verified by OpenSSL. No RSA/SHA is implemented here. */
#include "manifest_verify.h"
#include "ota_sdk_shim.h"
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

enum fault {F_NONE=0,F_INIT,F_HASH,F_HASH_LENGTH,F_IMPORT,F_IMPORT_ALLOC,
    F_ATTRIBUTES,F_BITS,F_TYPE,F_VERIFY,F_DESTROY,F_IMPORT_ZERO,F_LOOKUP,
    F_LOOKUP_SIZE,F_IMPORT_ALLOC_DESTROY};
static struct {
    const manifest_vector_t *vector;
    enum fault fault;
    unsigned hash_calls,import_calls,attribute_calls,verify_calls,destroy_calls,resets,lookups;
    bool active,canonical_match;
} mock;

psa_status_t psa_crypto_init(void){return mock.fault==F_INIT?-1:PSA_SUCCESS;}
psa_status_t psa_hash_compute(psa_algorithm_t algorithm,const uint8_t *input,size_t length,
    uint8_t *output,size_t capacity,size_t *written){
    assert(algorithm==PSA_ALG_SHA_256&&capacity==32&&length<=AG_MANIFEST_SIGNING_MAX);
    ++mock.hash_calls;
    mock.canonical_match=length==mock.vector->canonical_length
        &&memcmp(input,mock.vector->canonical,length)==0;
    /* NONCRYPTO: known fixture bytes select its externally computed digest.
     * Any other bytes deliberately select a different model digest. */
    memcpy(output,mock.vector->digest,32);if(!mock.canonical_match)output[0]^=1;
    *written=mock.fault==F_HASH_LENGTH?31:32;
    return mock.fault==F_HASH?-1:PSA_SUCCESS;
}
void psa_set_key_lifetime(psa_key_attributes_t *a,unsigned value){a->lifetime=value;}
void psa_set_key_type(psa_key_attributes_t *a,unsigned value){a->type=value;}
void psa_set_key_bits(psa_key_attributes_t *a,size_t value){a->bits=(unsigned)value;}
void psa_set_key_usage_flags(psa_key_attributes_t *a,unsigned value){a->usage=value;}
void psa_set_key_algorithm(psa_key_attributes_t *a,psa_algorithm_t value){a->algorithm=value;}
void psa_reset_key_attributes(psa_key_attributes_t *a){++mock.resets;memset(a,0,sizeof(*a));}
psa_status_t psa_import_key(const psa_key_attributes_t *a,const uint8_t *der,size_t length,psa_key_id_t *key){
    ++mock.import_calls;
    assert(a->lifetime==PSA_KEY_LIFETIME_VOLATILE&&a->type==PSA_KEY_TYPE_RSA_PUBLIC_KEY
        &&a->bits==3072&&a->usage==PSA_KEY_USAGE_VERIFY_HASH&&a->algorithm==PSA_ALG_RSA_PSS(PSA_ALG_SHA_256));
    assert(length<=AG_MANIFEST_KEY_MAX);
    if(length!=mock.vector->der_length||memcmp(der,mock.vector->der,length)!=0)return -1;
    if(mock.fault==F_IMPORT)return -1;
    if(mock.fault==F_IMPORT_ZERO){*key=0;return PSA_SUCCESS;}
    *key=7;mock.active=true;
    return mock.fault==F_IMPORT_ALLOC||mock.fault==F_IMPORT_ALLOC_DESTROY?-1:PSA_SUCCESS;
}
psa_status_t psa_get_key_attributes(psa_key_id_t key,psa_key_attributes_t *a){
    assert(key==7&&mock.active);++mock.attribute_calls;
    a->bits=mock.fault==F_BITS?3070:3072;
    a->type=mock.fault==F_TYPE?0:PSA_KEY_TYPE_RSA_PUBLIC_KEY;
    return mock.fault==F_ATTRIBUTES?-1:PSA_SUCCESS;
}
unsigned psa_get_key_type(const psa_key_attributes_t *a){return a->type;}
size_t psa_get_key_bits(const psa_key_attributes_t *a){return a->bits;}
psa_status_t psa_verify_hash(psa_key_id_t key,psa_algorithm_t algorithm,const uint8_t *digest,
    size_t digest_length,const uint8_t *signature,size_t signature_length){
    assert(key==7&&mock.active&&algorithm==PSA_ALG_RSA_PSS(PSA_ALG_SHA_256));
    assert(digest_length==32&&signature_length==384);++mock.verify_calls;
    /* MODEL ACCEPTANCE, not RSA verification: every byte and the externally
     * established fixed-salt validity must agree with this exact fixture. */
    return mock.fault!=F_VERIFY&&mock.vector->crypto_valid&&mock.canonical_match
        &&memcmp(digest,mock.vector->digest,32)==0
        &&signature_length==mock.vector->signature_length
        &&memcmp(signature,mock.vector->signature,signature_length)==0?PSA_SUCCESS:-1;
}
psa_status_t psa_destroy_key(psa_key_id_t key){
    assert(key==7&&mock.active);++mock.destroy_calls;
    if(mock.fault==F_DESTROY||mock.fault==F_IMPORT_ALLOC_DESTROY)return -1;
    mock.active=false;return PSA_SUCCESS;
}
static size_t lookup(void *context,const char *key_id,uint8_t *out,size_t capacity){
    assert(context==&mock&&capacity==AG_MANIFEST_KEY_MAX);++mock.lookups;
    if(mock.fault==F_LOOKUP)return 0;
    if(mock.fault==F_LOOKUP_SIZE)return capacity+1;
    if(strcmp(key_id,mock.vector->key_id)||mock.vector->der_length>capacity)return 0;
    memcpy(out,mock.vector->der,mock.vector->der_length);return mock.vector->der_length;
}
static ag_manifest_trust_t setup(const manifest_vector_t *vector,enum fault fault){
    memset(&mock,0,sizeof(mock));mock.vector=vector;mock.fault=fault;
    ag_manifest_trust_t trust={.inactive_capacity=vector->capacity,.minimum_secure_version=2,
        .accepted_release_floor=vector->floor,.key_context=&mock,.lookup_key=lookup};
    assert(strlen(vector->hardware)<sizeof(trust.hardware_id));strcpy(trust.hardware_id,vector->hardware);
    return trust;
}
static ag_manifest_result_t attempt(const ag_manifest_trust_t *trust,const uint8_t *bytes,size_t length){
    ag_manifest_verified_t out,before;memset(&out,0xa5,sizeof(out));before=out;
    ag_manifest_result_t result=ag_manifest_verify(trust,bytes,length,&out);
    if(result!=AG_MANIFEST_OK)assert(memcmp(&out,&before,sizeof(out))==0);
    else{
        assert(mock.vector->expected&&mock.canonical_match&&!mock.active&&mock.destroy_calls==1);
        assert(out.release_counter==mock.vector->release&&out.plan.image_bytes==mock.vector->image_bytes);
        assert(out.plan.minimum_secure_version==trust->minimum_secure_version);
        assert(strcmp(out.plan.firmware_version,mock.vector->version)==0);
        assert(strcmp(out.signing_key_id,mock.vector->key_id)==0);
        assert(memcmp(out.plan.sha256,mock.vector->image_hash,32)==0);
    }
    return result;
}
static void test_generated_vectors(void){
    unsigned accepted=0;
    for(size_t i=0;i<kManifestVectorCount;++i){const manifest_vector_t *v=&kManifestVectors[i];
        ag_manifest_trust_t trust=setup(v,F_NONE);
        ag_manifest_result_t result=attempt(&trust,v->envelope,v->envelope_length);
        if((result==AG_MANIFEST_OK)!=v->expected){fprintf(stderr,"vector %s result=%d expected=%d\n",v->name,result,v->expected);assert(0);}
        if(v->expected)++accepted;
        assert(!mock.active);
    }
    assert(accepted>=7);
    printf("PASS: %zu ephemeral cross-language manifest vectors (%u accepted target-profile cases)\n",kManifestVectorCount,accepted);
}
static void test_psa_and_trust_failures(void){
    const manifest_vector_t *v=&kManifestVectors[0];assert(v->expected);
    for(enum fault f=F_INIT;f<=F_IMPORT_ALLOC_DESTROY;f=(enum fault)(f+1)){
        ag_manifest_trust_t trust=setup(v,f);
        assert(attempt(&trust,v->envelope,v->envelope_length)!=AG_MANIFEST_OK);
        if(mock.active)assert((f==F_DESTROY||f==F_IMPORT_ALLOC_DESTROY)&&mock.destroy_calls==1);
        if(f==F_LOOKUP||f==F_LOOKUP_SIZE)assert(mock.hash_calls==0&&mock.import_calls==0);
        if(f==F_IMPORT_ALLOC)assert(mock.destroy_calls==1&&!mock.active);
        if(f==F_BITS||f==F_TYPE||f==F_ATTRIBUTES)assert(mock.verify_calls==0&&mock.destroy_calls==1&&mock.resets==2);
    }
    for(unsigned variant=0;variant<4;++variant){ag_manifest_trust_t trust=setup(v,F_NONE);
        if(variant==0)trust.inactive_capacity=0;
        else if(variant==1)trust.inactive_capacity=AG_OTA_IMAGE_MAX+1;
        else if(variant==2)memset(trust.hardware_id,'A',sizeof(trust.hardware_id));
        else trust.lookup_key=NULL;
        assert(attempt(&trust,v->envelope,v->envelope_length)==AG_MANIFEST_POLICY&&mock.lookups==0);
    }
}
static void test_truncation_and_bounded_random_input(void){
    const manifest_vector_t *v=&kManifestVectors[0];
    for(size_t length=0;length<v->envelope_length;++length){ag_manifest_trust_t trust=setup(v,F_NONE);
        assert(attempt(&trust,v->envelope,length)!=AG_MANIFEST_OK);assert(!mock.active);}
    uint8_t bytes[4097];uint32_t random=0x54ad129b;
    for(unsigned iteration=0;iteration<1000;++iteration){
        random=random*1664525u+1013904223u;size_t length=random%sizeof(bytes);
        for(size_t i=0;i<length;++i){random=random*1664525u+1013904223u;bytes[i]=(uint8_t)(random>>24);}
        ag_manifest_trust_t trust=setup(v,F_NONE);assert(attempt(&trust,bytes,length)!=AG_MANIFEST_OK&&!mock.active);
    }
    ag_manifest_trust_t trust=setup(v,F_NONE);
    assert(attempt(&trust,NULL,1)==AG_MANIFEST_INVALID);
    assert(ag_manifest_verify(&trust,v->envelope,v->envelope_length,NULL)==AG_MANIFEST_INVALID);
}
static void test_installer_hook_and_immutable_floor(void){
    const manifest_vector_t *v=&kManifestVectors[0];ag_manifest_trust_t trust=setup(v,F_NONE),before=trust;
    ag_ota_plan_t plan;memset(&plan,0x6b,sizeof(plan));
    assert(ag_manifest_verify_hook(&trust,v->envelope,v->envelope_length,&plan));
    assert(plan.image_bytes==v->image_bytes&&memcmp(plan.sha256,v->image_hash,32)==0);
    assert(memcmp(&trust,&before,sizeof(trust))==0);
    ag_ota_plan_t old=plan;trust=setup(v,F_VERIFY);
    assert(!ag_manifest_verify_hook(&trust,v->envelope,v->envelope_length,&plan));
    assert(memcmp(&plan,&old,sizeof(plan))==0);
    assert(!ag_manifest_verify_hook(&trust,v->envelope,v->envelope_length,NULL));
}
int main(void){
    test_generated_vectors();test_psa_and_trust_failures();
    test_truncation_and_bounded_random_input();test_installer_hook_and_immutable_floor();
    puts("PASS: 4 target manifest groups (NONCRYPTO PSA model; no target/RSA/SHA qualification)");return 0;
}
