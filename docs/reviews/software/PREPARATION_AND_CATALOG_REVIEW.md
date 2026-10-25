# Offline OTA preparation and retained-session catalog — independent review

24 October2026. Scope: APP-14 preparation actor/trust/archive and SwiftUI model/view, plus the journal's read-only retained-session catalog and endpoint adapter. Reviewer made no production edits. SwiftUI Pro skill and relevant Swift, data, views, hygiene, API, accessibility, localization and performance references read; existing iOS17 target unchanged.

**Result after corrections: no concrete unresolved defect found within the serialized-owner, trusted-policy and filesystem contracts.** This report does not qualify a physical OTA update, radio connection, journal flash provider or UI interaction on a real phone.

## Corrected findings

### OTAPackagePreparation.swift — invalidate before every restore attempt

Originally the missing-file early return ran before clearing a previously verified candidate. On an already prepared actor, restore with nil or a newly raised floor could return false for an absent/deleted archive while exposing the old candidate through verifiedCandidate(). The coordinator moved candidate=nil before the existence check. Selected file snapshots remain available, but any restore attempt now requires fresh verification before save/use. The public-API regression covers both missing trust and a raised current floor.

```swift
// Before
if !archiveExists { return false }
candidate = nil
// After
candidate = nil
if !archiveExists { return false }
```

### FirmwarePreparationModel.swift — drain canceled work before invalidation

The original cancel task invalidated the preparation actor independently of the previous task. With delayed scheduling, a stale cleanup could run after a newer verification; generation checks only protected publication, not mutation inside the actor. Restore work also was not retained in the work task handle. The coordinator now tracks restore, propagates its parent cancellation, and cancellation captures/cancels/awaits previous work (including previous cleanup) before invalidating. Busy remains until the current cleanup completes, with generation/cancellation checks before mutation/publication. This prevents old actor work from arriving after a newly accepted operation. The model fix was reviewed in source; there is no dedicated deterministic UI scheduler test in this report.

## Preparation data and security checks

File input must be a file URL resolving as a regular file. Security-scoped access is held for the bounded copy and released with defer. Reads request at most64KiB and stop after at most maximum+1 bytes; envelope4KiB/image16MiB limits are checked on copied bytes, independent of resource-size claims. No network/URL download path exists. The actor performs synchronous IO/crypto away from MainActor, with cancellation checks between reads and around verification. Filesystem/provider operations themselves may block; cancellation is cooperative rather than a guarantee of interrupting a stalled kernel/provider call.

Import or verification attempts invalidate the previous candidate, including failed imports; old file selections can remain visible honestly. The verifier returns the existing immutable candidate type with no public initializer/Codable fabrication route. Trust is injected locally, never read from a manifest/archive, and copies externally mutable key bytes deeply. A later upload must still obtain fresh target policy; a past offline verification cannot advance or authorize the device's floor.

The archive contains only a fixed versioned header, bounded lengths and exact envelope/image bytes. Save is an atomic sandbox file replacement, not an fsync/physical durability claim. Restore checks exact length/header/no trailing data, re-verifies all package bytes against the supplied current trust/capacity/floor and preserves rejected archive bytes. It serializes neither trust/private keys nor an authoritative verified/installed bit. Security-scoped filenames are display metadata; they do not choose trust or installation behavior.

UI model is MainActor/Observable, owns its actor/task state, and presents Italian action/error text. File selection, verification and save are separate controls; trust absence disables verify. The view uses native Form/navigation/file importer, named text+symbol buttons and Dynamic Type styles without color-only state. Installation remains unavailable in this view. No new UIKit/framework, forced geometry, icon-only action or deprecated SwiftUI API issue was identified in the reviewed files. Simulator/device build and visual interaction results remain coordinator-owned; this report does not claim VoiceOver/taps were executed.

## Journal catalog checks

`ag_journal_sessions` first validates the current journal and then scans only retained generations, validating each header/reservation count and replaying each retained entry. A bounded17SID sorted set supports a16SID page plus one lookahead, deduplicating and filtering strictly greater than the cursor. Empty sessions represented by retained control/header metadata remain discoverable; a header's old already-sampled SID is not fabricated as retained when its records have been evicted. The current empty session remains readable through the existing query semantics.

Outputs are staged locally and copied only on complete success. Invalid cursor/limit, IO or CRC/replay errors preserve caller page/count/more values. Corruption latches READ_ONLY; no program/erase/ACK is called. The endpoint helper owns its copied IDs and constructs the next cursor only for more=true. Enumeration is not a snapshot and SID lexical order is not chronology. Retention may change between catalog and data read; a new scan is needed for IDs created behind an old cursor. No absence/cloud/time claim is inferred from an empty page.

The scan is O(retained entries) reads/page with bounded stack, not an indexed journal. Real flash latency, concurrent task scheduling and hardware stack high-water remain unqualified; all writers/readers must obey the existing serialized owner contract.

## Independent verification executed

- `swift test --package-path mobile --filter 'OTAPackagePreparationTests|OTAPreparationReviewTests'`: **11 tests passed**. Existing10cases cover real OpenSSL/Security package verification, immutable file/key snapshots, archive roundtrip/current-floor/missing-trust, corruption/truncation/trailing input, bounds, filesystem error and cancellation. New reviewer case proves absent archive cannot retain a candidate under missing/new trust. No fake verified candidate was introduced.
- `python3 -m unittest discover -s tests -p test_journal_catalog_review.py -v`: **1Python test /3C groups passed** with C11 warnings-as-errors, ASan/UBSan. It creates200sessions, performs real simulated journal rollover/eviction, and independently compares every catalog result with the existing per-session query oracle. Includes retained empty-control sessions, pages of7, strict ordering/no duplicates, newly later SID, no program/erase, a CRC fault in an older noncurrent retained entry, READ_ONLY latch and transactional buffers. Flash IO is simulated, not SDK/hardware.

Priority after these fixes: preserve fresh trust at target admission and serialized catalog access during real integration; no additional correction is requested by this review.

## Reviewed source hashes

| File | SHA256 |
|---|---|
| `mobile/Sources/GaugeCore/OTA/OTAPackagePreparation.swift` | `45a6ba9eb66e072e9444d139b11a7a28831ca44996dbb8a920ebbc368e3579c1` |
| `mobile/Sources/GaugeCore/OTA/OTAPreparationTrust.swift` | `68ee97932e1118a16955e9e8e441e9eeca0e5f6e51b18a87ad9c7b037bd711e2` |
| `mobile/Sources/GaugeCore/OTA/OTAPreparationSnapshot.swift` | `f8fc761f26fb034ee03ca07c93427c0ddbd3846c8d84f0c3ac4a54037f5fdb33` |
| `mobile/App/Features/OTA/FirmwarePreparationModel.swift` | `ccd7fcac3a0767a9c860903a30695bb9a3fdc02c5698fd27b04e7f31a382d4bb` |
| `mobile/App/Features/OTA/FirmwarePreparationView.swift` | `258ecbb8f400f2615e312a7dac0eea5847b8ace0c074fb704f36a62c681570b4` |
| `mobile/App/Features/OTA/FirmwarePackageFilesSection.swift` | `857cb1adfe50160504275e27466d5207913b188743aa7da1a1c795869b2901b9` |
| `mobile/App/Features/OTA/FirmwareVerifiedPackageSection.swift` | `b2aeefd6b7479a18499390a83d91941fa37e6924f7fb6adb05f15f2ad2a198d2` |
| `firmware/include/ag_journal_history.h` | `f8d5492da655ccd98c75a5d171316604705603ed8e5365de104e6e010707db25` |
| `firmware/src/journal_history.c` | `42086302b1a162de7f47b67393c104de3bfb918d13c6da6e6874ebb65b1356f2` |
| `firmware/include/ag_journal_endpoint.h` | `9b69ceb72d9ebf8340e7d4251b9a86cab076ac516abaa43587dca2ae1635c846` |
| `firmware/src/journal_endpoint.c` | `db9cbd79743f8ed3b4725f03ea97b348acacf5021d49631801a7d97649a88ce9` |
