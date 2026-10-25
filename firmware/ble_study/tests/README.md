# Host lifecycle regression harness

Run from the repository root:

```sh
python3 -m unittest discover -s tests -p test_nimble_lifecycle.py -v
```

This compiles **the actual, unmodified** `../main/nimble_transport.c` in the driver's translation unit with C11 warnings as errors, AddressSanitizer and UndefinedBehaviorSanitizer. It requires a host C compiler supporting these sanitizers. The executable lives in a temporary directory. No ESP-IDF build, hardware, SDK runtime, radio, NVS, transport connection or task is launched.

`shim/` consists of minimal behavioral SDK doubles and forwarding headers. It supplies only the declarations/fields used by this adapter. These types and error constants do **not** establish the ESP-IDF ABI, FreeRTOS scheduling, ATT/SMP implementation or controller timing. The pinned real SDK compile/link study separately verifies the production declarations. No third-party implementation is vendored here.

The driver includes production C so it can observe static state and inject callbacks. `boot()` clears the singleton to represent a new process/device boot between independent cases; it is not a production restart API. Task creation records and validates parameters without running a task. Calling `host_task()` explicitly exercises only the adapter's handling of unexpected return from a mocked host loop. Hook context is static, and tests honor the adapter's owner/reentry restrictions. Trusted security evidence is modeled input, not a cryptographic proof.

Thirteen transport groups exercise:

1. Task OOM returns NO_MEM, callout stop/deinit and SDK cleanup, including cleanup failure; fault blocks another start. Also initial SDK/GATT/callout failures and mandatory fault hook.
2. Termination errors and EALREADY get at most three attempts, report fault once, and fail closed. Missing descriptor and ENOTCONN after descriptor disappearance clear local slot. TX failure and logical disconnect callbacks occur once.
3. Successful termination is not resent, waits for disconnect, faults at 30 seconds if the descriptor stays, and advertises again only after a clean disconnect.
4. Timer reset and asynchronous address failures fault. Timer failure stops advertising and prevents further private RX/TX.
5. Old generation events cannot acknowledge or disconnect a new link with the same handle, or unsubscribe it. Generation UINT32_MAX does not wrap or admit a new link.
6. Full 4112-byte frames at MTUs 23, 515 and 517 reproduce bytes and respect MTU−3/512. A synchronous status-0 callback does not advance, EDONE advances one chunk, concurrent frame submission fails, final completion occurs once and the buffer is wiped. MTU3 and frame lengths15/4113 are rejected.
7. Trusted admission denial, every required security field, identity mismatch, missing CCC subscription, RX lengths0/512/513 and authorization revoked between chunks. Unsubscribing during TX fails that frame.
8. Mbuf failure, synchronous indication failure, later indication failure, 10-second absolute frame timeout despite partial progress, and 30-second unauthenticated-link timeout.
9. Unexpected host-loop exit, reset and store pressure fault without allowing restart or bond eviction.
10. Pre-encryption bond lookup: known authenticated SC/128-bit retained key, ENOENT→unknown, other storage errors→failed, invalid authentication/SC/key length/LTK/identity and invalid/stale/faulted connection rejected. Local SDK key-value wiping is reviewed in production source; the test does not read a dead stack frame to pretend to verify erasure. The shim does not contain real key material.
11. A successful CONNECT rejected before logical admission is still tracked for bounded teardown. Policy change after advertising with terminate error gets three attempts and one explicit fault, never private RX/TX or logical connect/disconnect/display hooks. Stale disconnect callbacks do not clear a rejected link while its SDK descriptor exists. Successful termination waits once until descriptor disappearance or the30-second failure deadline. ENOTCONN cleanup, generation exhaustion without wrap/re-advertising, and rejected connections while faulted are covered.
12. SDK advertising fields/start failure during initial sync and reconnect produces an explicit latched fault, while a policy-denied call is legitimately false without SDK work/fault. Successful stop obeys policy; failed stop keeps the mock controller advertising, reports a fault and does not enter a tight retry loop or claim the radio stopped. A timer fault plus failed stop retains the original timer-fault report.

`bridge_driver.c` reuses these SDK doubles and includes the same transport, while **linking unmodified `endpoint_bridge.c` and real core implementations separately**: connection, admission, config validator/store, history codec, device endpoint, JSON/RPC/frame/stream. Only physical storage is the existing flash simulator; history reads explicitly report unavailable in this fixture, so no history retrieval claim is made.

Eleven bridge/core groups additionally establish:

1. An unknown peer without a local window is closed. With a local window, the actual admission RNG path produces `000042`, UI rendering succeeds before simulated SMP injection, verified SC evidence attaches the endpoint, and a framed authenticated hello is delivered with the original request ID.
2. Config apply emits `received` with request ID91 and performs zero flash programs while that frame awaits final EDONE, including intervening status0 callbacks/ticks. Only the next tick after all chunks are acknowledged commits revision2, whose applied response keeps ID91. Readback and simulated reboot/restore recover the canonical record. The fixture labels persistence **volatile**, because a RAM flash model is not physical durability evidence.
3. Disconnect and awake-policy denial before confirmation cancel uncommitted work. Sequential A→B→A obtains fresh leases/generations and permits request ID1 again per link; delayed A callbacks cannot acknowledge/disconnect B or reveal its state.
4. Missing entropy or failed local rendering closes the link without any SMP passkey injection; no fallback is introduced. Required platform RNG/UI callbacks cannot be omitted.
5. A simulated 40-second delay inside program IO can leave a committed revision2, but fresh post-IO time rejects the expired response and closes the link without a stale applied frame. The retained record must be re-read after a new qualified connection.

No mock models power loss, actual bond-key storage, admission-window authorization, SMP methods, hardware IRQ/event races, task OOM in the real allocator, radio behavior or end-to-end iPhone persistence. The timer mock is driven explicitly; the SDK's scheduling and real timeout accuracy remain untested. There is no assertion that the production adapter is qualified for activation.


Four OTA bridge groups additionally use the real framed endpoint and transfer
owner, with explicit behavioral installer hooks (no cryptographic substitution
claim):

6. Only local idle arming selects OTA; JSON hello in OTA cannot attach RPC or
   mutate configuration, binary OTA in RPC never reaches the transfer owner,
   and a live link cannot switch route. Request IDs remain strictly sequential.
7. A request staged while the previous indication awaits final EDONE cannot
   execute yet. Local policy revocation closes the link, cancels exactly once,
   reports closure, discards staged finish and isolates a later peer from old
   confirmation callbacks.
8. Failed cancellation is reported as cleanup UNKNOWN and blocks RPC rearm;
   failed select retains possible boot-selection change and prevents cancellation
   or fallback to RPC. No retry erases evidence of side effects.
9. Missing local OTA permission closes after admission without calling installer;
   simulated slow begin beyond deadline invokes bounded cleanup and emits no
   stale ACK. The application route callback is mandatory and its revocation
   also blocks ordinary RPC received→commit progression.

The bridge requires real local application-mode and maintenance authorization;
the test callbacks are fixture controls, never production trust provisioning.


10. Binding to external application admission/RPC preserves the exact objects,
    installs actual NimBLE trusted security hooks while idle and rejects aliasing
    the bridge's own storage. With the application configured not to pump RPC,
    repeated bridge ticks cannot commit before final indication EDONE. Disconnect
    invalidates the same external admission and endpoint, not a duplicate copy.


13. Transport permanent local quiesce denies private IO/advertising, stops active
    advertising, waits for actual disconnect callback rather than terminate
    acceptance, rejects unsolicited connects, and never reports quiet after a
    failed advertising stop.

11. Bridge deferred startup binds zero RPC storage while admission is initialized.
    Owner ticks execute idle, initialize RPC on the same admission, then permit
    advertising; false owner result faults once and closes the existing link.
    Subsequent timer callbacks do not invoke the faulted application again.
