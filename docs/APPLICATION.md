# Serialized firmware application owner

`ag_application` now composes the existing discovery, configuration, runtime, journal, gauge UI, RPC and boot-health cores. It is used by `firmware/idf/main/app_main.c`, replacing the previous session-ID-only skeleton. One task owns all referenced cores and adapters; ISR/radio callbacks must enqueue into that task. No public application API is thread safe or reentrant.

## Boot and profiles

The default image calls the application owner with an absent board profile and stays `BLOCKED`. It never configures GPIO, starts radio, writes flash, selects an OTA image or grants hardware qualification. A missing profile is explicit rather than silently selecting pins for an unknown ESP32.

The opt-in `CONFIG_AG_SOFTWARE_BENCH` profile uses real core algorithms with volatile simulated media and generated CAN responses. It provisions only its RAM fixtures: two configurations for RPM/road speed, a two-sector journal, a simulated stable boot record and a clean reset snapshot. All admission security callbacks deny real use. Its device ID begins `SIMULATO-`, RPC advertises simulation and volatile persistence, and boot-health confirmation is forbidden. The synthetic policy flags apply exclusively to this declared simulation, never to a physical board.

The main creates a single application task with a development 32KiB stack and runs it every100ms. Stack margin and heap/CPU behavior require later measurements. The build target/2MiB factory partition configuration remains a development crossbuild, not identification or sizing of either user's board.

## Ownership and states

The caller supplies restored config/journal/boot/admission owners, a fresh nonzero SID, known ECU candidates on one bus variant, timing policy and local hooks. Init performs no implicit storage provisioning. Unready/readonly initialized stores enter `MAINTENANCE` with all radio routes denied; explicit external provisioning/reconciliation must make them ready before `leave_exclusive` resumes discovery.

Flow: `WAIT_POWER` → `DISCOVERING` → `RUNNING`, or `CONFIG_REQUIRED` when the durable configuration cannot run against actual discovered capabilities. `CONFIG_REQUIRED` permits only the normal authenticated RPC route, so an admitted client can submit a compatible config. The scene is cleared and CAN quiesced while that correction is needed. No automatic default config is written.

Discovery uses the existing bounded page orchestrator. The local profile order is also an explicit source-binding policy: the first candidate advertising a usable PID is chosen. Every configured PID must have an actual usable source. A product profile may need a different source-selection policy; no source identity is invented from wire data.

On each tick, the owner pumps at most one adapter step, appends at most one queued log record, advances at most one RPC phase and projects a scene. It refreshes monotonic time and power after callbacks/storage work. Config store loss of ready/writable status is terminal, including errors after a commit marker may already have been written; a stale in-memory revision never licenses continued polling. Config updates rebuild validated jobs while retaining the SID, pending records, counters and per-PID1Hz budgets, including removal/readdition of a PID.

A fault clears the scene, attempts CAN quiescence at most once and submits an unavailable display scene once. Failed quiescence is never blindly retried. Physical cleanup failure is still a hardware uncertainty; software cannot prove the bus or panel state after adapter failure. No automatic reinit or retry unseals the owner.

## Adapter contract

`can_step(context,discovery,runtime)` receives exactly one non-NULL core. A physical profile must use the existing completion-aware TWAI port, recheck fresh guards at the driver, pump bounded callbacks and report terminal/ambiguous failures. A successful `can_quiesce` must guarantee no queued ISR/completion retains references to old scheduler/discovery state before reconfiguration or exclusive modes. The software bench fulfills this with synchronous generated responses and no hardware handles. `can_request_pending` suppresses core response timers only until an actual queued request completes; it must return false for Flow Control so FC cannot extend the original response deadline.

The rendering hook consumes a scene under the same owner. The software bench uses the bounded RGB565 row renderer:16rows per normal tick, finishing its immutable frame before accepting the latest scene, and a bounded complete RAM-sink redraw for off/unavailable transitions even if a terminal fault prevents later ticks. This proves software progress, not panel electrical completion. The board adapter owns actual panel protocol, offsets/byte order, brightness, touch qualification and power rails. ALS is already normalized by an explicitly supplied local policy; no invented lux curve is in the owner.

`ag_application_enter_exclusive(app,qualified_local,ota)` requires an idle connection, no pending RPC attachment or runtime log queue, successful CAN quiescence and a reconciled stable boot. It pauses acquisition and clears the scene. Returning requires known cleanup and unchanged selection; unknown cleanup or a potentially changed boot selection makes the application terminal. A real maintenance operation that resets media must restore/provision/reinitialize the external stores explicitly before allowing runtime again.

The bridge can bind to the same external admission and `app.rpc` after discovery via `ag_nimble_bridge_init_bound`; set `rpc_externally_pumped=true` so only the bridge steps that endpoint. Its mandatory route gate wraps `ag_application_route_allowed`: RPC is allowed only in RUNNING/CONFIG_REQUIRED; OTA only in the local exclusive OTA state. Separate local bridge preparation and lease authorization are still mandatory; packet contents cannot choose a route. Native adapters are not enabled by the bench main.

`ag_application_confirm_health` rejects simulation and incomplete platform evidence. The supplied mask must contain every bit required by the boot policy, without unknown bits, and current core readiness/clock/power guards must also pass. It never ORs in missing evidence. Product quantitative health, trust/floor provisioning and actual SDK boot hooks remain local profile inputs.

## Reproduce

```sh
cmake -S firmware -B /tmp/ag-application-build -DAG_SANITIZERS=ON
cmake --build /tmp/ag-application-build -j4
ctest --test-dir /tmp/ag-application-build --output-on-failure
sh firmware/idf/build.sh
sh firmware/idf/build.sh --software-bench
sh firmware/idf/build.sh --native-profile
```

The script accepts only the optional `--software-bench` or `--native-profile` switches, checks the pinned SDK revision/cleanliness and invokes build/size only. It never accesses a device. Default, bench and native use separate build/config paths. The native variant links the actual SDK adapters in the same image. Initialization runs on a32KiB bootstrap task, then the NimBLE host timer owns all application/TWAI/RPC/OTA/local-event operations on one task. It requires a complete trusted local board profile; its weak profile getter returns NULL by default. It does not derive GPIO, trust keys or local health evidence from a packet. The real native renderer completes each current scene before returning, within the required local `maximum_frame_ms` bound; the row sink must independently satisfy its qualified per-row timing. A valid interrupted reset takes a separate bootstrap-only local recovery loop with no NimBLE/CAN start, physical confirmation and qualified USB. Completion leaves erased stores for explicit commissioning; it does not reboot or provision automatically.

Application sanitizer groups exercise generated discovery → actual runtime → journal append/read/catalog → scene, local taps, denied admission/USB/CAN guards, clock/fault latches, config reconfiguration/PID budget retention, exclusive route transitions, reentry denial, commissioning state, actual framed catalog through the application's endpoint, missing health bits, unavailable scene on terminal boot error, and commit-after-effect config failure with explicit restore demonstrating the newer revision. These are host software tests, not physical flash/radio/vehicle or CloudKit qualification.
