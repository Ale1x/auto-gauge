# FW12 target manifest verifier, SDK compile study

`main/manifest_verify.h/.c` provide the target-side manifest authentication hook for the separate OTA installer study. They call the **real PSA Crypto API**, not a wire authentication flag or a simulated crypto routine. The study main remains empty: no key provisioning, radio, flash, boot selection, counter update or verifier initialization is invoked automatically. Product signing/trust keys are not included.

## Explicit local trust and output

`ag_manifest_trust_t` supplies an immutable local hardware ID, actual inactive-slot capacity, durable accepted release floor, minimum secure_version floor, and a mandatory local key lookup callback. Lookup copies at most 1024 bytes of DER **PKCS#1 RSAPublicKey** selected by an authenticated key ID into caller storage. The lookup must resolve locally trusted public keys; an envelope cannot supply its own trust anchor. The owner serializes calls, lookup and changes to these trusted inputs; no callback reentry or concurrent mutation is supported.

`ag_manifest_verify` returns a status and changes its output only after complete parsing, policy, signature verification and successful key destruction. `ag_manifest_verified_t` contains the installer plan, authenticated uint64 release counter and signing key ID. `ag_manifest_verify_hook` is an explicit wrapper for `ag_ota_hooks_t.verify_manifest`, with context pointing to trusted local configuration. It copies only the plan on success. Neither function advances the accepted floor. `AG_MANIFEST_CLEANUP` means a PSA handle may remain live; treat it as terminal, not a reason to blindly retry. The installer already faults on any failed hook.

This function authenticates the manifest's digest and size, not the candidate image bytes. The installer must still hash actual written bytes, perform SDK image checks and preserve boot/rollback sequencing. Trusted chip revision, image Secure Boot signature/provisioning, durable floor policy, rotation/revocation and post-boot health remain separate integration responsibilities.

## Bounded schema and canonical signing bytes

Envelope size is 1–4096 bytes. The purpose-built parser accepts exactly the two required objects: three envelope keys and eight manifest keys, any input order, no unknown or duplicate decoded keys, no arrays/extra nesting/trailing content. JSON whitespace is bounded by the envelope. Strings decode ASCII `\uXXXX` and JSON quote/backslash/slash escapes before matching. Non-ASCII, control characters, malformed escapes and surrogate forms cannot satisfy this ASCII schema and are rejected. Thus an escaped duplicate such as `schema_versi\u006fn` cannot evade the key mask.

Schema is exactly integer 1; target exactly `esp32s3`. Numbers are unsigned canonical JSON integer tokens with checked uint32 arithmetic; bool/null/fractions/exponents, leading zeros, overflow and strings in numeric fields are rejected. Hardware/key IDs follow `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`. Version follows `[A-Za-z0-9][A-Za-z0-9.+_-]{0,30}`. Digest is exactly 64 lowercase hexadecimal digits. Release counter is a canonical decimal **string**, 1–20 digits without leading zeros (except `0`) and checked uint64 overflow; it must exceed the trusted floor. Image size must be positive, at most 16 MiB and the trusted inactive capacity, and a multiple of 16.

The version limit 31 and image-size alignment are the explicit ESP target profile, stricter than generic Python manifest validation (version up to64, arbitrary positive image size). They match the installer descriptor/plan profile; this is not silent compatibility with every generic host package. Format/Secure Boot preflight remains separate.

Signature algorithm is exactly `RSA-PSS-SHA256`. A 384-byte RSA3072 signature has exactly512 Base64 characters and no padding; the decoder permits only the standard alphabet, rejects padding, whitespace and noncanonical forms. Encoded JSON escapes are decoded first, as in the Python reference.

Canonical bytes are the exact domain `AUTO-GAUGE-OTA-MANIFEST\0V1\0` (both explicit NULs retained), followed by compact sorted ASCII JSON with key order:

`firmware_version`, `hardware_id`, `image_bytes`, `image_sha256`, `release_counter`, `schema_version`, `signing_key_id`, `target`.

Validated values contain no characters needing JSON escaping. Integer output uses explicit canonical decimal conversion. The signing buffer is fixed at768 bytes and checked on every append; no input JSON lexical order/whitespace/escape spelling affects signed bytes. This is byte-compatible with `shared/reference/ota_manifest.py:signing_bytes` for the common target profile.

## Key and cryptographic policy

Before PSA import, a small bounded DER framing check requires one exact SEQUENCE containing exactly two minimally encoded positive INTEGERs, with no trailing bytes, SPKI wrapper, indefinite/nonminimal lengths or extra objects. The modulus must occupy385 DER bytes: leading zero followed by a high bit set, which establishes an exact3072-bit positive modulus. RSA mathematical validation is delegated to PSA; this is a framing check, not an RSA implementation.

The verifier initializes PSA, computes SHA256 over canonical bytes, imports a **volatile RSA_PUBLIC_KEY** with bits3072 and VERIFY_HASH usage, and requests `PSA_ALG_RSA_PSS(PSA_ALG_SHA_256)`. Imported attributes are read back and checked for exact public RSA type and3072 bits. Verification uses that same exact algorithm, never `PSA_ALG_RSA_PSS_ANY_SALT`. In the pinned PSA definitions/driver, standard PSS with SHA256 requires32-byte salt (digest length), whereas ANY_SALT is a separate policy. Digest length must be exactly32.

Attributes are reset after import/readback, and every nonzero imported handle is destroyed on all subsequent paths, including an abnormal failed import publishing a handle. Destroy failure prevents successful output. No persistent key ID, PSA key storage, private key or eFuse operation is requested.

Primary implementation contract read locally: ESP-IDF v6.1 commit `fff9895c82d744c7237be8847347bdd1b07c6643`, bundled `components/mbedtls/mbedtls/tf-psa-crypto/include/psa/crypto_values.h` (RSA-PSS standard vs ANY_SALT) and `drivers/builtin/src/psa_crypto_rsa.c` (import/key-byte-alignment/expected PSS salt). API compilation and host behavioral shims do not prove hardware cryptographic behavior.

## Resource and test boundary

The codec performs no heap allocation. Automatic buffers include DER1024, canonical768, signature384, bounded parsed strings, SHA256 output and transactional result objects. PSA import/verification may allocate internally; task stack and heap peaks must be measured on the actual target, including verifier+installer+PSA call depth, before runtime activation. A standalone `-fstack-usage` compile using the existing real Xtensa SDK compile-command flags measured static frames of2736 bytes for `ag_manifest_verify`,592 for `envelope`,96 for `manifest` and192 for the hook. These frames nest and exclude PSA/key-lookup/installer/caller stack and internal heap; they are not a whole-task high-water measurement. Temporary assembly/usage files were removed. Default main stack is not qualified. Input/key lookup remain immutable for the call; untrusted callers never choose a trusted floor/capacity or lookup implementation.

Independent host tests compare captured canonical/hash input byte-for-byte with Python, use common ephemeral OpenSSL fixtures, inspect exact PSA key/algorithm attributes and inject parse/policy/import/verify/destroy failures. A behavioral PSA shim validates orchestration, not RSA arithmetic or target cryptographic execution. Common real signatures are additionally verified by the actual OpenSSL reference. The dedicated target manifest harness passes four C groups/55 vectors (46 common vectors plus9 review cases), all prefix truncations,1000 random bounded inputs and PSA fault injection under ASan/UBSan; it also checks transactional hook outputs and unchanged floors. SDK compile/link of real PSA calls is a separate integration check; none of these checks installs firmware on a device.

## Additional native real-crypto execution

`scripts/check_native_ota_crypto.py` optionally builds an isolated temporary copy of the cached pinned TF-PSA-Crypto implementation and links this production verifier against its **actual portable RSA-PSS and SHA256**, without crypto function doubles. The recorded run in `native-crypto-report.json` passed55 vectors with13 accepted. The SDK handle/partition declarations are opaque host-only types; five forwarding headers select the portable private headers instead of ESP accelerator declarations. The vendor math sources are not replaced. The verifier/driver have ASan/UBSan instrumentation; the vendor library is a Debug build without sanitizer instrumentation.

The final native rerun matched all six recorded source hashes and confirmed clean pinned SDK/PSA trees. The script rejects tracked/untracked/submodule modifications, additionally rejects ignored files in PSA, and builds a `git archive` snapshot of the pinned PSA commit instead of copying the working tree. The report records the archive SHA256 and Git tree; symlinks/gitlinks and unsafe archive entries are rejected. Five temporary-repository regression tests cover altered cache, wrong pin, dirty submodule, unsupported entries and changes after the clean check. Ignored SDK build products outside the copied PSA source are not compiled by this check. Ephemeral OpenSSL private keys are removed by vector generation; the native compile receives public fixture data. The script uses cache only and does not download, edit the SDK or run by default in portable CI.

This evidence strengthens real portable cryptographic interoperability beyond the behavioral shim, but is **not ESP32 target execution** or proof of its accelerator/provider configuration, entropy, physical boot, flash, eFuse, Secure Boot provisioning or rollback behavior. The separate real SDK cross-link checks symbol/configuration compatibility; they do not convert host execution into hardware qualification.
