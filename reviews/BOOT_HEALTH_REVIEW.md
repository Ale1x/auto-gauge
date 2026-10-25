# Boot health and durable floor — independent review

24 October2026. Review scope: production boot-health core, NVS/SDK binding and mandatory pre-selection integration hook. No production edits by reviewer, no device execution, flash, eFuse, reboot or remote operation. The report describes source and behavioral-host evidence, not physical crash consistency.

**Result after correction: no concrete unresolved defect found within the atomic-blob, exclusive-owner and trusted-health contracts.** Factory-to-OTA first provisioning remains a separate platform integration task; the current binding does not silently bootstrap a factory image.

## Findings corrected by the coordinator

1. The first record format persisted only pending==2 as a generic health witness. After a mark-valid operation that changed SDK state but returned failure, reopening with a stronger required-health mask could advance the floor without evidence for the additional bits. The corrected record persists the confirmed mask in bytes72…75. Shape validation binds it to pending state; reconciliation requires the witness cover the current required mask. Confirm always persists the fresh supplied mask before any SDK mark, including a retry after a fresh open. Independent policy15→255 regression now passes and floor remains unadvanced for missing evidence.
2. `nvs_open_from_partition(...READWRITE...)` creates an absent namespace. The original bind could therefore write namespace metadata while appearing to only bind existing storage. The corrected bind first probes READONLY, closes the handle, then opens READWRITE under an exclusive namespace-writer contract. Missing namespace fails without creation. `ag_ota_boot_storage_provision_namespace` is an explicitly separate side-effecting maintenance operation and does not create a floor record.

## Ordering and reconciliation checked

A signed/verified target is staged as a durable intent containing counter, address, length and digest **before SDK selection**. The core rechecks the inactive target hash through its trusted target hook. The payload adapter requires a prepare-selection hook; null or failure prevents `ag_ota_installer_select`. Lease/auth/hash checks remain the installer's responsibility around actual selection. A failure after intent write may have committed that intent and is never treated as rollback or a safe repeat.

On trial boot, running/boot addresses and SDK state are observed; the candidate bytes are actually hashed by the SDK binding. Health must be supplied locally, cover the required mask and use known bits. The order is persisted health mask → SDK mark-valid → fresh SDKVALID/address/digest observation → durable accepted-floor replacement. SDKVALID with only the original selected intent is rejected as unexpected. Digest or address mismatch cannot become accepted health. Provisioning requires an empty record and a stable VALID running==boot observation, through an explicit platform action.

A storage or SDK failure after effects faults the current core; repeated confirmation does not repeat the write/mark. Fresh open reads the durable record. A matching running candidate that is SDKVALID and has sufficient persisted health evidence can finish the floor transaction without repeating mark-valid. A stable VALID fallback running elsewhere clears the pending candidate while preserving the old floor. Selected-but-not-rebooted state retains the intent. The record does not assert physical antirollback or prevent malicious storage rollback; eFuse/SecureBoot/provisioning remain outside this software mechanism.

## Pinned SDK source inspected

ESP-IDF commit `fff9895c82d744c7237be8847347bdd1b07c6643`, local primary implementation:

- `components/nvs_flash/src/nvs_api.cpp:nvs_commit` and `nvs_handle_simple.cpp:NVSHandleSimple::commit`: commit currently validates the handle and returns success; persistence occurs in the set path, so commit is not a separate physical transaction barrier.
- `nvs_storage.cpp:Storage::writeItem/writeMultiPageBlob`: a replacement uses alternate blob chunk versions, writes all data then a BLOB_IDX, and removes the previous version afterward. Removal can fail with `ESP_ERR_NVS_REMOVE_FAILED` after a new value exists. Startup duplicate/index/orphan recovery is implemented in that library. This supports treating false as possibly committed; it is not a physical power-loss proof for every flash/provider failure.
- `nvs_partition_manager.cpp:NVSPartitionManager::open_handle`: writable mode allows `createOrOpenNamespace` to create metadata; read-only mode does not. The corrected bind therefore requires an existing namespace. Concurrent deletion/replacement between the two opens is excluded by the documented single-owner contract.
- `components/app_update/esp_ota_ops.c:esp_ota_current_ota_is_workable/rewrite_ota_seq`: marking VALID rewrites active otadata using erase then write. Errors can be ambiguous. This API does not reboot on valid=true, and factory lacks the OTA state used by the binding. Hardware antirollback would be a separate operation; the study disables it.

## Independent verification executed

`python3 -m unittest discover -s tests -p 'test_boot*review.py' -v`: **2Python tests /6C groups passed** under C11 warnings-as-errors, ASan and UBSan.

- `test_boot_health_review.c` uses an independent atomic-blob/SDK-state behavioral model with production `boot_health.c`. It tests failed health/floor replacement before/after commitment, no repeated mark during the faulted boot, reopen/fallback ordering, policy-mask change15→255, unexpected VALID/digest mismatch and reentry from every trusted hook. Floor10 writes are asserted to occur only after an actual modeled SDKVALID transition.
- `boot_storage_review_driver.c` links production `boot_storage.c` and core through a separate SDK shim. It verifies namespace absence bind has no writes, explicit namespace provisioning leaves the record empty, chunked reads account for the exact image length, hash mismatch/abort rejection, record-length/state errors and actual write/mark ordering `W,W,W,M,W`. NVS set/commit errors are injected after their model has committed bytes; SDK mark can similarly change VALID before failing. Reopen/reconcile preserves the intended behavior.

The second harness's hash is **NONCRYPTOGRAPHIC** and NVS/partition/SDK APIs are behavioral doubles. It proves wiring, byte coverage and ordering, not RSA/SHA mathematics, real NVS recovery or ESP32 execution. Separate real SDK compile/link and native cryptographic evidence are owned by the coordinator.

## Limits and integration requirements

The application must provide truthful local health checks, one retained boot-state owner and serialized/exclusive NVS and OTA writers; health is never a remote boolean. Existing NVS initialization, namespace/floor provisioning, partition geometry and trust anchors must be qualified explicitly. Failed writes/marks require fresh-state reconciliation, not retry on the same object. A generic witness mask does not measure peripherals, stack/heap, display, storage durability, physical timing or CAN safety itself.

The stable record intentionally stores a floor rather than a permanent cryptographic identity for the running firmware; it is a package admission policy, not hardware rollback resistance. At the initial review snapshot the binding required SDKVALID OTA provisioning; the coordinator follow-up below adds a separately authorized factory profile. Bootloader fallback, NVS power-loss behavior, flash brownout/wear and provisioned security still require real board tests. No target or product completion is inferred from these host results.

## Reviewed production source hashes

| File | SHA256 |
|---|---|
| `firmware/include/ag_boot_health.h` | `ec923b7c208cd260d22eba4f772148f4620fc002453e618e8222b6814964c49d` |
| `firmware/src/boot_health.c` | `e68bc231cc54c219aa86d6dd1774622dc56f3dc16bc2056c5402403408a8696d` |
| `firmware/ota_study/main/boot_storage.h` | `3740ef0022558436242a97dd3c47839dfcd24b131c064b1157af16095d71f29a` |
| `firmware/ota_study/main/boot_storage.c` | `5023505df9c5b0c90da465a9a90dd07fb61bc836f801a8f55ab543a1410c530c` |
| `firmware/ota_study/main/transfer_adapter.h` | `e1c3318abdca52033776a832b472ab05d86650f8a636f1aabf5e243644e56330` |
| `firmware/ota_study/main/transfer_adapter.c` | `cd358c0a5794457f10e597e4ea65b2d2ae38dcf8fc7b2ba2b1c08f837b3c9b60` |

## Coordinator follow-up —25October2026

After the independent snapshot above, the coordinator completed the explicit factory path and reran the independent harnesses:2Python/7Cgroups pass (3core+4binding). The core's own suite passes8groups, including final floor-write ambiguity and factory trial/fallback. `test_ota_factory_bootstrap.py` also passes the actual installer with behavioral SDK/hash plus its8baseline groups. These extensions were source-reviewed by the coordinator; they are not represented as a second independent full review.

Both boot-core and installer/platform profiles default `allow_factory_bootstrap` tofalse. Explicit trusted local maintenance can provision a baseline on a registered runningfactory image and install a verified candidate into OTA0, preserving the factory as fallback. Geometry/overlap, local admission, image verification and durable prepare-selection remain required; no wire flag enables this mode. A new OTA trial still requires fresh health witness and SDKVALID before floor advance. Pinned SDK get-next-update iterates OTA subtypes fromOTA0 and get-state does not provide factory state; the binding reports an explicit FACTORY observation instead. The factory path was not physically executed.

The optional runtime degraded-logging policy was also checked: defaultstrict behavior is unchanged, and opt-in storage failure disables further recording without claiming persistence or bypassing CAN/clock guards. Baseline6runtime groups plus degraded cases pass; no recovery/retry writes are added.

Final coordinator-reviewed hashes (the earlier table remains a historical independent snapshot):

| File | SHA256 |
|---|---|
| `firmware/include/ag_boot_health.h` | `efd1366c9b6047248d64824a1ea51af536f54568ae85a55d2bd21ebc8ed469bc` |
| `firmware/src/boot_health.c` | `e2103ff361ca2513a190e32d43d26fb022805199d0856a0e9884e854c4e25e4e` |
| `firmware/ota_study/main/boot_storage.h` | `3740ef0022558436242a97dd3c47839dfcd24b131c064b1157af16095d71f29a` |
| `firmware/ota_study/main/boot_storage.c` | `d716cb5c0a60759467b6f654f0e73ea344575ce434f1f7274ad83a6938c2fb27` |
| `firmware/ota_study/main/transfer_adapter.h` | `bc13eb05f8102d13a128e72172f9aaf6bfa2dd38ae1369d37357376f4baee05c` |
| `firmware/ota_study/main/transfer_adapter.c` | `4166852b62f6235bb080b6b2234414a2b87117a522430302abb00157390e4542` |
| `firmware/ota_study/main/ota_installer.h` | `0a950a11428b9f3e4167330a58adcff641f5b7c064f0c0d3f8f91280074ff7d0` |
| `firmware/ota_study/main/ota_installer.c` | `938511a51908038b9e9ea84bfca31a32a6cd7f1ff5d57c86763acefee7a793b0` |
| `firmware/src/runtime.c` | `b37d56110456db3862a024cb20f826bb5f140dfcea70695c9e4e1e126f358b35` |
| `firmware/include/ag_runtime.h` | `54e54e43fe5b1957275b0dfe074ddc63f7465a4961634f9c95e9d24924113140` |
