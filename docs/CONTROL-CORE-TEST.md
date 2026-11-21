# Synthetic control-core test — Preview 2026.11.22-18

Local diagnostic only; no public publication or flashing performed. This tests
the existing BSP worker before moving RDX GUI/control ownership. The renderer,
effects, sample update rates and audio interrupt ownership remain unchanged.

## Procedure on the FM-1

Open the local `/install/` page. Previous single-core Preview 17 remains under
`/compiler/o2/` for rollback through the same checked installer.

Hold SEL and tap REC four times, then release SEL: CONTROL CORE TEST.
The initial OFF state is the baseline with CPU1 booted but no jobs submitted.
Hold the same four notes throughout the comparison; keep preset and FX unchanged.

1. In OFF, collect at least 30 seconds. Record APP, SYN, XRUN and >90%.
2. SELECT chooses IDLE; SAVE starts it and resets audio/test counters. Collect
   at least 30 seconds with the same held notes, then record all rows.
3. SAVE stops; SELECT chooses BUSY; SAVE starts a fresh measurement. Repeat.
4. Repeat busy with encoder movement and USB MIDI/audio recording. Stop with
   SAVE or HOME. Leaving this diagnostic automatically stops new submissions.

CORE should be 1, FAULT 0, ERR 0 and successful OK replies should increase.
IDLE means a checksum-only job, not a sleeping worker; BUSY adds 8192 dependent
unsigned multiply/add iterations with a volatile stack value. This is a bounded
RAM/branch/arithmetic contention probe, not a faithful simulation of GUI flash
fetches, LCD transfers or every I/O workload. It does not prove the final split
is faster or safe. Repeat boots and a longer soak before proceeding.

ROUND is maximum **main-loop collection round trip**, in milliseconds. It
includes display/USB scheduling latency. LATE counts collection intervals at
least 10 ms, including completed jobs collected late; neither is a direct worker
execution time or an audio deadline-miss counter. XRUN and >90% cover audio.
CPU percentage elsewhere is a ~16-block smoothed measure, not a maximum.

## Ownership and failure contract — VERIFIED-SOURCE

CPU0 app_init starts CPU1 before platform audio/timers, using `cpu2=true` and
`cpu2_fallback=true`. The BSP's guarded start/check-word/fault/flash-gate protocol
is unchanged. CPU0 main loop owns a single outstanding request. It publishes
sequence, mode and 32 varying words, invokes the BSP memory barrier and dispatches
the RAM worker. CPU1 owns reply/result until completion. CPU0 only reuses the
mailbox after guarded collection; it independently verifies sequence and checksum
plus the known result of the workload. Unsigned wrapping arithmetic is defined.

No waits or worker interactions were added to app_render or synthesis. Main-loop
collection polls busy nonblockingly; on completion/error or after 10 ms it uses
the BSP's existing bounded wait/retirement API. The latter can wait up to its
existing 8 ms timeout in the **main loop**, with audio interrupts enabled. A
fault stops further submissions; startup failure leaves normal single-core audio.
This retains the BSP's hardware limitations, including reports of unreliable
shared RAM on some units; checksums are detection, not a hardware guarantee.

Stop the diagnostic before saving/editing firmware. Existing platform flash
guards refuse flash entry with an outstanding worker; no flash APIs are called
by the worker. Other GUI and I/O still run on CPU0 in this build.

## Build and validation — MEASURED-LOCAL (host/build only)

Baseline: local Preview 17 O2; user-reported normal SYN 1817 us O2 and 1816 us O3,
both with/without U4. These are user hardware reports, not a reproduced benchmark.
fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319; BSP
f869dfa8dc3c081f040407e370d303349e4444d5; existing pi32v2 clang 4.0.1 / AC79 SDK.
DSP remains O2, r3/fprev1, FP contraction off; 44.1 kHz, 128 frames, 2902.49 us.

Commands: `bash tools/build-fm1.sh`, `bash tools/test-app.sh`,
`bash tools/build-emu.sh`, `node tests/emu-test.mjs`, `node tests/site-test.mjs`.
`cc -std=c11 -Wall -Wextra -fsanitize=undefined,address tests/core_test.c ...`
passes sequencing, no premature reuse, both workloads, corrupt reply rejection,
stop/drain and timeout retirement with mocked platform calls. Host control tests
exercise the new page and existing app suites. Browser tests cover all 33 presets,
finite audio, controllers, FX, profile navigation and panic. Browser/host CPU1 is
simulated synchronously and cannot validate target concurrency.

Target guards pass: RAM code 20684/32768 B, data+bss 49884/98304 B,
pool 333572/344064 B, image 186728 B. CPU1 default user stack 2816 B plus
256-byte exception stack; actual peak stack remains a hardware check.
Disassembly shows core_worker is a 92-byte RAM leaf with bounded loops,
integer operations and no calls, allocations, libm or software-double helpers.
The existing GUI-unused-variable and FX constructor conversion warnings remain.

Firmware SHA256: `11d9733ac8a4a1492d929f8bf4017a330fe611ba44df0c6ac2ebdbbe78bb8390`.
Wasm SHA256: `4e99fed249ba55a09ecd309ccd86fa61cd8d86f3d326a904f7f0076278acdd8f`.
Build ID 1CBCB7EC; `build/evidence.json` records full source/artifact hashes.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target build and existing ABI/manifest/register guards retained; no SDK/BSP edits. |
| Audio hot path / numerics | PASS — unchanged DSP and audio callback; worker is bounded unsigned integer RAM leaf, target guard/disassembly inspected. |
| Execution ownership / synchronization | PASS (source/host scope) — one outstanding mailbox, BSP barriers and completion/retirement before reuse; no audio waits; hardware visibility remains UNVERIFIED. |
| Memory / storage / I/O | PASS (static/build scope) — linker budgets fit; no persistent format/write changes, existing flash gating retained. Actual CPU1 stack peak and contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — no new external dependencies/assets; existing platform licensing and notices retained. |
| Compatibility / package / recovery | PASS (build/site scope) — original identity, loader and storage unchanged; pinned package/installer checks. Actual flashing/rollback UNVERIFIED. |
| Host functional validation | PASS — complete app suite, new UI scenario, ASan/UBSan protocol test, matching wasm/worklet suite and installer tests. |
| Hardware real-time validation | UNVERIFIED — run OFF/IDLE/BUSY procedure, I/O stress, boots and soak; record errors, faults, audio peaks/XRUNs and stack. No higher polyphony or speedup claimed. |

Skipped: repeated DSP equivalence/full effects sanitizer suites because no DSP
code changed; real hardware fault injection, analog/USB listening, flashing,
deadline measurements and multicore soak unavailable to the agent.
