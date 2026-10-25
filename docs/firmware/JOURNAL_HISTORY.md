# FW10: sector journal history, development format 1

`ag_journal_history.h` / `journal_history.c` implement a separate append-only backend for synchronous **simulated flash callbacks**. This is not the COW backend, a hardware driver, the final partition table, or a qualified V1 storage implementation. Nothing starts from `app_main`; no flash, BLE, CAN or eFuse operation is enabled. Geometry is a development hypothesis: 2–512 sectors of 4096 bytes, at most 2 MiB. The approved 2 MiB policy remains conditional on OTA sizing.

## API and lifecycle

`ag_journal_open` scans without writing, erasing, defaulting, or attaching the previous boot's writer. An entirely erased medium returns `EMPTY`; only explicit `ag_journal_bootstrap` may initialize it. Nonblank media without a valid authority returns `CORRUPT`. Ambiguous retained data or short reads fail closed (`READ_ONLY`/I/O failure), with operational pages and ACKs denied. Diagnostic fields must not be promoted to a current durable snapshot after such failures.

After a healthy reopen, `ag_journal_begin_session` appends a session-start control with a **fresh trusted 128-bit SID**, preserving previous retained sessions. It rejects current/retained SIDs, including retained header evidence. The caller must guarantee freshness against fully evicted history; timestamps, crank events and reset counters do not supply that guarantee. No persistent unlimited SID registry is maintained. Initial sequence/global serial inputs are internal recovery/test inputs, not remote commands; normal initialization starts at 1.

`ag_journal_append` accepts the shared sample status and up to 64 raw bytes without truncation. PID must be nonzero; ECU is bounded to 29 bits. Valid status requires nonempty raw, other statuses require empty raw. Monotonic milliseconds cannot decrease within a session. Session sequence and global serial use `uint64_t`; the terminal value can be used once, then exhaustion prevents wrap. Session-start controls consume global serials but do not consume sample sequences. `retained_first_serial` therefore counts entries, not samples; `evicted_samples` cannot be inferred by subtracting serials.

`ag_journal_window` copies a typed window of at most 16 records, without exposing pointers into flash. `ag_journal_make_page` delegates canonical JSON to `ag_history`: sequence/time are decimal strings, raw/status are preserved, payload is at most 4096 bytes, plus terminating NUL in the caller buffer. Output failure does not expose a partial page. A cursor beyond the latest sample is rejected; a wholly evicted SID returns `NOT_RETAINED`, rather than an empty complete page. A retained empty session can return an empty page. Prefix loss is clipped to the requested cursor and session; controls and other sessions do not create fabricated PID-record losses.

ACK is stateless and nondestructive: `ag_journal_acknowledge` validates retained session/cursor state but never erases a record or stores a map of phone cursors. A's ACK cannot delete B's history. Each app persists its own cursor. Sampling decimation (at most 1 Hz/PID) belongs upstream; this backend does not schedule or decimate acquisition. Eviction counters describe previously committed samples, not acquisitions that never committed.

The caller serializes all events using one task/lock. Callbacks are trusted, synchronous, exact-length and durably complete before success; no reentry or concurrent external media mutation is supported. Runtime integrity scans detect tested faults, not arbitrary adversarial replacement. CRC and mirrored bits are not authentication or rollback-resistant storage.

## Explicit on-media encoding

All integers are little endian, written field by field; native struct layout/padding is never serialized. CRC32 uses reflected polynomial `0xedb88320`, initial/final XOR `0xffffffff`. Each sector contains:

| Offset | Bytes | Contents |
| --- | ---: | --- |
| 0 | 128 | Sector header and mutable mirrored reservations |
| 128 | 3840 | 30 entries, each 128 bytes |
| 3968 | 128 | Rollover intent and mutable mirrored future-header witness |

Header: `AGJH`, version 1, generation, first global serial, initial global serial, evicted sample count, retained first serial, SID, next session sequence, last time, initial sequence, flags, parent generation, physical index and geometry. Bytes 112–115 and 116–119 hold mirrored reservation bitmaps. Header CRC at 120 covers bytes 0–111 only; final marker `JH01` is at 124. Reserved bytes must be canonical zero. Generation/index/serial relationships are checked: each completed sector consumes exactly 30 global entries.

A reservation starts with two `0xffffffff` words. The low n bits become zero for n reserved entries (0–30), strictly a contiguous prefix. **Both 4-byte copies are programmed and read back before an entry body**. Copies must agree, and the number of committed entries must equal the reservation count. A missing reserved entry, an unreserved valid entry or mismatched bitmap is ambiguous and blocks operational use. This detects an ACKed last entry replaced by all `0xff` rather than silently rolling counters back.

Entry: `AGJE`, kind (sample=1/session-start=2), status, PID, raw length, SID at 8, global serial at 24, session sequence at 32, monotonic ms at 40, ECU at 48, raw[64] at 52. Unused raw bytes and bytes 120–123 are zero. CRC at 116 covers bytes 0–115 and 120–123. Marker `JE01` at 124 is programmed last. Session controls have zero status/PID/raw/ECU/time and carry their initial sequence. An ordinary append programs **136 bytes in four calls** (two reservation words, 124-byte body, 4-byte marker), with readbacks. Actual flash must support and qualify these repeated small 1→0 programs; this format does not establish that from simulation.

Footer: `AGJI`, version, source/next generation, next serial, victim generation and first serial, victim sample count, post-eviction count/boundary, current SID, target/source index, geometry and flags. Bytes 120–123 hold a CRC of canonical current state. Footer CRC at 116 covers bytes 0–87, 96–115 and 120–123. Final marker `JI01` at 124 commits the intent. Two mutable witness words at 88/92 are excluded from CRC; they start `0xffffffff` and become `0xfffffffe` before the future header is programmed.

## Rollover and interrupted writes

1. Validate full current sector and any known victim.
2. Commit/read back the footer intent **outside the victim**. This is the logical eviction point; queries exclude the victim even if its bytes remain physically intact.
3. Erase the exact verified old victim, if necessary, then verify the target is fully erased.
4. Reserve/read back both future-header witness words in the surviving footer.
5. Program/read back the new header with its marker last, then append normally.

One sector is reused every 30 entries. There is no capacity-wide copy per append. Even with two sectors the intent/witness remains outside the erase target. A committed intent with unreserved witnesses may resume only when its target is exactly the validated old victim or fully blank; eviction is counted once. Partial erase, partial target header, partial marker or mismatched witnesses remain fail-closed. A reserved witness without its future header also blocks recovery: an ACKed entire latest sector/header erased back to `0xff` cannot masquerade as a never-created sector. Older valid footers must witness the next retained header and agree with its state.

A torn entry can still leave the medium `READ_ONLY`, requiring explicit maintenance policy; bounded append cost does not imply automatic recovery or crank autonomy. Unknown corruption is never erased as a guessed incomplete write. A complete external media erase is indistinguishable from virgin flash and returns `EMPTY`, but never silently bootstraps. No repair protocol, bad-sector handling, ECC, wear leveling or authenticated anti-rollback is provided.

## Capacity, I/O, memory and limits

At 2 MiB, 512 × 30 = **15,360 entries including session controls**. During steady rollover there are 511 full retained sectors plus 0–30 entries in the newest sector (15,330–15,360 entries). At five PID records/second, before controls, this is about **51 minutes**, not seven hours. Raw64 and fixed 128-byte records impose substantial overhead. The earlier seven-hour arithmetic was a theoretical estimate for a smaller record model, not a retained guarantee. This format increases capacity over the COW model's 8190 entries, but is not an endurance/retention qualification or a final V1 backend.

Normal append performs a full current-sector integrity scan (header + 30 slots + footer, 32 reads), bounded independent of total capacity. Rollover additionally validates/erases one sector and writes intent/witness/header. Recovery scans the configured capacity. Session freshness checks and each query window also scan retained capacity, potentially 2 MiB. Repeated small pages across a full history can approach **O(capacity²)** total scan work. No throughput or vehicle sleep-budget claim follows from these host tests.

The store metadata is 5776 bytes on the tested host ABI, with no heap allocation and fixed arrays for at most 512 sectors. Simulation media is caller-owned and separate. Query uses a local `ag_history_snapshot_t`; `make_page` holds another snapshot while calling the window builder, then calls the shared encoder after that call returns. These are different nested paths: their frame sizes must not all be added as if concurrent. The measured window path exceeds 12 KiB plus caller/helper overhead; task-stack sizing remains mandatory before runtime integration. The default `app_main` stack is not qualified. The source compiles in the inert target; compilation does not measure live task stack, flash timing or radio coexistence.

## Verification

Host ASan/UBSan suite has eight journal groups. It covers 2573 injected I/O cuts across append/control, intent, erase and header paths (every programmed-byte boundary plus read/erase boundaries), additional bootstrap program-byte cuts, all 8192 single-bit byte positions on a two-sector populated medium, short reads, exact loss/cursors, terminal counters, raw bounds, monotonic time, controls-only rollover, SID reuse, maximum geometry, ACK independence, erased ACKed entry/sector/header, and explicit interrupted-intent resume without duplicate eviction.

Independent review additionally exercises 480 operations over four sectors against an external FIFO/control/session oracle, reopen during operation, and all six shared pagination cases (12 pages) byte-for-byte. These tests use simulated media and do not qualify physical NOR behavior, endurance, brownout, board power or production recovery.

The independent oracle and shared-page checks are retained in `tests/test_journal_golden.c` and run as `gauge_journal_golden_tests`. The typed adapter in `ag_journal_endpoint.h/journal_endpoint.c` can be supplied as `options.history_read`, with the opened journal as `history_context`. It maps degraded/unavailable/not-retained/cursor states without producing a successful stale snapshot, and never bootstraps or mutates media. `gauge_journal_endpoint_tests` verifies paging, restart, retained old/new sessions, errors and no program/erase during reads. This does not automatically connect the journal to the CLI, NimBLE runtime or a physical flash partition.

Coordinator cross-compilation with the pinned Xtensa SDK `-Og -fstack-usage` reports static frames: `ag_journal_open`1024bytes, `scan_sector`304, `check_current`304, `query`528, `ag_journal_window`6224, `ag_journal_make_page`6256, `ag_journal_endpoint_read`32. Frames overlap according to the actual call path and add along nested calls; these values are not a measured high-water mark. The combined endpoint/encoder chain and task/SDK overhead still require runtime stack qualification. Both new sources cross-compile in the inert main project (17 core +4 shared sources); neither is invoked there.
