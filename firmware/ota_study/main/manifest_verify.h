#ifndef AG_MANIFEST_VERIFY_H
#define AG_MANIFEST_VERIFY_H
#include "ota_installer.h"
#define AG_MANIFEST_ENVELOPE_MAX 4096u
#define AG_MANIFEST_KEY_MAX 1024u
#define AG_MANIFEST_SIGNING_MAX 768u
/* Trusted local inputs, immutable throughout a call, one owner/no reentry.
 * Lookup copies a locally trusted DER PKCS#1 RSAPublicKey into output and returns
 * exact size (1..capacity), or0 on absent/failure. Never resolve a downloaded key. */
typedef struct {
    char hardware_id[65];
    uint32_t inactive_capacity,minimum_secure_version;
    uint64_t accepted_release_floor;
    void *key_context;
    size_t (*lookup_key)(void *,const char *key_id,uint8_t *,size_t capacity);
} ag_manifest_trust_t;
typedef struct {
    ag_ota_plan_t plan;
    uint64_t release_counter;
    char signing_key_id[65];
} ag_manifest_verified_t;
typedef enum {AG_MANIFEST_OK=0,AG_MANIFEST_INVALID,AG_MANIFEST_POLICY,
    AG_MANIFEST_KEY,AG_MANIFEST_CRYPTO,AG_MANIFEST_SIGNATURE,AG_MANIFEST_CLEANUP} ag_manifest_result_t;
/* Transactional output: untouched unless all checks AND key destroy succeed.
 * Exact target profile narrows host firmware_version64 to31 for ESP descriptor.
 * No image hash/read, flash, counter advancement or production key supplied. */
ag_manifest_result_t ag_manifest_verify(const ag_manifest_trust_t *,const uint8_t *,size_t,
                                         ag_manifest_verified_t *);
/* Explicit wrapper for ag_ota_hooks_t.verify_manifest. context points to a
 * trusted ag_manifest_trust_t, not wire data. Floor remains unchanged. */
bool ag_manifest_verify_hook(void *context,const uint8_t *,size_t,ag_ota_plan_t *);
#endif
