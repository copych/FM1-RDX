# Hardware voice-cost sweep — local Preview 2026.11.22-25

Diagnostic based on d57f7b4 / Preview 24 block kernels. Local `/install/`;
previous pinned installer `/install-preview24/`. No publishing or device flashing
by agent. DSP kernels and effect algorithms are unchanged.

## Device procedure

1. Use the same **sustaining polyphonic patch and FX** as the previous test.
   Play your usual four-note chord briefly to seed the normal prediction, then
   release every key/pedal and stop incoming MIDI. P6=0 means no learned model.
2. Hold SEL, tap REC **six times**, then release SEL: **VOICE COST SWEEP**.
3. SAVE starts the test. It takes up to 35 seconds, stepping 0..6 notes with
   two seconds settling and three seconds steady recording at each count.
4. After DONE or LIMIT, SELECT browses the retained rows. Send rows **0, 1, 4,
   5 and 6** where available: ACT, APP, SYN, FX, P6, STEADY, XRUN and status.
   A row with no recorded blocks is untested. STEADY=0 means no settled
   observation was completed; a LIMIT row is not a sustained-polyphony result.
5. SAVE stops/restarts; HOME exits and stops new test notes. New notes,
   controllers, patch/editor changes, another diagnostic or panic cancel the
   controlled workload. Results are RAM-only and retained until a new run/boot.

The test is audible. It uses the current patch/FX/master/controller settings,
MIDI notes 48,55,60,64,67,72 at velocity 100 (first N notes), restarting voices
at each stage. FX state/tails are not reset; the zero-voice stage includes any
remaining tail. Mono/legato patches are rejected (POLY ONLY). Patches whose
voices end naturally during a stage stop with VOICE LOSS, rather than falsely
labeling a smaller active set as N sounding voices. Five seconds per stage is
not a worst-case envelope/FX duration guarantee for arbitrary patches.

## Meaning of the measurements

Adaptive **prediction-based admission/learning is frozen only during the sweep**,
so the old estimate cannot prevent measuring five actual voices. Each stage
temporarily sets admission to N (one available slot but zero notes for row 0).
The original normal budget/model is retained and its current admission restored
after stop, completion, invalidation or overload. Test voices are silenced and
the sustain-controller setting is restored. Other patch/controller settings are
not changed; an incoming change cancels the test before applying normally.

Actual protection remains: APP over 90% of the block deadline stops the sweep;
the platform's full-callback >90% overload hook also stops it, including IRQ/USB
overhead outside the APP timer. The one offending block may already have exceeded
its deadline: this is a measured stress test, not a guarantee of zero XRUNs.
No further voice count is attempted after LIMIT. Following normal blocks regain
ordinary protection; a measurement stop does not permanently suppress shedding.

APP/SYN/FX are independent maxima over **all** recorded stage blocks, including
warmup/note setup. Stage note setup occurs before the control timestamp t1, so
its cost is included in APP (not the narrower SYN timer). STEADY counts only the
post-settling portion. All-block count and SYN/FX paired with the worst APP block
are retained in the 8-word row snapshot, in addition to the independent maxima.
ACT is the latest actually rendered count; a mismatch aborts. P6 is the historical
normal six-voice prediction captured at sweep start, not learned from the test.
Thus actual five/six-note times can be compared with the existing conservative
estimate without changing its production formula or safety margins.

The application guard is floor(deadline_ticks*0.9), displayed rounded upward
(2613 us). The platform's integer-microsecond guard can stop slightly earlier and
also includes work/interrupts outside APP. XRUN is the platform counter delta
from SAVE. Normal production admission remains 85% target / 80% growth margin.
Measurement-off adds small bounded scalar checks to the callback; snapshots and
the sweep's recording also have overhead. Compare counts within this build;
do not assume timing instrumentation is free versus Preview 24.

## Ownership and execution

CPU1 only requests COST_START/STOP through the existing checked command queue.
CPU0 audio owns test state, count changes and peak recording. Up to six note-on
calls at a stage boundary are bounded audio-owner control work, included in APP.
No log output, allocation, I/O, waits or worker dependencies are introduced in
audio. State has seven fixed rows; no dynamic storage. The UI reads a complete
62-word snapshot captured by CPU0 under its local interrupt lock, never live
CPU1 access to the synth. Platform panic cancels under that same lock.

The test requires a normal ready poly patch. It may wait up to five seconds for
initial patch/FX preparation; later patch/mode invalidation cancels. Profiling
is turned off on start, and enabling a render profile cancels the sweep. A
one-block guard marker prevents the same test overload from reducing the saved
normal budget a second time; it resets at the next audio block. Host tests
explicitly verify ordinary overload shedding resumes after that reset.

## Build and host validation

Same fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319, BSP
f869dfa8dc3c081f040407e370d303349e4444d5, AC79 SDK / pi32v2 clang 4.0.1,
r3/fprev1, O2 engine/editor, FP contraction off; 44.1 kHz/128 frames,
2902.49 us deadline, same clock/voltage/core/peripheral ownership.
Image 191076 B, RAM code **19832/32768 B** unchanged from Preview 24,
data+bss **64156/98304 B**, pool **333572/344064 B**. Same identity/loader/storage.
Build ID 2C310B38. Firmware SHA256
d3a0d058c33deddc46a19e15dddb2edd0f69a86d170df106656819127c0792b7;
wasm SHA256 e5af53bd62312c7e43c969693a8d1e367af818b825a14d5ba59195ba768c1f69.
Full source/artifact hashes: build/evidence.json.

PASS: tools/test-voice-cost.sh (ASan/UBSan/float-cast checks): policy tests cover
all seven rows, timer wrap, warmup/steady transitions, paired peak recording,
90% guard, mismatch/invalid data and overload target. Real adapter integration
tests cover forced count, idle baseline, restoration, same-block overload
suppression, next-block normal shedding, mono rejection, editor invalidation
and sustain restoration. An initial test compile needed the explicit standard
initializer_list include; corrected and rerun successfully.

PASS: full tools/test-app.sh, now including the sweep script: start/stop, complete
0..6 progression, retained row selection, no stuck notes, incoming MIDI
cancellation and zero persistent writes, plus existing MIDI/Soundmondo, UI,
profile, core fallback and bridge sanitizer suites. The bridge mock was updated
for the new snapshot and continues to assert CPU0-only capture under the lock.
Matching wasm and tests/emu-test.mjs pass all 33 presets, finite audio,
FX/controllers, encoders, profile transitions and panic. Sweep screenshot was
visually inspected. Host timing/cores are simulated, not hardware evidence.

A repeated host run exposed an OS descheduling pause that tripped the device
threshold and stopped the sweep at row 4. The host/browser sweep now records
zero timing/prediction and explicitly displays HOST: NO DEVICE TIMING, like
the existing benchmarks. Device recording/90% guards are unchanged. Native
policy tests exercise supplied timing values directly; adapter integration
tests explicitly invoke the platform overload hook. The complete app/safety
suite and final matching wasm were rerun successfully after this correction.

PASS: target guards for ABI/manifest, RAM closure (27 app RAM functions), register
access, no software doubles, 64-bit-load hazard, image/loader/package integrity.
Original GUI/FX and compact-C host warnings remain. Pinned installer/package/link
tests and git diff --check pass. No external dependencies, assets or licenses
changed. Physical protection/latency and actual data remain unverified.

User-reported Preview 24 baseline: APP 1981/SYN 1655/FX 341 us, predicted 6V
2972 us, four active/admitted voices, XRUN 0. These are user reports, not an
agent-controlled benchmark. This diagnostic is intended to test actual occupancy
against that prediction, not promise higher production polyphony.

## Compliance gate

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — matched compiler/platform, target guards retained; diagnostic snapshot/event additions internal, persistent/wire ABI unchanged. |
| Audio hot path / numerics | PASS — bounded fixed rows/notes/scalar checks, no new double/libm/I/O/allocation/waits; unchanged DSP kernels, target guards and policy/engine sanitizers. Hardware overhead UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/host scope) — CPU0 audio owns sweep; CPU1 uses queued commands and captured snapshots; panic cancel under CPU0 lock, normal budget/controller restoration tested. |
| Memory / storage / I/O | PASS (static/build scope) — seven bounded rows, 62-word snapshot and linker budgets fit; no flash writes, loader/peripheral changes. Actual timing/stack/contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — explicit local header/API dependencies, no external code/assets or license changes. |
| Compatibility / package / recovery | PASS (build/site scope) — same checked identity/loader/storage, pinned installer and Preview 24 rollback. Real flashing/recovery UNVERIFIED. |
| Host functional validation | PASS — complete sweep UI/control flow, policy/real-adapter safety with sanitizers, full app/bridge and matching wasm/installer suites. |
| Hardware real-time validation | UNVERIFIED — run current patch/FX sweep, record rows/actual counts/steady blocks/XRUN/status and repeat under intended GUI/USB conditions. No six-voice real-time guarantee. |

Skipped: repeated DSP/effects numerical suites (DSP unchanged), physical
flashing/listening/recording, real overload/voice-loss/fault injection, device
timing, repeated boots and hardware soak. Guard stops future work; it cannot
retroactively prevent an initial over-budget block.
