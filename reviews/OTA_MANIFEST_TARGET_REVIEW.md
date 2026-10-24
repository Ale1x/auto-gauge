# FW-12 target manifest verifier — independent review

Date: 2026-10-24. Status: SOURCE_REVIEW_AND_HOST_PARSER_PSA_WIRING_TESTS_COMPLETED; NOT_TARGET_CRYPTO_QUALIFIED. Reviewer owns only this report, new host manifest test files and compatible test-only PSA shim additions. No production C, installation, SDK runtime/flash/radio, release keys, provisioning, commit or remote operation.

## Contract and test oracle

Read `shared/reference/ota_manifest.py`, `shared/OTA_V0.1.md` and the installer plan/lifecycle contract. The target verifier must enforce a closed4096-byte envelope with the exact manifest fields, JSON duplicate rejection after string decoding, correct primitive types, canonical uint64 release string strictly above a trusted floor, bounded image size/capacity and local key lookup. Target firmware-version length may be narrower than the Python64-byte host format because `ag_ota_plan_t` has32bytes; this must be an explicit fail-closed profile limit rather than truncation.

The signed byte oracle is the Python `signing_bytes()` output: domain `AUTO-GAUGE-OTA-MANIFEST\0V1\0` with actual embedded NULs followed by ASCII JSON sorted over all eight exact keys, no whitespace. Semantic input key order/whitespace or supported escapes must not change this canonical message. Duplicate aliases such as `schema_version` and `\u0073chema_version` must not bypass duplicate rejection. Unknown fields, floats/exponents/booleans in integer fields, noncanonical counter strings, invalid hex, noncanonical Base64, malformed/truncated/trailing JSON and oversize/depth must fail without modifying a caller's verified output.

Key policy: public-key DER comes only from bounded local trust lookup, never the envelope. Import type/bit attributes and imported key properties must both establish exactly RSA public3072bits, VERIFY_HASH usage and `PSA_ALG_RSA_PSS(PSA_ALG_SHA_256)`, not ANY_SALT. Local primary SDK `tf-psa-crypto/include/psa/crypto_values.h:1454–1477` defines this algorithm as MGF1 with the same hash and salt of hash length; with RSA3072/SHA256 that is32bytes. Signature is exactly384bytes decoded from512canonical Base64 characters. Signature validation is distinct from the installer's later hash of actual target bytes; no durable counter or boot state advances during manifest verification.

Host PSA doubles will be explicitly NONCRYPTOGRAPHIC: compare captured canonical bytes/digest, imported DER, attributes and signature with an ephemeral fixture independently generated/verified using Python/OpenSSL. They test API wiring and parser/data flow, not RSA/PSS/SHA mathematics or SDK cryptographic behavior. No homemade RSA implementation or production private key is permitted. Real pinned-SDK compile is separate evidence; no host-double result will be called target cryptographic qualification.

## Failure paths to exercise

- Invalid/unknown local key, lookup length0/over1024, wrong hardware/capacity/floor, imported type/bits mismatch and wrong fixed-salt algorithm must fail closed.
- PSA init/hash/import/get-attributes/verify/destroy errors, including cleanup failures, cannot leave a success output or silently bypass verification. Output remains byte-identical on every failed transaction.
- Capture exact signing bytes including both domain NULs, strict integer formatting beyond2^53/u64max, hash length32 and signature length384. Vary input order, whitespace and escaped duplicate aliases.
- Include real ephemeral OpenSSL fixtures and negative signatures for wrong canonical domain/body/algorithm where provided. Any mock rejection remains a model result, not independent PSS verification.
- Installer hook must fail when verification fails and return only the verified typed plan; it must not accept caller-controlled assertions or partially decoded metadata as authorized.

## Implementation state

Read the stable `ota_study/main/manifest_verify.{h,c}`. It has bounded local trust lookup, canonical buffer768, closed nonrecursive two-object schema, PSA hash/import/verify, attribute inspection and cleanup. It decodes printable ASCII escapes before key comparison and rejects control/non-ASCII/surrogate strings; because every schema string is ASCII, this is an explicit target profile, not full general Unicode JSON support. The version limit is31characters and image length must be divisible by16, narrower than the general Python reference. No truncation normalizes invalid values into a valid plan.

The implementation validates strict DER framing before import: one exact SEQUENCE, exactly two minimally encoded positive INTEGERs, no trailing bytes, and a385-byte signed modulus encoding with one required leading zero/top bit that establishes3072bits. This is framing/bit-length validation, not RSA arithmetic; PSA remains responsible for cryptographic key validity. Signature decoding is exactly512Base64 symbols into384bytes; since384is divisible by3, there are no padding bits to normalize. The key arrives only from local lookup.

## Final findings

No concrete unresolved P0/P1 defect was identified under the trusted immutable context, bounded lookup and single-owner/no-reentry contract. Source review and test capture agree on canonical byte ordering/domain, integer precision, exact target field/type rules, explicit PSS-SHA256 algorithm, volatile public-key import and actual imported type/bits inspection. Successful output is assigned only after successful key destruction. On cleanup error the result is CLEANUP and output remains untouched; this does not claim the key resource was freed. Caller must treat that as failure, not blindly retry a partially cleaned operation. The installer wrapper faults an admitted installation when its trusted verification hook returns false.

The target verifier does not read/hash the candidate image, verify its ESP container, advance a release counter, set boot state or provision keys. Image-only mutations are intentionally still accepted by manifest-only validation and rejected by the separate package/installer image checks. A correctly signed envelope does not turn a downloaded public key into a trust anchor.

## Reproducible verification

```sh
python3 -m unittest discover -s tests -p test_ota_manifest_target.py -v
python3 -m unittest discover -s tests -p test_ota_installer.py -v
```

Both pass independently. The new target-manifest test compiles unchanged production `manifest_verify.c` and new `manifest_driver.c` with C11 warnings as errors, ASan and UBSan in a temporary directory. It imports the coordinator's `scripts/generate_ota_vectors.py`, which generates ephemeral RSA keys inside its own temporary directory, signs variants and invokes the real OpenSSL-backed Python verifier before returning public vectors. Private keys and private paths are not returned or committed. Requirements are a host C compiler, Python and OpenSSL; no SDK build, package download or native-PSA installation is needed.

The current frozen bundle has46cases; this review adds9local parser variants, yielding55vectors and four test groups:

- Shared valid/reordered/whitespace/ASCII escaped input, every escaped signature symbol, maximum hardware/key/version profile, >2^53/u64max counters, hardware/capacity/floor/key rejection, real alternate-PSS-salt/MGF/PKCS1/wrong-domain signatures, signature mutation, RSA3070/SPKI/trailing/garbage key formats, schema/type/duplicate/depth/boundary errors. Ten shared manifest-positive cases plus three review escape variants succeed. Package-negative image changes remain correctly manifest-positive. The C test uses target `expected_manifest`, not the intentionally broader Python `expected_reference` profile.
- Review variants cover escaped envelope and manifest names, decoded ASCII target values, escaped duplicate envelope key, embedded NUL, surrogate, unsupported escape, leading-zero integer and raw UTF8. Duplicate detection operates on decoded keys. Every proper prefix of the compact valid envelope is rejected, and1000deterministic random inputs bounded to4097bytes are rejected without sanitizer failures.
- PSA failure injection covers init, hash error/length31, import error before/after an allocated key, impossible success-with-zero-handle, imported bits3070/type mismatch, attribute-read error, invalid signature, destroy error and import-error cleanup failure. It also checks absent/oversize lookup results and malformed trusted hardware/capacity/callback context. All failed transactions preserve every byte of the caller output, including padding. Imported attribute requests are asserted VOLATILE/RSA_PUBLIC_KEY/3072/VERIFY_HASH/PSS(SHA256); get-attributes objects are reset and all known acquired keys get a destroy attempt.
- Installer-hook tests return only a successfully verified typed plan, retain the old plan on failure and never change the caller's release floor. The existing installer eight-group suite also passes after compatible PSA shim declarations were extended.

## What the PSA model proves and does not prove

The shim captures the actual canonical message supplied by production C and compares every byte with Python `signing_bytes()`, including both actual embedded NULs. Exact fixture bytes select their externally computed digest; different input selects a deliberately different model digest. Mock import requires exact trusted DER and attributes. Mock verify requires the expected digest/signature/DER fixture and its externally established crypto-validity flag. This verifies that production parsing and API wiring pass the right bounded data and fail closed at modeled PSA error boundaries.

It is **not** an independent C RSA/SHA/PSS implementation or proof that a native PSA backend enforces those semantics. The model only accepts known fixtures, not every mathematically valid alternate signature. Alternate salt/MGF/domain fixtures were produced with real OpenSSL, while the C-side negative result is still a model result. PSS fixed hash-length salt semantics were checked against the local pinned SDK primary header; actual target crypto execution, task stack, timing/resource behavior and fault characteristics remain untested. The coordinator owns real SDK compile/link evidence separately.

Production still needs locally provisioned trust anchors/hardware ID, a trustworthy durable accepted-release floor, authenticated bounded transfer, real image/partition verification and boot-health/counter sequencing. Cleanup-error resource uncertainty and caller serialization remain explicit. Neither these tests nor the separate host signature reference qualify Secure Boot, eFuse antirollback, physical flash installation, production key management or vehicle use.
