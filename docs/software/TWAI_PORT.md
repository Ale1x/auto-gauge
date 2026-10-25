# Modern TWAI SDK adapter — development boundary

25October2026. `firmware/platform_study/main/twai_port.{h,c}` uses pinned ESP-IDF6.1 `esp_twai`/`esp_twai_onchip` APIs. No legacy driver, raw-transmit public API, default pins, bitrate auto-detection, Wi-Fi or physical bus action is supplied by the build.

Init prepares a static16event queue only. Explicit start requires a qualified local GPIO/bitrate/filter profile and fresh CAN power/IGN/rail/hardware guards; it allocates/enables a real node IF called. The study main never calls it. Support is classic CAN11/29bit at250/500kbit/s with RTR/FD rejected. Only the existing discovery/runtime Service01 builders and their generated FlowControl can transmit. Policy is checked again immediately before actual SDKenqueue and afterwards. Hardware automatic retry is disabled; no automatic recover/restart API exists.

ESP_OK from transmit means QUEUED. ISR callbacks only copy bounded TX/RX/error/bus-off events into the static queue; business logic, policy, storage and quality processing remain on the serialized owner. Actual TXcallback success changes COMPLETE. A coordinator correction starts response deadlines at that completion observation, instead of enqueue; FC completion does not renew the original response deadline. The observation is a software callback timestamp, not a qualified physical-wire timestamp. Late/buffered/early RX, STmin and timer behavior still require target timing tests.

Failure, revocation, timeout, bus-off, queueoverflow, clockregression or unknown enqueue result seals the port, clears pending core request/FC/RX and disables at most once. Close deletes a known inactive node at most once; failed disable/delete remains cleanup_unknown. The static/internal-DRAM object must outlive every callback, including failures; no reinit is supported. A retained allocation after failed start must be released by the explicit close path, never blindly retried.

`tests/test_twai_port.py` passes7groups with actual discovery/runtime/scheduler/ISO-TP and behavioral SDK/timer/queue under ASan/UBSan: four CANvariants, queue-versus-completion, reentry/guard revocation, ambiguous enqueue, errors/timeouts/bus-off, RX/IRQ bounds, FC cleanup and delayed completion greater than response timeout but within TXtimeout. The real SDK project separately compiles/links all adapter entrypoints; neither result is an actual CAN transmission or GPIO/transceiver/IGN qualification.

Still required: application-owner wiring, selected board/pins/transceiver, local hardware policy, ISR/cache/task/heap high-water and real timing/traffic/fault tests. No vehicle connection or board programming was performed.

## Idle quiescence and suspend/resume — V0.13

`ag_twai_port_quiesce` refuses queued TX with BUSY and no state change. After TX completion it cancels core response/FC state, detaches core pointers and discards bounded old RX events; unexpected TX/error/bus-off or clock regression remains terminal. The node stays enabled until the caller explicitly suspends it. ISR callbacks reference only the static port and queue, not the detached application cores.

`ag_twai_port_suspend` first quiesces, then calls SDK disable once and enters SUSPENDED. `ag_twai_port_resume` requires that exact healthy suspended state, fresh qualified ON policy and no unresolved cleanup before SDK enable. This is explicit lifecycle control, never bus-off or fault recovery. Failed or ambiguous disable/enable operations latch terminal state and are not retried. A successful resume starts a new disable epoch; closing a suspended node deletes it without trying to disable it again.

Pinned SDK `components/esp_driver_twai/esp_twai_onchip.c` was inspected: `_node_disable` disables the interrupt, stops HAL and releases the PM lock; `_node_enable` starts HAL/enables the interrupt and may continue an outstanding transaction. Therefore the no-pending-TX condition is material. Disabling this SDK node does not prove external transceiver standby, total current, ISR timing or board power behavior; those remain trusted board-profile actions and physical tests.

Nine sanitizer groups now cover original driver behavior plus idle detach/rebind, BUSY without mutation during queued TX, healthy suspend/resume cycles, denied OFF resume and terminal SDK failures without repeated enable/disable. No SDK recover call is used.
