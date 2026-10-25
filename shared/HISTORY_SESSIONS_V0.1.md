# Retained history session discovery v0.1

Read-only `logs.sessions` discovers actual retained session IDs through the admitted framed endpoint. It does not choose a SID from hello, accept a supplied catalog, format or change history, acknowledge records, erase data or persist per-client enumeration state. Authentication/lease checks are the same as other private RPCs; the request is not an authorization proof.

## Request and exact success shape

```json
{"op":"logs.sessions","limit":16}
```

An optional `after_session_id` is exactly32lowercase hex digits representing a **nonzero**16byte SID. Omission starts enumeration; null, empty, allzero, uppercase, wrong length or other primitive types are rejected. Limit is an integer1…16; bool/fraction/exponent/overflow are not normalized. The schema is closed, detects duplicates after key escape decoding and rejects fields belonging to other operations. Existing operation enum values and payloads remain unchanged; LOGS_SESSIONS is appended.

```json
{"op":"logs.sessions","phase":"sessions","sessions":["01000000000000000000000000000000"],"has_more":true,"next_after_session_id":"01000000000000000000000000000000"}
```

A success contains exactly those five fields, with no simulation/device/configuration metadata added. `sessions` has0…limit nonzero SIDs, strictly increasing by unsigned byte lexical order (equivalently lowercase hex string order), without duplicates, and every SID is strictly greater than the optional cursor. If `has_more=true`, count is positive and `next_after_session_id` equals the last returned SID. Otherwise next is JSON null. A valid empty page has `sessions=[]`, `has_more=false`, next=null.

## Typed firmware boundary

`ag_history_sessions_page_t` in `shared/c/ag_rpc.h` holds at most16IDs, count, has_more and a next cursor. The internal next buffer is allzero when has_more is false; the encoder converts it to null. `ag_rpc_reply_encode` validates count≤requested limit, nonzero/strict order/cursor and next consistency before copying output, preserving caller bytes on any failure. No arbitrary JSON catalog is accepted.

`ag_device_endpoint_options_t.history_sessions` is optional, using the existing history_context and the signature:

```c
ag_device_history_result_t callback(void *context, const uint8_t *after_sid_or_null,
                                   uint8_t limit, ag_history_sessions_page_t *page);
```

The platform must supply actual retained catalog data from its backend. A missing callback gives the existing explicit `not_implemented` rejection. READ_ONLY gives the existing `history_not_operational/read_only` diagnostic; backend unavailable gives `history_unavailable`. Those errors retain the established legacy RPC error envelope, rather than masquerading as successful empty history. Invalid typed backend data fails closed instead of returning fabricated metadata. No callback is invoked for malformed/unauthorized/stale requests. Post-callback and final-output authorization/deadline checks can suppress output even after a read; callers still supply fresh monotonic time after synchronous IO as required by the endpoint contract.

Callbacks/endpoint/store remain under one serialized owner and may not reenter. The page owns all returned IDs; it retains no pointers into changing flash state. A new page request cannot cause ACK consumption or storage mutations.

## Mutable catalog semantics

Enumeration is **not a snapshot**. Each next request uses the last cursor and obtains IDs strictly greater than it from the then-current retained catalog. Newly created IDs at or before a prior cursor are discovered by the next full scan, not inserted retroactively into an in-progress scan. Retention may remove a session between catalog and `logs.read`; the read result governs actual available records. Ending a scan does not prove global absence of sessions, cloud data or earlier records. No absolute dates, record counts or continuous-time claims are inferred from SID order. SID order is an enumeration order only, not chronology.

The journal adapter and mobile archive/import flow bind this operation separately. Native trust, flash lifetime, catalog scan latency and physical radio remain independent qualification obligations.

## Verification

`python3 -m unittest discover -s tests -p test_history_sessions.py -v` compiles production RPC/parser/endpoint with ASan/UBSan. Four new groups cover strict positive/negative request forms, escaped duplicate keys, transactional outputs, all16IDs, buffer sizes, malformed typed pages, cursor ordering, missing backend, multi-page/empty catalog, no record/config mutation, stale lease, revocation inside callback, READ_ONLY/unavailable backend and final-output deadline. Five existing endpoint groups also execute unchanged. The test catalog is a trusted behavioral host fixture, not evidence that a production flash backend or radio has run.
