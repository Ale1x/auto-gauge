# Platform persistence — real SDK partition adapter

Development compile/link project for the existing two-slot configuration store and append journal. `app_main` is empty. It performs no binding, partition-data reads, formatting, provisioning, flash writes/erases, radio or peripheral initialization.

`main/partition_io.{h,c}` supplies `ag_partition_store_bind` with a **trusted local** layout: two bounded labels, two distinct custom DATA subtypes≥0x40 and journal sector count2…512. Config size is exactly8192bytes; journal is exactly sectors×4096bytes. Binding queries default-chip capacity and enumerates registered partitions twice. It requires unique labels, exact registered descriptor identity, default flash chip, plain writable DATA, erase size/address4KiB, bounded ranges and no overlap with **any other same-chip partition**, including apps and otadata. Encrypted partitions are rejected because the current formats use arbitrary byte/bitmap programming; no unqualified read-modify-write encryption shim is invented. Layout never comes from the phone or config payload.

On success, `.config_io` and `.history_io` can be passed directly to existing cores. Binding does not create data, defaults or sessions. Config open remains NO_VALID_CONFIG on blank storage; journal open remains EMPTY, until an explicit qualified provisioning/bootstrap path is selected by the platform. The adapter never implicitly formats either partition.

All callbacks run synchronously under one non-reentrant owner. Read operations validate the retained registered partition identity. Program checks every input byte for legal1→0 transitions before the SDK write, then checks exact readback; erase accepts exactly one aligned4KiB sector and verifies every byte erased. SDK errors or mismatched readback may follow side effects and latch all further writes/erases off. Diagnostic reads remain available when their metadata/IO are still valid. There is no clear-fault or blind retry API; reconcile through explicit maintenance. Input must stay immutable and must not alias adapter/descriptor/output storage.

SDK success and readback are the software preconditions, **not physical durability, brownout, wear or power-loss qualification**. Partition descriptors and callbacks must outlive the adapter; all external writers must follow the same serialization/exclusion contract. Config/journal cores have separate recovery/READ_ONLY semantics that this adapter preserves, not repairs.

## Reproduce without a board

```sh
python3 -m unittest discover -s tests -p test_partition_io.py -v
sh firmware/platform_study/build.sh
python3 firmware/platform_study/report_build.py
```

Host test compiles production adapter and actual config/journal cores with behavioral SDK flash under ASan/UBSan. Four groups cover invalid/missing/duplicate/encrypted/read-only/wrong-geometry/overlapping/app/otadata/out-of-chip mappings; bounds/unaligned calls; actual byte-program/erase/readback; SDK failures after effects and fault latch; metadata mutation; reentry; and config/journal open plus **explicit test-only** provisioning/bootstrap/commit/append/query. This is not the SDK running on a chip.

The separate ESP-IDFv6.1 crossbuild pins SDK commit `fff9895c82d744c7237be8847347bdd1b07c6643`, checks tracked/untracked/submodule cache cleanliness and links the actual partition APIs plus config/journal public entrypoints. The report records source/artifact hashes, inspected SDK partition source hashes and empty-main disassembly. Ignored SDK files are excluded from that cleanliness check; it is not a full reproducible SDK source archive. No device is accessed by the build/report tools.

## Development layout, not V1 sizing

The4MiB table has factory+two1MiB OTA slots, cfg8KiB and journal**256KiB**. Labels are `ag_cfg_dev` and `ag_logs_dev`, custom subtypes0x40/0x41. It does **not** provide the conditional2MiB product buffer or establish final image/asset/OTA/PSRAM sizing. No encryption, Secure Boot or antirollback eFuse is provisioned.

Primary contract inspected locally: pinned SDK `components/esp_partition/include/esp_partition.h`, `partition.c` and `partition_target.c`. In that SDK, plain partition writes accept arbitrary byte lengths; encrypted writes impose16byte alignment; readonly blocks program/erase; erase requires4KiB offset/length alignment; iterator advancement releases its allocation at end. Adapter additionally verifies geometry, write legality/readback and all-partition exclusion before allowing persistence.

## Modern TWAI integration

The same isolated project now links `twai_port`, real modernSDK TWAI calls, discovery and runtime, with main still empty. The final image is223312byte. Seven behavioral testgroups pass, including completion-based response deadlines; queued does not mean transmitted. GPIO/filter/bitrate/hardware/IGN are trusted local inputs, no defaults are assigned. [Adapter contract](../../docs/TWAI_PORT.md). No physical CAN or GPIO action was executed.
