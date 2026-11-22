# CPU1 dry generation test — Preview 2026.11.22-26

Local experiment on codex/audio-control-isolation, based on 52b980f / Preview 25.
No public publishing or agent flashing. Installer: http://127.0.0.1:8790/install/.
Rollback: http://127.0.0.1:8790/install-preview25/.

## Hardware test

The mode starts OFF on every boot. Select the same patch/FX as the previous
measurements. Hold SEL and tap REC seven times, then release SEL, to reach
**DRY CORE PIPELINE**. Turn SELECT once: SYN CPU1 enables it; another turn returns
to CPU0. SAVE resets timing/statistics. HOME leaves the page without disabling
the pipeline; this permits testing other editor screens while it runs.

First hold the same four-note chord, reset maxima after attack, and observe for
30 seconds. Report WORK, TURN, APP, FX, voice count/limit, LATE, FAULT and XRUN.
Then try five and six notes, retriggers/slow attacks, the heaviest FX/modulation,
rapid encoders, screen changes, program changes, sustain, bend, dense USB MIDI,
Soundmondo and USB recording. Re-enter the page to read counters. Reset between
comparable workloads; include elapsed duration. Repeat with CPU0 for comparison.
Do not run the existing voice sweep/fine renderer in CPU1 mode: they are disabled
there to avoid changing the ownership contract. Switch CPU1 mode off first.

- WORK: maximum CPU1 dry generation time, including any CPU1 interference.
- TURN: maximum publication-to-completion time, including waiting for a GUI
  checkpoint. It excludes the rest of the block after publication.
- APP: CPU0 callback maximum; in pipeline mode it excludes asynchronous SYN.
  APP and WORK overlap, so their sum is not an elapsed block measurement.
- LATE: an audio callback found its next dry block unfinished and emitted zero
  samples. This is an application dropout even if the platform XRUN remains 0.
- FAULT: existing BSP retirement/fault reason. STK is CPU1's stack high water.

All timing is zeroed on host/browser: those runtimes test behavior, not pi32
performance. A successful first test requires no late blocks or faults under
the intended workload, acceptable sound/controls, and measured headroom.

## Ownership and pipeline — VERIFIED-SOURCE, hardware UNVERIFIED

CPU1 generates ALL active dry voices in this first experiment: per-sample PEG,
LFO, glide, AEG, oscillator, operator modulation and feedback. This deliberately
measures full generation on CPU1 before adding a second concurrent DSP partition.
CPU0 sums voices in the original order, applies original gain/saturation, FX,
performance/master gain and output conversion. GUI/control remains on CPU1 when
its existing switch is enabled. Hardware interrupts/USB transport stay on CPU0.

At callback entry CPU0 acquires completion of the preceding generation. Only
then may it apply commands, change patches, observe voices or change admission.
It copies/mixes that completed dry bus to its existing stereo arrays. It then
publishes generation of the NEXT block and immediately continues FX/output.
The 4096-byte per-voice buffer can be reused because CPU0 has finished reading
it; CPU0 output arrays and the next dry buffer provide the pipeline's separate
storage. Initial priming produces one zero block. Voice generation latency is
one block (128/44100 = 2.90249 ms) in steady playback. Control/GUI backlog and
late blocks can add latency; this is not an end-to-end latency guarantee.

Publication/completion use aligned 32-bit volatile sequence words plus the
existing BSP csync barriers. Volatile alone is not relied on. CPU1 owns live
voices and render scratch until completion; CPU0 owns them between jobs.
There is no heap, engine copy or concurrent voice mutation. Original LFO random
stream traversal is retained. CPU0 captures completed-job counts/times before
dispatch; budget decisions made while CPU1 renders only update the budget.
The new voice limit is applied at the next acquired boundary.

CPU1 services pending generation at control-job entry/exit, canvas strip blits,
and every eight rows of bitmap scaling. An idle core receives an ordinary BSP
audio job. No persistent custom worker bypasses the BSP's flash-idle handshake,
request check words, guarded startup or fault retirement. CPU0 dispatch sites
exclude audio interrupts to serialize the shared BSP mailbox. The audio callback
never waits, calls wait(), or processes unfinished output. A late block is muted,
commands remain queued, the pending job is retained, and LATE increments.

Main-loop snapshot/commit/panic waits acquire ownership with audio interrupts
enabled. It services BSP completion/retirement on timeout before recovering
ownership. Fault recovery disables the pipeline and requests panic after the
worker is completed/held. UI control jobs have a 500 ms main-loop timeout in
pipeline mode (100 ms otherwise), because audio checkpoints extend UI wall time;
the existing BSP retirement routine remains bounded to 8 ms. None of these waits
runs in audio. LCD waits and indivisible editor operations remain possible
checkpoint delays; their adequacy is exactly what TURN/LATE must test.

Patch/fade transitions dispatch at callback end, after all live state changes.
Their work does not have the steady path's full overlap. Old dry data is
invalidated on panic/patch replacement. FX processing, hidden operator state and
all sample-rate envelope/modulation arithmetic are retained; patch/controller
transition listening and latency still require hardware validation.

The existing patch-local budget uses max(CPU0 APP, completed CPU1 TURN) as a
conservative workload sample, rather than adding overlapping costs. Its marginal
model remains approximate and does not establish a universal voice capacity.
The pipeline suppresses the second platform overload action for that block;
normal CPU0 budgeting is unchanged. No hardcoded five-voice guarantee is added.

## Build and validation

FM-1 / AC79, fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319,
felucca-bsp f869dfa8dc3c081f040407e370d303349e4444d5. Existing AC79 SDK and
clang 4.0.1 pi32v2r3/fprev1, engine/editor O2, platform/app Os,
-ffp-contract=off. 44.1 kHz, 128-frame block, 2902.49 us service period. No RTOS,
clock, loader, storage, USB identity or pin changes. Build 26A45741; complete
external source/artifact hashes are recorded by tools/write-evidence.mjs in
build/evidence.json (the platform build ID alone omits external DSP includes).

Target: image 195032 B; RAM code 22140/32768 B; data+bss 68348/98304 B;
effect pool 333572/344064 B; CPU1 stack allocation remains 8192 B. Generated
code confirms RAM renderDry/mixDry/dry_core, far dispatch to RAM, original
operator kernels and barriers. Builder guards reject soft-double helpers,
unsafe inline wide multiplies, register access outside BSP and memory overflow.
Existing source FX conversion/editor warnings remain; none introduced here.

Commands: bash tools/build-fm1.sh; bash tools/build-emu.sh;
bash tools/test-app.sh; node tests/emu-test.mjs; node tests/site-test.mjs.
The app suite includes sanitized bridge, measurement and dry-pipeline tests.
1188 dry/normal bit-exact scenarios cover all 33 patches, 12 algorithms and
three mono/poly modes, with deterministic LFO selection in paired-instance
comparisons, live changes, retirement, release and panic. Random LFO arithmetic
is unchanged/source-reviewed, not independently paired due to its global PRNG.
Delayed job tests cover ownership, initial silence, duplicate service, deferred
admission, transitions, mode switching and simulated held-worker retirement.
Host and wasm exercise actual page switching, audible notes, panic and CPU0
return; emulator does not simulate simultaneous cores or real cache behavior.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — matched existing target build, manifest/ABI guards retained; internal APIs only. |
| Audio hot path / numerics | PASS (source/build/host) — bounded arrays/loops, no new double/libm/allocation/I/O/waits; exact dry comparisons and generated-code guards. Target costs UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/host) — single voice owner, BSP barriers and guarded jobs, no audio wait; delayed-job and bridge tests. Actual multicore visibility/fault behavior UNVERIFIED. |
| Memory / storage / I/O | PASS (static/build) — buffers and sections fit; unchanged flash gate, storage and peripherals. Actual stack/contention/GUI/MIDI latency UNVERIFIED. |
| Dependencies / includes / licenses | PASS — only local APIs, no imported third-party code/assets/dependencies; existing notices retained. |
| Compatibility / package / recovery | PASS (build/site) — existing product/loader/storage and pinned local package, Preview 25 rollback retained. Physical update/recovery UNVERIFIED. |
| Host functional validation | PASS — full app/bridge/measurement suites, sanitized pipeline and 1188 exact dry cases; matching wasm and installer checks. |
| Hardware real-time validation | UNVERIFIED — device flashing, physical multicore/cache/fault behavior, WORK/TURN/LATE, listening/recording, dense I/O, boots and soak remain to be measured using the procedure above. |

Skipped: physical flashing/recovery, hardware fault injection, target timing,
real output/listening/USB recording and soak. No performance or polyphony win
is asserted before these measurements.
