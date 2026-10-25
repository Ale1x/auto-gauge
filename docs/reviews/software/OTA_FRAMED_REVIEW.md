# OTA framed session — independent review

24 October2026. Scope: production framed C endpoint, owner interaction, Swift packet transport and existing AG frame/stream contracts. Reviewer owns this report and independent C/Python tests only; no production edits, no flash, crypto, radio or remote operations. Required SwiftUI Pro skill and relevant Swift/concurrency, hygiene and API references read. Existing iOS17 target retained.

**Result: no concrete unresolved correctness or security defect identified within the exclusive-session, serialized-owner and trusted-callback/pipe contracts.** Source and host fault checks do not qualify a native BLE route, hardware installation or boot health.

## Source review

- C feed stages a bounded payload only. It does not call the OTA owner. A second complete frame or any trailing incomplete bytes in the completing chunk seals the endpoint before step can dispatch the first request. This includes BEGIN, whose verification/erase callback therefore cannot run from a concatenated/trailing request. Traffic in later chunks is separately subject to the one-outstanding/busy contract; no parser can infer future bytes from an earlier clean frame.
- Request kind and exact monotonic request ID are checked in staging; the ID advances before dispatch. Replay/skipped/zero IDs and CRC/magic/header failures cannot re-execute an owner callback. Oversize payload length/nonrequest kind are rejected once a header is complete, without waiting for the advertised body. Existing AG frame/stream codecs are reused unchanged; CRC and IDs are not authentication.
- Trusted lease is checked before busy, authorization, clock or owner operations. An alien lease cannot close, poll, emit or feed the current endpoint, and does not mutate endpoint/owner state. Initialization retains the existing owner without resetting it; reconnect cannot bypass the owner's one-transfer/boot state.
- Busy is latched around trusted hooks and owner operations. Clock regression, frame timeout, absolute complete-request→ACK deadline and revoked authorization seal terminally. Poll does not refresh these deadlines. ACK is generated only after post-dispatch checks and emitted only after fresh authorization/time checks. Failure may follow an already accepted write/select; output suppression is not rollback.
- Seal cancels only a same-lease active owner and latches the close attempt before calling it. Idle/faulted/selected or alien owners are not mutated. It neither resets the owner nor repeats cleanup; the existing owner preserves cleanup/selection uncertainty.
- The SDK aggregate `framed_adapter` constructs the payload verifier/installer adapter once, then initializes the framed endpoint against that same retained owner. All feed/step/output/poll/close wrappers delegate only after aggregate initialization. Local platform authorization/clock forwarding preserves the caller context and trusted lease; initialization does not dispatch crypto/flash, and close cannot reset/reinitialize the aggregate. Production use still requires the documented single static/exclusive owner; constructing multiple unrelated owners is not a permitted bypass of that contract.
- Swift transport is an actor with a closed-by-default host gate, one open attempt and one exchange across awaits. It binds nonzero generation/write limit, uses exact ID correlation and actual fragment bound min(write limit,512), and validates exactly one response with no trailing partial bytes plus a valid16byte OTA payload.
- Readiness/generation/write-limit checks and actor epoch guards surround async pipe IO. Close during ready/write/receive cannot publish late success. Concurrent exchange rejects BUSY; currentGeneration can observe the active session without a busy rejection. Cancellation/deadline/invalid response or changed readiness seals and disconnects the bound generation without retry/reopen. Timestamp bounds reject future and pre-exchange received chunks.

## Independent tests executed

`python3 -m unittest discover -s tests -p test_ota_framed_review.py -v`: **PASS**, three independent C groups with production frame, stream, framed endpoint and OTA owner compiled under C11 warnings-as-errors, ASan and UBSan. Behavioral backend callbacks only:

1. START+trailing prefix, concatenated requests and BEGIN+trailing prefix cannot dispatch owner mutation; partial-frame timeout closes before any begin/erase callback.
2. Alien feed/step/take/poll/close preserve endpoint and owner byte-for-byte and invoke no authorization/clock callbacks. Authorization reentry cannot poll/close in-flight work. Matching close performs a single cleanup, and a replacement endpoint cannot reset/re-admit the faulted owner.
3. Polling just before the pending deadline cannot extend it. Revocation after image acceptance suppresses ACK and cleans up once. Revocation after selection suppresses ACK while retaining possible boot change and never cancelling/reselecting.

`swift test --package-path mobile --filter OTAPacketTransportTests`: **8 tests passed**, zero failures. Covers closed gate/readiness bounds, fragmented responses/writes and monotonic IDs, max512 and frame deadline, wrongID/event/CRC/payload/extra-frame/trailing-prefix/timestamp/generation/write-limit failures, stalled write/receive deadlines, cancellation, concurrent BUSY/current-generation observation and close during open/ready/write/receive. Tests use a packet-pipe double, not physical BLE. Coordinator owns the separate real C-process framed interop and SDK compile/link evidence.

## Explicit limits

Platform routing must select an exclusive OTA connection locally. This endpoint and packet transport do not implement BLE authorization, local maintenance approval, key provisioning, physical send confirmation or cross-writer exclusion. Hooks and pipe operations must honor the serialized ownership/lifetime/cancellation contracts. Swift bounded completion depends on cooperative pipe cancellation; type conformance alone does not prove it. Owner polling must continue without traffic. A copied ACK frame does not prove indication delivery, physical flash durability, boot selection, boot health or rollback.

No device was flashed or connected by these tests. Production trust anchors/hardwareID/map, durable floor/health/rollback/recovery, native BLE/main routing, task/heap high-water and physical fault qualification remain separate work.

## Reviewed production source hashes

| File | SHA256 |
|---|---|
| `firmware/include/ota_framed_endpoint.h` | `4fef039013a4373c4fc225b2bc4d76457a3d437c0d6989be49e8405e846f452c` |
| `firmware/src/ota_framed_endpoint.c` | `9bc557b523f7e83abb381ac824479562adc1d22280bf7bc4544125fecb1a34ed` |
| `firmware/include/ota_transfer.h` | `76abb1b713f441c3ea3735368a90f2e9bc6d9131fb438644a66c41f10c3d832b` |
| `firmware/src/ota_transfer.c` | `769d36b64915bad0ebfec00babe5a37efbfd42361e1e185875066027e4e9a25e` |
| `firmware/ota_study/main/framed_adapter.h` | `44f3837480bc0fb3fe726baab030da7899460df26921893c105fc27e87fc1b9c` |
| `firmware/ota_study/main/framed_adapter.c` | `940259bd6024c204589945e420713ea01abaecc234d246e0f46704e61ccbb19c` |
| `shared/c/ag_frame.c` | `e2d0e2786c0f36914e15e63307e9b4f1f8218f61a7a4d572965837d5c03e1299` |
| `shared/c/ag_stream.c` | `d4afa5f352faccd2580a02eef69d6845a0f1189a3d7d093878eeeadd9b218cbc` |
| `mobile/Sources/GaugeCore/OTA/OTAPacketTransport.swift` | `33f18032cca487a43d6bf9d897832be4783f459db038456daf17dc22c58cce87` |
