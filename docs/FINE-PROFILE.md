# Fine renderer diagnostic — Preview 2026.11.22-16

This adds a finer split to the Preview 15 renderer diagnostic. Normal rendering and the previous coarse profile remain available; no clock, envelope curve/rate, sample rate, gain, effects, voice ceiling or core ownership is changed.

## User-device results

User-transcribed results supplied after installing this diagnostic (four voices):

| Stage | Fine (us) | Coarse (us) |
| --- | ---: | ---: |
| PREP | not displayed separately | 13 |
| MOD | 347 | 335 |
| ENV STEP | 363 | — |
| ENV GAIN | 259 | — |
| ENV total | 622 (sum) | 526 |
| PITCH | 262 | — |
| WAVE | 704 | — |
| OPS total | 966 (sum) | 964 |
| MIX | 44 | 50 |
| TOTAL | 1986 | 1889 |

Coarse BLOCKS 9191, about 26.7 seconds at nominal cadence. Fine duration, exact held notes/velocities, FX slot types and clock readback were not supplied with this transcript. Treat these as user-device reports, not directly measured agent results. Each mode selects its own worst-total block; these are not simultaneous samples or averages.

The fine total is 97 us (5.14%) above coarse. Its envelope sum is 96 us (18.25%) above coarse ENV, whereas its operator sum differs by only 2 us (0.21%). This is consistent with significant envelope capture/readback overhead; it does not isolate that overhead exactly because the two snapshots are different blocks. Do not assign the original fused envelope a literal 363 us state-advance cost plus 259 us conversion cost. The operator split is a useful prioritization clue: WAVE is about 73% of the fine operator bucket and PITCH about 27%. WAVE still groups feedback, signed phase wrap, sine interpolation, gains and routing. It is not a standalone sine-LUT measurement.

The displayed fine components sum to 1979 us; the remaining 7 us includes undisplayed PREP and orchestration/rounding. Coarse components sum to 1888 versus TOTAL 1889 us. These numbers support examining WAVE and envelope generated code before a blanket numeric-format change. A candidate optimization still needs output equivalence and the same-workload device A/B; these measurements alone do not establish a higher safe voice limit or a guaranteed speedup.

## Running

Install the matching local preview. From normal view, hold SEL and tap REC three times for Render Profile. Turn SELECT one step to choose Fine Render. SAVE starts measurement. Play the same four-note P7 workload used for the coarse 1888 us result, then photograph the screen. SAVE stops while retaining results; HOME restores normal rendering. Turning SELECT while measuring switches between coarse and fine and restarts the measurement, so each snapshot belongs to one mode. Changing a preset exits profiling.

- ENV STEP: envelope stage advancement, boundary transitions and capture of the per-sample level/curve coefficients.
- ENV GAIN: captured level to amplitude, using the original rising linear expression or falling gain LUT.
- PITCH: per-operator semitone-ratio lookup, phase advance and positive phase wrap, plus capturing the old phase for waveform rendering.
- WAVE: feedback rectification/filter, modulated lookup-phase wrap, sine LUT, operator gains and FM routing. Those operations remain grouped because feedback and modulation have sample-to-sample dependencies.
- MOD and MIX retain their previous meanings. TOTAL includes preparation and orchestration as well as the displayed stages. Values belong to the same worst-total block, with its voice count.

Only stage boundaries are timed. No timer reads are added inside per-sample or per-operator loops. All component kernels remain in checked RAM; timer/orchestration calls stay outside them. The finer split needs 10,240 additional bytes of static scratch: four 16-byte envelope snapshots per sample and four old phases per sample, reused for each voice. Capturing each sample's coefficients after its envelope step preserves stage transitions. No new cross-context state access is introduced.

The extra stores, later loads and altered code layout are diagnostic overhead. Fine ENV STEP + GAIN can differ from coarse ENV; fine PITCH + WAVE can differ from coarse OPS. Compare TOTALs before attributing costs. This is not a promise that replacing those stages with a different algorithm would save their full measured time.

## Generated-code observation

The inspected pi32v2 disassembly shows the coarse envelope loop inlining stage/rising tests, level/step/target loads, adds/subtracts, level stores and boundary comparisons. Stage advancement remains a RAM call on transitions. Falling gain conversion inlines range clamps, float-to-int truncation, two table reads, int-to-float conversion, interpolation arithmetic and the output store. No hidden pow/exp/log library call explains its cost. The short original C++ may already describe the necessary math compactly; this diagnostic is intended to distinguish state/branch/memory cost from gain conversion, not presume that a shorter C++ rewrite exists.

## Evidence and compliance

Baseline: local Preview 15, coarse device report 1888 us at four voices; the source tree also includes the earlier uncommitted diagnostic changes. Platform and BSP pins remain 7932dff6f1474cc8c3420e0df75eb75879e47319 and f869dfa8dc3c081f040407e370d303349e4444d5. Existing AC79 SDK/pi32v2 clang 4.0.1/r3 and build flags are retained. Nominal 44.1 kHz / 128-frame deadline is 2.90249 ms. Exact source/artifact hashes: build/evidence.json.

- Target/build/ABI: PASS build and retained RAM-closure, register, image, package and soft-double checks. Twenty RAM functions close; total RAM code 20,264/32,768 bytes, data+bss 46,460/98,304, pool 333,572/344,064. Logs: build/fine-target.log and build/fine-disassembly.txt. Device execution/stack margin UNVERIFIED.
- Hot path/numerics: PASS bounded stages, no per-sample timer calls, heap, locks, I/O, doubles or transcendental calls added. Matching normal/coarse/fine host output below. Actual target numerical equivalence and overhead UNVERIFIED.
- Ownership/synchronization: PASS existing audio-only static scratch and interrupt-protected UI snapshots, single-core execution retained. Diagnostic modes have named constants. Budget learning remains frozen during profiling; overload protection remains active.
- Memory/storage/I/O: PASS source bounds and linker budgets; no storage/peripheral changes. Hardware memory contention, stack high-water and I/O overlap remain UNVERIFIED.
- Dependencies/includes/licenses: PASS no external dependency or implementation introduced; original notices retained. Regenerating the imported core headers produces identical files. Existing original-source licensing uncertainty remains.
- Compatibility/package/recovery: PASS unchanged patch/storage format and retained package/update-loader checks. Device installation, rollback and USB recording retest UNVERIFIED.
- Host validation: PASS 1,188 UBSAN normal/coarse/fine equivalence scenarios, including live switches, envelope transitions, mono/glide, sustain, retirement and panic. Separate-process fine output across all 33 presets, including random LFO modes, matches the 25,952,256-byte normal stream: SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645. Host UI tests cover fine/coarse selection while active, stop/start, retained snapshots and HOME exit. Browser checks exercise audible fine rendering and return to normal. Final installer pins/checks are validated at delivery.
- Hardware real-time: UNVERIFIED for the fine split. Compare the same workload in normal, coarse and fine modes, note voice count and TOTAL, inspect XRUN/overload counters and listen/record for changes. No device was flashed by the agent. This change provides measurement, not an optimization or higher-polyphony claim.

Repeated full effects/manual sanitizer suites are skipped because those algorithms and interfaces are unchanged. No multicore implementation is included in this diagnostic.
