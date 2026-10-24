# OTA03 installer study — independent review and host fault injection

Date: 2026-10-24. Status: SOURCE_AND_HOST_FAILURE_PATHS_REVIEWED; NOT_TARGET_INSTALLATION_QUALIFIED.

Scope: `firmware/ota_study/main/ota_installer.{h,c}` plus the pinned SDK contract. Reviewer wrote only new installer test/shim/Python files and this report; production C and its CMake are coordinator-owned. No SDK build, radio, physical partition/flash write, reboot, eFuse, Secure Boot provisioning or installation was executed by this review.

## Result and corrected defect

No unresolved concrete P0/P1 defect identified in the final reviewed source under its zero-initialized static object, init-once/boot, serialized sole OTA-writer and trusted-hook contract. This verdict concerns a compile study with host doubles, not a usable production OTA service.

The firmware agent identified an SDK-specific resource bug, independently confirmed here in local ESP-IDF v6.1 commit `fff9895c82d744c7237be8847347bdd1b07c6643`, `components/app_update/esp_ota_ops.c:195–219`: `esp_ota_begin()` allocates the operation and assigns `*out_handle` BEFORE erase, then returns an erase error without freeing the entry. The original installer marked a handle active only after success and would miss cleanup. The coordinator fixed this with a zero sentinel and handle tracking on either return; a nonzero allocated handle is aborted on begin failure, and failed abort latches `cleanup_unknown`. The sentinel assumption is tied to this pinned SDK and the exclusive bounded installation lifecycle, not a generic all-SDK guarantee. Host regressions distinguish pre-allocation failure, post-allocation error and post-allocation error with failed abort. All pass; no automatic retry follows.

Before test freeze, the coordinator also added fresh guard checks after partition validation and before SDK write/end, canonical version padding, and explicit PSA cleanup uncertainty. Those paths were reread and exercised. Manifest authorization is a mandatory trusted callback; this study does not implement target signature verification/provisioning or make a wire boolean authoritative.

## Reproducible harness

Run from repository root:

```sh
python3 -m unittest discover -s tests -p test_ota_installer.py -v
```

The Python test compiles the **unchanged production** `ota_installer.c` as a separate translation unit with `firmware/ota_study/tests/installer_driver.c`, C11 warnings as errors, AddressSanitizer and UndefinedBehaviorSanitizer. It uses temporary build files, no production/SDK CMake. One Python test/eight C groups pass.

`tests/shim/` provides minimal behavioral doubles for `esp_partition`, `esp_ota_ops`, PSA and sdkconfig. These declarations/constants are not SDK ABI evidence, controller emulation or physical flash behavior. The coordinator's real pinned-SDK compile/link study remains the separate declaration/API check.

**Hash double is deliberately NONCRYPTOGRAPHIC.** It accumulates the bounded bytes passed to hash-update and compares them with a known2080-byte fixture pattern. Exact fixture bytes produce a fixed32-byte model digest; different bytes or injected errors produce another digest. The model digest is independent of the manifest's expected digest, so incorrect expected hashes are rejected too. This tests reread→update→finish→compare control/data flow only. It is not SHA256, collision resistance, PSA correctness, manifest-signature verification or Secure Boot evidence. No cryptographic implementation is vendored.

## Coverage and observations

1. **Successful staged lifecycle.** The2080-byte fixture uses chunks1024/1024/32. Finish consumes the SDK handle, performs three partition rereads and checks model digest/version/secure-version without explicit boot selection. Select performs another complete reread/hash, then one boot-selection attempt. A second selection is rejected. The initial running and boot partition must match a validated OTA slot; factory/first-install provisioning is not supplied.
2. **Pre-erase authorization/plan checks.** Denied maintenance, denied verifier, revoked/expired authorization during verification, empty/oversized/nonaligned image size and malformed version/padding fail before an SDK begin. Missing mandatory hooks and bad timeout configuration fail init; repeated init cannot clear live state.
3. **Partition and state checks.** Tests reject running/boot mismatch, pending/new state, state-read error, overlapping slots/otadata, wrong otadata size, unaligned extents, missing/unverified partitions, wrong next-update target and insufficient capacity. Changing boot partition before a later write aborts the current handle without writing. The shim models fields used here, not the whole physical partition table or external flash chip properties.
4. **Chunk/lease boundaries.** Wrong offset, UINT32_MAX offset, zero/null chunk,1025/SIZE_MAX length, crossing remaining image size and incomplete finish fail without out-of-bounds access. Non-full1024-byte final chunks are supported. Stale leases cannot abort/fault the current writing/ready transaction, advance time, perform IO or select boot. Phase-invalid operations remain errors, not recovery/reset commands.
5. **SDK failure/cleanup.** Begin before/after allocation, write that may mutate before error, image-end failure that consumes its handle, failed abort and cancel are injected. Failed cleanup is explicit, and the runtime cannot silently reuse a consumed/unknown handle. SDK write/end error is not presented as a definite rollback or proof that target bytes are unchanged.
6. **Read/hash/descriptor errors.** Crypto-init/setup/update/finish/abort failures, digest length31, wrong digest, wrong manifest-expected digest, read error, corrupted reread, malformed descriptor version and secure version below trusted floor reject the image. A deliberately defective mock read that fills only part while returning success also fails model integrity; real `esp_partition_read()` does not expose a byte-count API, so the normal contract is full success or error. PSA abort is attempted on setup failure; abort failure sets cleanup uncertainty.
7. **Time/policy/reentry.** Clock regression and100ms idle deadline abort a live operation. Eleven valid writes spaced90ms apart still expire at the absolute1000ms total deadline. Slow/revoked begin, write, state read, end, reread and descriptor IO are checked before/after the relevant boundary, including fresh authorization before write/end. A trusted callback attempting all six reentrant begin/write/finish/select/poll/cancel operations gets BUSY without corrupting current progress. Owner serialization remains mandatory; this is not a thread race test.
8. **Selection ambiguity.** Target bytes changed after finish fail the mandatory second hash before selection. Boot mismatch before select denies selection. A mock SDK selection that changes boot and returns error, a successful selection followed by authorization revocation, and selection crossing timeout all leave `selection_may_have_changed=true`, terminal fault and no second selection attempt. Cancel does not promise to undo selection. The model explicitly permits actual selection despite an error return.

## Important limits

SDK `esp_ota_begin()` can erase target bytes and invalidate inactive otadata metadata before explicit select. The harness's explicit-select counter and `selection_may_have_changed` flag do **not** prove all boot metadata is unchanged before selection. No rollback/health/floor advancement or reboot is implemented; a future installer must reconcile actual otadata and boot state. There is no automatic first-boot/factory conversion, alternate release-key provisioning or recovery after terminal installer fault.

Partition pointers and physical policy remain trusted SDK/platform inputs. Fresh checks and postchecks cannot make revocation atomic with an already-running physical SDK operation. Error/timeout after IO means bytes or selection may already have changed. A new production adapter must serialize all writers, recheck actual installed bytes with the SDK, apply the real signature/eFuse/partition policy and implement qualified boot health and durable replay-floor sequencing. The Python host package verifier and immutable-image preflight are separate components; this C callback does not inherit their cryptographic validation automatically.

The fixed input/chunk bounds and sanitizer checks establish host memory/control safety for exercised cases. They do not establish flash atomicity, encryption block behavior, task stack/high-water, watchdog/scheduling, power-loss/crank recovery, rollback efficacy, authentication keys, antirollback or vehicle standby. Study app_main remains inert; invoking these APIs on a real configured target WOULD erase/write/select flash and is outside this test execution.

## Follow-up: denial before admission preserves the idle installer

Reread the coordinator's final `begin()` precheck and independently reran `test_ota_installer.py`: one Python/eight C groups pass with ASan/UBSan. Maintenance authorization is now checked before binding the supplied lease or observing/starting its clock. A denied caller returns DENIED while leaving phase IDLE, lease zero, verifier/SDK begin untouched and clock-call count zero. The regression then enables trusted authorization and successfully begins the installation. This closes an availability defect in the earlier order, where an unauthenticated attempt could consume the one-installation boot slot by latching FAULT. Once admitted, subsequent authorization/verification/IO failures retain the existing terminal-fault semantics.

No additional concrete defect was identified in this small source change. The coordinator reports a final successful real SDK compile/link image of196688bytes; this reviewer did not run an SDK build. All prior noncrypto/ABI/physical-installation limits remain.
