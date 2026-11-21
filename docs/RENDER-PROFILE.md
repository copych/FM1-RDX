# Renderer instrumentation — Preview 2026.11.21-15

This is the single-core baseline stage before changing core ownership. No second core is started by this preview. It adds an optional split renderer for component timing and leaves the normal RAM renderer available for direct device comparison. At initial delivery no device timing was available; the subsequent user report is recorded below.

Follow-up device report: PREP 7, MOD 342, ENV 518, OPS 977, MIX 43, TOTAL 1888 us, four voices, 67,943 blocks (about 197 seconds at the nominal cadence). Values are from the worst-total block, not averages. They sum to 1887 us before separate display rounding. The user supplied this transcript; it was not measured directly by the agent. Prior normal SYN peak was 1841 us in the earlier photo. These are useful comparable observations, not a controlled proof that the split has exactly 47 us overhead. The next diagnostic separates envelope state/gain and operator pitch/wave work; see [FINE-PROFILE.md](FINE-PROFILE.md).

## Device procedure

1. Keep the same patch, notes, FX and USB recording state throughout each comparison. Start with the earlier P7 four-note workload and both slots THRU.
2. Hold SEL and tap REC once to open Audio Timing. SAVE resets its peaks; play the workload and photograph this normal-render baseline.
3. Repeat SEL + REC twice more (through DSP Benchmark) to reach Render Profile. It initially says OFF. SAVE starts profiling and resets its component snapshot.
4. Play the same notes. Photograph PREP, MOD, ENV, OPS, MIX, TOTAL and VOICES. SAVE stops profiling while retaining the snapshot. Starting again clears it. HOME restores normal rendering; selecting a preset also restores normal rendering.
5. Return to Audio Timing and inspect XRUN and >90% counters. Reset its peaks before another normal-render comparison: peaks observed during the diagnostic do not describe normal-render cost.

PREP: zeroing the accumulator, selecting active voices, and advancing each voice's block LFO. MOD: PEG, glide where applicable, and per-sample pitch/amplitude modulation preparation. ENV: all four amplitude envelopes. OPS: oscillator phase advance/wrap, pitch and sine LUTs, feedback, gains and FM routing. MIX: voice accumulation, retirement fades/reset, output gain and saturation. TOTAL includes all diagnostic synthesis work, dispatch/timestamps and orchestration; it excludes effects, app control handling and final device output conversion (existing Audio Timing covers those scopes).

Rows are retained from the block with the largest TOTAL since profiling started, not independent per-component maxima. Round-up to microseconds can make displayed sums differ slightly. TOTAL can exceed the component sum because the orchestrator lies outside the component intervals. VOICES refers to that captured block; BLOCKS counts all diagnostic blocks. No per-sample timestamps are taken. Normal voice-budget learning is frozen while profiling; the existing platform overload response can still reduce the limit. Hosts show zero profile timing and HOST: NO TIMING.

## Measurement limitations

Normal rendering is sample-major and fuses modulation, envelopes and operator work. The diagnostic processes one voice's block in three bounded RAM stages using 6,656 bytes of static scratch, followed by mixing. The equations and per-voice accumulation order are preserved. Separating the stages changes code generation, instruction/data working sets and register lifetimes, so component numbers describe this split path, not exact percentages of the original fused kernel. Compare its TOTAL to the normal SYN baseline before using the breakdown to choose an optimization. A much faster/slower diagnostic total is itself a clue about execution layout, not proof of a single arithmetic bottleneck.

Timing reads and orchestration remain outside the checked RAM kernels. Each component includes its far-call/return and timer overhead. At most eight voices are processed; a retiring voice prepares exactly its remaining fade frames. The runtime contains no waits, heap allocation, logging, storage or new synchronization. Static scratch is audio-owner-only. The main loop copies/resets profile state under the existing interrupt exclusion; no cross-core safety is claimed for that exclusion.

## Core ownership findings and next stage

The pinned fm1-base build has `cpu2` disabled. All application synthesis/FX and MIDI/event consumption run in CPU0's audio interrupt; UI and Soundmondo parsing run in its main loop. The platform's USB audio work continues after app_render returns. This confirms missing ESP-style separation, but GUI time alone does not explain the synthesis interval measured inside the callback.

The supported CPU1 facility is a checked render-job worker (`plat_cpu2_*`, include/plat.h lines 648 onward). It starts at boot before audio/timers, has stack guards, mailbox checks and fault/start fallback. Its stock completion path synchronously waits, with an 8 ms failure timeout. That exceeds a 2.90249 ms block deadline and is not a suitable new wait inside this project's audio callback. Local interrupt exclusion does not protect state from CPU1. No platform/BSP files or safeguards have been altered.

The planned dedicated-audio layout is CPU0 for platform I/O, input/SysEx parsing and GUI, and CPU1 for synth/FX state and rendering. CPU0 would publish a bounded block command packet, consume only completed output, and publish GUI/patch snapshots copied from completed jobs. It must never wait for unfinished work in the audio callback or read the worker's live engine. Panic, patch changes, Soundmondo snapshots, USB fixed-level recording, output gain and budget diagnostics all need explicit ownership in that protocol. Asynchronous dispatch needs a completion/fault/deadline poll and safe retirement at the platform boundary; `plat_cpu2_busy()` alone does not retire a stuck worker. Buffering may add one block (2.90 ms) of latency. Startup failure must select the existing single-core path; late/faulted jobs need held-worker recovery before any single-core access to its state. This second stage remains to implement and validate after obtaining the baseline; it is not enabled or claimed complete in this preview.

## Compliance and validation

Baseline is main 2ff9f47 plus the local Preview 14 diagnostic work; unchanged fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319, BSP f869dfa8dc3c081f040407e370d303349e4444d5, AC79 SDK/toolchain and flags. pi32v2 clang 4.0.1/r3; nominal 44.1 kHz, 128 frames, 2.90249 ms. Actual output cadence and bootloader remain unmeasured locally. Exact source/artifact hashes are in build/evidence.json.

- Target/build/ABI: PASS retained target checks, all 16 RAM functions closed. RAM code 18,084/32,768 bytes; data+bss 36,204/98,304; pool 333,572/344,064. Final target log: build/renderer-target.log. Hardware ABI execution/stack high-water UNVERIFIED.
- Hot path/numerics: PASS source and target closure/helper audit, bounded stage loops, no doubles/libm/heap/I/O/waits. Host bit-exact comparisons below; target numerical equivalence and actual diagnostic overhead UNVERIFIED.
- Ownership/synchronization: PASS single-core source review, audio-only scratch and existing short interrupt-protected UI snapshots. Multicore remains a separate unimplemented stage.
- Memory/storage/I/O: PASS static bounds and linker budgets, no storage or peripheral changes. Actual cache/interference behavior and stack margin UNVERIFIED.
- Dependencies/includes/licenses: PASS no external implementation/dependency added; existing notices retained. Importer reproduces generated headers exactly. Existing original-source licence uncertainty is unchanged.
- Compatibility/package/recovery: PASS retained package/loader checks, no patch/storage format change. Physical update, rollback and recording retest UNVERIFIED.
- Host functional validation: PASS full application suite including profile entry/toggle, notes, retained results, HOME exit and no storage writes. 1,188 UBSAN bit-exact normal/split scenarios cover all presets, 12 algorithms and 3 voice modes, live mode switching, sustain, stealing/retirement, live algorithm/operator changes, pitch/wheel modulation, release and panic. Paired tests substitute sine for global-random LFO modes to avoid advancing one global generator twice. Separate-process all-33-preset dry streams include the original random modes: both 25,952,256-byte streams SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645. Matching browser/installer checks are recorded at delivery.
- Hardware real-time: UNVERIFIED. Run the normal/profile workload comparison above, observe output, USB recording, XRUN/overload counts and duration. No device was flashed here. This preview provides measurement tools, not a higher-polyphony or two-core performance claim.

Full repeated effects/manual sanitizer suites are skipped because their algorithms and control semantics are unchanged; normal application/browser scenarios still exercise those paths. No test or compiler success substitutes for the new device baseline.
