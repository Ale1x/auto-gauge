# OTA transfer v0.1 — development contract

24 October 2026. A bounded stop-and-wait protocol over AG frames. No native BLE routing is enabled by defining this protocol. Authentication/encryption/local maintenance approval are trusted platform prerequisites, never inferred from hello or CRC. No production trust anchor, boot health, durable floor or recovery is supplied here.

## Payload bytes (little endian)

Every request payload has a12byte header: version u8=1, opcode u8, reserved u16=0, transfer_id u32 nonzero, offset u32. Opcodes:

| Opcode | Name | Offset | Body |
|---|---|---|---|
|1|start|envelope length1…4096|empty|
|2|manifest|next exact manifest offset|1…1024bytes; cannot exceed declared length|
|3|begin|0|empty; only once complete manifest received|
|4|image|next exact image offset|1…1024bytes; bounded by verified image size|
|5|finish|0|empty; only once image length complete|
|6|select|0|empty; explicit separate request after ready|
|7|cancel|0|empty; terminal fault, never reopens this owner|

One request at a time. AG frame request_id matches the response through the surrounding packet transport. The controller handles a complete payload only; frame/reassembly are separate existing components. Client generates a nonzero transfer_id and never retries any request after uncertain delivery. transfer_id and offsets are routing/ordering safeguards, not authentication. Reconnecting or changing ID does not reset a target's owner. There is no resume, status reconciliation or automatic select/reboot in v0.1.

Response transfer_id is the current accepted transfer ID (zero before start), never an echoed alien ID. Basic codecs validate syntax, enum/flag subsets and offset maxima; semantic phase/flag/ACK relations are checked by the sender/owner. Request chunk offset+body must also fit the format maxima even before state checks.

Response is exactly16bytes: version u8=1, status u8, phase u8, flags u8, transfer_id u32, next_manifest_offset u32, next_image_offset u32. Status0=ok,1=invalid,2=denied,3=expired,4=backend_failure,5=busy. Phase0=idle,1=manifest,2=writing,3=ready,4=selected,5=fault. Flag bit0=selection_may_have_changed; all other bits reserved0. Flag is set before invoking the select backend and remains latched even on callback failure/revocation/deadline. Success selected means boot selection acknowledged by backend, never installed/healthy/rebooted. Offset ACK means callback accepted bytes, never promises per-chunk physical durability.

An invalid request never calls a mutating backend. Invalid ordering, offset, size, op, reserved/version or transfer ID returns invalid with current phase/offsets; it does not advance progress or refresh idle timeout. Clients treat every non-ok response as terminal locally and send no automatic recovery request. In-flight protocol rejection is not proof of a safe retry. Failure may preserve previously accepted bytes or boot selection.

## Target owner

One serialized owner, zero initialized, initialized once per boot; no reentry/concurrent use. Fixed4096byte manifest buffer, no heap/retained packet pointers. start binds a nonzero externally provided trusted connection lease; incoming packets never supply that lease. A stale lease is denied without modifying the current owner's state or calling its backend, so a different phone cannot cancel another transfer. start allowed only in idle; successful start consumes the one transfer/boot. Trusted authorization and monotonic clock are checked before and after admitted callbacks; owner must also poll periodically even with no traffic. Both total and idle deadlines are explicit nonzero bounds (total≤15min), absolute checks at or beyond deadline expire; clock regression faults admitted state. Idle refreshed only by a valid successful progress operation.

Before start, denied authorization leaves idle reusable. After start, revocation/deadline/backend failure is terminal fault, attempts backend cancel once only if begin was attempted (never after select attempted). Cleanup result remains uncertain on failure; selection flag prevents blind retry. Reentry returns busy without touching the in-flight operation. Failure after callback may have side effects: no callback is blindly repeated. Poll invokes backend poll only after begin and before select; it checks authorization/deadlines again afterwards. Terminal states do not invoke backend operations.

begin callback receives the complete buffered envelope and returns a verified image length1…16MiB, multiple16. The callback MUST use local cryptographic verification (the real manifest verifier) and installer begin; no wire image size or remote boolean authorizes erase. Set a local 'begin attempted' latch before callback because a failed begin may already have side effects. image callbacks receive exact offsets and borrowed bytes valid only for the callback. finish verifies complete written content using the backend. select remains separate and explicit. cancel callbacks tolerate a backend already faulted and report cleanup uncertainty honestly. The generic owner is not a flash driver and cannot prove truthfulness of injected trusted callbacks.

## Swift sender

Use only VerifiedOTACandidate immutable snapshots. Preparation, upload and selection are distinct: upload sends start/manifest chunks/begin/image chunks/finish and stops in ready. Selection requires a separate explicit method; never runs automatically. Protocol dependency represents one trusted externally managed transport session and complete payload exchange; default construction must keep mutation disabled unless an explicitly named host-development gate is enabled. This increment provides no native BLE implementation or app UI to enable it.

Validate every ACK: exact length/version/known enums/reserved flags/transfer ID, expected phase, exact acknowledged offsets and selection flag semantics (false before select). An exception, malformed/rejected response, timeout/cancellation or transport generation change after delivery may leave target outcome unknown: sender latches terminal unknown, never retries/restarts or selects. Use operation deadline and generation checks around each await; actor reentry cannot interleave a second upload/select. Cancellation is checked before sending and after response. A failed select is always conservatively unknown with possible selection; no inferred rollback.

The host transport must supply a nonzero immutable generation and trusted readiness; generation is not authenticated by the wire. Do not accept 'authenticated' or trust data in OTA responses. Production integration will require platform-reviewed transport authorization instead of a host development flag.

## Verification boundary

Shared JSON fixtures encode exact valid and invalid payload bytes; C and Swift consume the same cases. Target tests use behavioral callbacks to prove ordering, lease isolation, no writes before begin verification, bounds, lost ACK behavior assumptions, revocation/timeouts/cancel/select ambiguity. Swift tests use real verified candidates from ephemeral OpenSSL vectors. Host exchange tests do not qualify BLE, flash, boot, power loss or SDK crypto. An adapter to the real installer may be compiled separately; main remains inert.

## Framed binding update

The [dedicated AG session](OTA_FRAMED_V0.1.md) now implements the framing boundary using unchanged kind1/2 codecs, request correlation, fragmentation/deadlines and terminal closure. It is tested through the actual Swift packet transport and C endpoint on a behavioral host process. A separate aggregate SDK adapter links endpoint→owner→manifest verifier→installer; main is inert. This updates the earlier payload-only milestone, without enabling native BLE routing, provisioning, health/floor or physical installation.
