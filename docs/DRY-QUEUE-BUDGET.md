# Paired queue timing and pipeline budget — Preview 2026.11.22-27

Local test based on commit 1e41738 / Preview 26, not hardware-qualified or
published. CPU1 generation, CPU0 summing/FX/output and one-block voice pipeline
remain as documented in [DRY-CORE-PIPELINE.md](DRY-CORE-PIPELINE.md).

User-reported Preview 26 baseline after resetting maxima: WORK 1083 us,
TURN 2279 us, APP 399 us, FX 298 us, three active/admitted voices; previous report
had LATE/FAULT/XRUN zero. These are reports from the user's FM-1. WORK and TURN
were independent maxima, so their difference was not a measured queue interval.

## Device procedure

Install http://127.0.0.1:8790/install/ (Preview 27), then close the installer.
Rollback is http://127.0.0.1:8790/install-preview26/.
Hold SEL and tap REC seven times, release SEL, then turn SELECT until the screen
shows SYN CPU1. Mode still starts disabled at boot. Use the same preset/FX,
press SAVE to reset displayed maxima, hold the same chord for 30 seconds and
report every row. Repeat while changing screens and turning knobs. Test new
four-, five- and six-note chords after admission recovers; raising the limit
does not recreate notes that were previously stolen. Do not use the synchronous
voice sweep/fine profiler with CPU1 mode enabled.

WORK/TURN remain independent overall maxima. **QMAX and the WORK beside it come
from the same completed job**. AT identifies where that job was serviced, and
the V count belongs to that job. AT is a checkpoint label, not proof that all
the queue delay was caused by that single operation. IDLE is a standalone BSP
audio job; CONTROL/LCD/TEXT/SCALE/EDITOR/PROTO identify cooperative checkpoints.
The complete paired TURN is retained in the detail snapshot as well. All times
are integer 24 MHz ticks internally, rounded upward to microseconds on screen.

WHY is the most recent pipeline budget decision: 0 none/reset, 1 CPU0 urgent,
2 late block, 3 worker completion above 90% of the block, 4 predicted capacity
reduction, 5 recovery/growth. It persists until the next decision. LATE still
counts application dropouts independently of platform XRUN. SAVE clears displayed
maxima/counters, not the adaptive model. CPU0 -> CPU1 switching resets that model.

## Changes and boundaries — VERIFIED-SOURCE

CPU1 now records start/end once per job: queue=start-publication,
work=end-start and turn=end-publication. Unsigned subtraction handles timer wrap.
At a queue maximum it stores the paired work/turn, checkpoint and voice count,
then publishes all fields through the existing completion barrier. CPU0 copies
them only after acquiring ownership. The GUI receives an additional bounded
eight-word snapshot; it never reads in-flight job fields.

Text rasterization yields before every glyph and after the string, preserving
the original cv_text end-x advance, byte/glyph mapping, clipping and blending.
Canvas begin yields before/after its existing LCD synchronization/clear. Bitmap
scaling checkpoints are every four rows instead of eight. Protocol entry/exit
are labelled. Original LCD DMA ownership and bounded hardware waits remain;
no audio work waits for the GUI. A pending dry job can execute at a checkpoint
without modifying the GUI canvas. Large indivisible editor/protocol operations
can still delay a checkpoint and must be assessed with QMAX/LATE on hardware.

CPU0 uses a separate pipeline budget; normal CPU0-only budgeting is unchanged.
It tracks CPU0 service, publication offset within the callback, queue delay and
per-voice generation cost independently. The prediction is the larger of CPU0
service and publication-offset + queue + per-voice-cost * voice-count. Including
the offset fixes the assumption that a worker dispatched partway through the
callback still has a whole block remaining before its next consumer.

Two rotating 1.5-second peak banks retain observations for 1.5..3 seconds.
An old scheduling spike can age out; queue delay is never discarded from deadline
accounting. Predicted CPU1 load must fit the existing 85% target. Growth requires
1.5 seconds since the last change and predicted load below 83%; it adds one
admitted voice at a time, maximum six. A fixed CPU0/FX peak blocks growth rather
than making the CPU1 prediction loop shed every voice. Actual CPU0 or worker
completion above 90%, or a late-job signal, reduces admission by one. All policy
inputs are bounded before sums/products. Estimates are conservative heuristics,
not guarantees for future attacks, algorithms, modulation or effects.

The policy runs on CPU0 and mutates no worker-owned voices. Its new limit applies
at the next acquired boundary. CPU0 captures completed-job costs before launching
new work. Each completion is consumed once, so an effects-clear pause cannot
repeatedly charge an old job. CPU0's application and platform overload hooks
avoid charging the same measured callback twice. A platform-only outer-service
overload is deferred and accounted for after ownership returns. Existing
no-wait audio behavior, muted late blocks, BSP retirement/fallback, flash gates,
command queues and panic/patch contracts remain in place.

## Build and validation

Board/SDK/compiler/platform/44.1 kHz/128-frame configuration is unchanged from
Preview 26: fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319, BSP
f869dfa8dc3c081f040407e370d303349e4444d5, matched AC79 SDK, clang 4.0.1
pi32v2r3/fprev1, engine/editor O2, platform/app Os, -ffp-contract=off.
Block period 2902.49 us. Loader/storage/pins/clocks and product FM-1_9900 unchanged.

Target build F3127F8D: image 196328 B, RAM code 22140/32768 B,
data+bss 68508/98304 B, pool 333572/344064 B; CPU1 stack remains 8192 B.
Build guards pass: no soft-double helpers, unsafe inline wide multiply or direct
register access outside BSP. Generated code shows the glyph checkpoints and
bounded policy implementation; per-sample DSP code/RAM allocation is unchanged.
Complete source/artifact SHA-256 values are in build/evidence.json and installer
pins in site/preview.json; the platform build ID does not hash all included DSP.

Checks: bash tools/build-fm1.sh; bash tools/build-emu.sh;
bash tools/test-app.sh (includes bridge/voice-measure/pipeline ASAN/UBSAN checks);
node tests/emu-test.mjs; node tests/site-test.mjs. Policy tests reproduce the
reported timing scale, then verify spike aging/recovery, publication lead,
fixed CPU0 FX pressure, late/CPU0/turn guards, invalid samples, wrap and bounds.
Real-adapter tests verify deferred mutation and duplicate-overload suppression.
All 1188 dry/normal renderer comparisons pass. Native editor-home PNG is
byte-identical to Preview 26 after the glyph split. The emulator exercises
pipeline switching, pitch, panic and return to CPU0; its dry WORK/QUEUE/TURN
timings remain zero. Host APP/FX readings are host values, not device evidence.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target and guards; internal detail snapshot only, persisted/wire ABI unchanged. |
| Audio hot path / numerics | PASS (source/build/host) — bounded 32-bit counters/windows, no new double/libm/allocation/I/O/waits, unchanged per-sample DSP; timing cost on hardware UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/host) — CPU0 policy, acquire-before-snapshot, deferred limits, existing BSP barriers, one-time completion/overload accounting and adapter tests; actual multicore behavior UNVERIFIED for this build. |
| Memory / storage / I/O | PASS (static/build) — snapshot/windows fit linker limits, no storage/peripheral changes; actual stack, checkpoint latency and USB/LCD contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — local header/API additions only; no third-party code/assets imported or notices changed. |
| Compatibility / package / recovery | PASS (build/site) — pinned product/package/loader checks, Preview 26 rollback retained; physical installation/recovery UNVERIFIED. |
| Host functional validation | PASS — full app, sanitizer policy/adapter/bridge/measure, 1188 exact DSP cases, unchanged editor PNG, matching wasm and installer tests. |
| Hardware real-time validation | UNVERIFIED — run the same patch/FX/chord plus GUI, modulation, dense MIDI/Soundmondo and USB recording; record paired delay, WORK/TURN/APP/FX, voices, WHY, LATE/FAULT/XRUN and duration. |

Skipped: physical flashing/recovery/fault injection, target timing, listening,
USB recording, repeated boots and soak. No increased polyphony or reduced GUI
delay is claimed until measured on the device.
