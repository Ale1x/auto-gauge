# OTA transfer v0.1 — independent source and fault review

24 October 2026. Scope: bounded request/ACK codecs, generic target owner, real-installer adapter wiring and Swift host-development sender. No production edits by reviewer. Required SwiftUI Pro skill and relevant Swift/concurrency/hygiene references read; iOS17 target unchanged.

**Result: no concrete unresolved correctness or security defect found within the serialized-owner and trusted-callback/transport contracts.** This is not OTA runtime, physical flash or BLE qualification.

## Corrected finding

The initial SDK adapter treated only `AG_OTA_OK` as successful cancellation. The existing installer deliberately returns `AG_OTA_DENIED` after a normal cancel enters FAULT, even when its abort succeeded. This falsely reported every clean cancellation as cleanup-unknown/backend-failure. The coordinator corrected the adapter to recognize DENIED+FAULT with inactive handle and known cleanup, while preserving already-faulted/idle cleanup status and refusing selection uncertainty. Source reread confirms the mapping. Dedicated adapter regressions are managed by the coordinator; the original unsafe status mapping is no longer present.

## Source checks

- Requests use exact12byte header, known opcodes, nonzero transfer ID, zero reserved bytes, bounded body/offset arithmetic and complete consumption. ACK decoder enforces exact16byte length, enum/flag subsets and offset maxima. Codecs are serialization only and make no authentication claim.
- Owner rejects malformed/order/offset/ID requests without progress, backend calls or idle refresh. A stale trusted lease is denied before any authorization/clock/backend callback or state mutation. Payload IDs do not select the trusted lease. Fixed4096byte manifest storage is assembled before begin; no image callback runs until trusted begin returned a valid bounded/aligned verified image length.
- Authorization and monotonic total/idle deadlines are checked around callbacks, and poll does not refresh idle. Busy is latched before authorization and backend callbacks, so callback reentry cannot interleave work. A begin attempt is latched before invocation; subsequent errors cancel once. After select is attempted the uncertainty flag is permanent and cancel/retry is excluded. Terminal states do not invoke backend operations.
- SDK adapter binds the production manifest verifier to the existing installer hook using copied local trust, then routes image/finish/select/poll/cancel through the real installer APIs. Successful manifest verification does not itself assert installer success, image validity or boot health; release floor is never advanced. Main remains a separate integration boundary, not invoked by these APIs.
- Swift sender accepts VerifiedOTACandidate snapshots, preserves busy across awaits, compares transport generation before and after exchange, and validates exact semantic ACK offsets/ID/phase/selection flags. The mutation gate is closed by default. Upload stops READY; selection is a separate explicit method. Exceptions, malformed/rejected ACKs, uncertain delivery, deadline/cancellation or generation changes after a transfer attempt latch UNKNOWN; no resend, resume, implicit cancel or second select exists.

## Independent verification executed

`python3 -m unittest discover -s tests -p test_ota_transfer_review.py -v` passed. The new reviewer-owned C driver compiles production `ota_transfer.c` with C11 warnings-as-errors, ASan and UBSan and executes three independent groups:

1. Valid image chunks refresh idle but cannot extend the absolute total deadline. A begin callback crossing the total deadline is treated as possibly side-effecting and cleaned up exactly once.
2. Reentry during authorization and cancellation returns BUSY. A successful cancel that consumes the remaining deadline remains terminal without duplicate cleanup. Failed selection latches possible boot change and cannot invoke cancel/select again.
3. Ten thousand deterministic malformed payloads preserve the entire admitted owner byte-for-byte and invoke no callbacks. Alien handle/poll leases also preserve it without callbacks.

`swift test --package-path mobile --filter 'OTATransferCodecTests|OTATransferSenderTests'` independently passed **11 tests** (2codec+9sender). Shared wire fixtures roundtrip, real OpenSSL-generated verified snapshots, exact ACK checks, gate/IDs/timeouts, generation changes, actor busy/cancellation, lost/malformed ACKs and selection uncertainty are covered. This reviewer does not attribute the mocked Swift target to actual flash or RSA on ESP32.

## Limits and required platform guarantees

The owner and adapter require one serialized non-reentrant owner of all OTA/partition writers, trusted local authorization/clock/lease context and periodic polling even without traffic. Callback implementations can have side effects before returning failure; truthfulness and exclusivity are platform prerequisites. The generic owner cannot prove a supplied begin callback actually performed cryptographic verification.

Swift timeout uses structured task-group cancellation. Its bounded completion requires the transport to honor cancellation/deadline, as explicitly required by OTATransferTransport. A future non-cooperative radio implementation cannot inherit that guarantee by implementing only the method signatures. AG frame matching/reassembly and complete-response isolation remain the transport's responsibility. No native BLE route or UI enables this sender.

Behavioral fault tests execute no cryptography, flash, bootloader, radio, reset or eFuse. Existing real-crypto vectors and SDK cross-link evidence are separate. Production key provisioning/policy, authenticated transfer binding, boot health/durable-floor/rollback sequencing, USB recovery, task/heap high-water, timing and physical power-loss qualification remain open.

## Reviewed production source hashes

| File | SHA256 |
|---|---|
| `firmware/include/ota_transfer.h` | `76abb1b713f441c3ea3735368a90f2e9bc6d9131fb438644a66c41f10c3d832b` |
| `firmware/src/ota_transfer.c` | `769d36b64915bad0ebfec00babe5a37efbfd42361e1e185875066027e4e9a25e` |
| `firmware/ota_study/main/transfer_adapter.h` | `7279742eea1f0cdb0c517141ac8a8f01fd42e7f388d98289222a85def7fc9856` |
| `firmware/ota_study/main/transfer_adapter.c` | `1c66d536a4ff1ed76257e576be43fd65a244f32ed3267e7374e70f07705d1743` |
| `mobile/Sources/GaugeCore/OTA/OTATransferSender.swift` | `4b9042a24734603ff1e8124de79c4bf76551cc73a42cf051e4ac73624c8aada1` |
| `mobile/Sources/GaugeCore/OTA/OTATransferCodec.swift` | `9c2ef62c1141e63aadb3f8963348aef5b1cf658874137542e379bb36196bba14` |
| `mobile/Sources/GaugeCore/OTA/OTATransferTransport.swift` | `6d11683525716f4257b8da67f512deec1eb48655b6dc2cc0650abaf864a74be8` |
