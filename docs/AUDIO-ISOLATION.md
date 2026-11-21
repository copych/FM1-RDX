# Audio/control separation — local Preview 2026.11.22-21

Experimental, host-tested firmware on branch codex/audio-control-isolation.
Baseline checkpoint: 0cb6d73, Preview 20. No publishing or device flashing by agent.
The local checked installer is `/install/`; previous Preview 20 installer is
retained at `/install-preview20/` for rollback. Persisted objects and loader
identity are unchanged. Close the installer before MIDI/Soundmondo testing.

## Actual boundary

CPU0 audio interrupt now consumes up to 32 **prepared commands** per block,
applies staged system/patch changes, generates voices/FX, applies output gain,
fills audio output and observes the existing overload budget. It never dispatches
or waits for CPU1. No additional audio buffer or block of latency is introduced.
All sample-rate envelope/LFO/glide/feedback/numerical behavior remains unchanged.
Engine sources/flags are unchanged from checkpoint 0cb6d73; note/parameter
application and patch/FX activation still belong to the audio owner. Preparation
of all note-independent engine coefficients/FX reconfiguration is NOT newly
offloaded in this build; structural parameter changes can still cost audio time.

CPU1 owns the application's button/encoder interpretation, acceleration, menu
state, MIDI decoding/channel filtering, Reface/Soundmondo SysEx parsing and
validation/reply construction, editor bitmap/scaling, font drawing, canvas and
application LCD submission. Only snapshots and prepared replies cross the
boundary. Parameter/patch changes are staged; CPU1 never reads/writes live voices,
effects or working patch. The graphical code uses the same original drawing
functions and fonts; no visual approximation or audio-rate reduction.

CPU0 still owns hardware audio/USB/input interrupt handlers, raw input capture,
draining USB MIDI packets into request batches, USB transport/reply-ring service,
updater/storage/platform system screens and the main-loop handoff. Peripheral
interrupt affinity has not been modified. This is application workload isolation,
not a claim that CPU0 runs nothing other than audio or that its idle time is 100%.

## Synchronization and ownership — VERIFIED-SOURCE

There is one control job at a time. CPU0 captures raw keys/buttons/encoder deltas
and up to 32 MIDI packets, plus engine/status/profile snapshots under its local
interrupt lock. It releases the lock before hashing/dispatch. CPU1 validates a
sequence/checksummed request and produces up to 96 commands and bounded patch/
system staging buffers. SysEx request limit remains 640 bytes. Reface transaction
state belongs to the serialized worker; USB TX servicing occurs after completion
on CPU0, outside audio. The old live-state helpers route worker calls to snapshots.

Worker-side control callbacks deliberately use snapshot reads and no-op local
locks: disabling CPU1 interrupts would not protect the CPU0 synth. After the BSP
completion barrier, CPU0 checks sequence, valid flag, count and reply checksum;
only then publishes commands/staged state under CPU0 interrupt exclusion.
The SPSC audio queue is now 128 entries. On insufficient space/stage overflow,
the batch is rejected and panic requested; the next audio block clears queued
events and silences voices, preventing a dropped note-off from leaving stuck notes.
Normal decoding order, note-on velocity zero, bend, CC and bank/Init mapping are
retained; control channel selection is sampled once per batch rather than read
from the live engine for every MIDI packet.

CPU0 **main loop** waits for each job before resuming other platform operations.
This serializes LCD/font/canvas and protocol buffers with platform system screens,
updates and USB TX servicing. Audio IRQs remain enabled and do not wait. The
main-loop completion polling is bounded by a free-running 24 MHz timer to 100 ms,
then the unchanged BSP wait/retirement routine can take its existing 8 ms timeout.
Polling includes a fixed 128-iteration pause between mailbox checks, not an audio
spin wait. The platform watchdog and other IRQ behavior are unchanged.

This choice prioritizes a reviewable ownership boundary, NOT fully independent
core scheduling. CPU0 main-loop USB/control delivery can be delayed for the
duration of a GUI/LCD job (potentially more than one audio block). It must be
measured under dense MIDI and heavy redraws. Busy polling and shared cache/memory
traffic can also affect audio; no speedup is asserted from source inspection.

On startup failure, work runs locally on CPU0. After a guarded worker fault,
timeout, invalid reply or bad checksum, CPU0 first has a completed/held worker,
drains outstanding LCD DMA, reconstructs GUI state, resets protocol state,
requests audio panic and uses CPU0 controls until reboot. Failed replies are
invalidated and never committed. GUI mode switching also permits deliberate
CPU0 control comparison. Worker code cannot redispatch its own core: the old
nested GUI job path is compiled out in this application.

LCD ownership is serialized through the main-loop job boundary. Board source
fm1_lcd_hw.h uses SPI1/PC7/PC8/PC9/PC10 for transfers; the input matrix/LED scanner
uses PA/PB/PH. No LCD backlight, pin setup or SPI clock change is made by CPU1.
Canvas blits explicitly invoke the BSP memory barrier before starting DMA reads
of CPU1-written pixels. Worker clears use bounded 8-row canvas strips with that
same publication path. Existing LCD waits have a finite 4,000,000-iteration
timeout. Hardware peripheral visibility from CPU1 remains a validation item.
Existing BSP flash gates and guarded start/check-word/fault retirement are retained.

The old fourth diagnostic page now performs local idle/busy checksum checks on
the selected control core; it no longer submits a competing asynchronous worker.
Its historical ROUND/LATE counters are not meaningful for this architecture.

## Hardware procedure — UNVERIFIED for this build

After installing Preview 21, test panel notes, rapid encoder changes, program
changes, sustain/bend/controllers, Soundmondo send/receive and USB audio recording.
Keep the same preset/FX/chord as the baseline. Six is still the budget's starting
and maximum admission count; adaptive shedding remains enabled.

Hold SEL and tap REC five times for **AUDIO ISOLATION**. It reports CTL CPU1,
completed jobs, control errors, BSP faults, maximum control job time, CPU1 stack
high water, APP/SYN maxima, XRUN, decoded MIDI count and rejected command batches.
SELECT toggles CPU1/CPU0 control for comparison. SAVE resets audio/cost and
control workload counters; HOME returns to normal GUI exercise. Return after
30–60 seconds of notes and GUI changes, then record all rows. Any ERR/FAULT/DROP
or XRUN needs investigation. Stack allocation remains 8192 bytes plus the 256-byte
exception stack. Run repeated boots and a longer soak before qualification.
Audio Timing (SEL+REC once) retains FX/6V prediction and actual/admitted voices.
CTL time includes the whole input/UI/protocol job, not just editor rasterization;
it is not directly comparable to Preview 19/20's GUI-only number. Host timing and
host's simulated CPU1 are not FM1 evidence.

Previous user-device baseline: Preview 20 APP 2297 us, SYN 1971 us, FX 341 us,
4/4 admitted despite ceiling 6, six-voice prediction 3467 us. Those are user
reports, not agent-reproduced measurements. Moving controls is not proof of six
voices fitting; the unchanged synth's per-voice cost can remain the bottleneck.

## Build, tests and artifact identity

Unchanged fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP
f869dfa8dc3c081f040407e370d303349e4444d5; existing AC79 SDK and pi32v2 clang
4.0.1, r3/fprev1, engine/editor O2, FP contraction off, app/platform Os.
44.1 kHz, 128 frames, 2902.49 us deadline; same clocks, voltage and memory map.

PASS: `bash tools/build-fm1.sh` retained ABI/manifest, RAM closure, register,
no-software-double/64-bit-load hazard, image and loader/package guards.
Image 189496 B; RAM code 20684/32768 B; data+bss 61916/98304 B;
pool 333572/344064 B. Existing FX/GUI warnings and old compact-C host style
warnings remain. No new external libraries, assets or allocations; control
requests, replies, expanded queue and buffers have fixed static bounds.

PASS: `bash tools/test-app.sh` includes original bank/editor/acceleration/manual,
benchmark/profile, six-voice budget, Soundmondo protocol tests and new control-
core/MIDI/fallback scenario. Host checks demonstrate controls execute with
simulated plat_core()==1 and deliberate CPU0 fallback, no errors or stores.
`node tests/emu-test.mjs` on the matching wasm passes all 33 presets, finite audio,
FX, pitch/controllers, panel/encoder behavior, profile transitions and panic.
Updated isolation screenshot inspected. Host CPU1 executes synchronously and
does not validate actual parallel hardware execution.

PASS: `cc -std=c11 -O1 -g -fsanitize=address,undefined
-fno-sanitize-recover=all tests/control_bridge.c -o build/control-bridge-test`
and its executable test the actual request/decoder/commit code with deferred
mocked completion: core-ownership assertions, MIDI command ordering/bend,
channel filtering, full-queue panic, staged SysEx system publication, corrupted
reply rejection, bounded timeout/held-worker recovery and CPU0 fallback.
Initial mock compilation required correcting external/static declarations and
forward declarations; the final sanitizer run passes. This is protocol evidence,
not a cache/peripheral concurrency test.

Target disassembly retained in build/isolation.dis: app_render calls prepared
event application, patch/system activation, rdx_render and budget observation;
it no longer calls plat_midi_in, a parser, GUI/LCD, CPU2 dispatch or a wait.
DSP source diff against 0cb6d73 is empty. Pinned installer tests/site-test.mjs
and git diff --check pass. Full source/artifact hashes: build/evidence.json.

Firmware SHA256: 655e04a8c1fcfc16e64c8c8e94ac4d0c45d62684b1d6b110103b31e84b06787a.
Wasm SHA256: eeef77556162b5fe0298d1632165d49540065a867eb9ecd379673a28b4f4b83c.
Build ID 88918BA5. Same checked loader SHA
f0da7398b234c99c7916ce123d1069d71abc02242f243c0c7976636f656a50ba.

## Compliance gate

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target build, source pins and original guards; no SDK/BSP changes. |
| Audio hot path / numerics | PASS — bounded prepared-command consumption, no worker/GUI/parser waits in callback; unchanged DSP and sample update rates; target helper guards/disassembly inspected. |
| Execution ownership / synchronization | PASS for source/host scope — serialized worker, immutable capture/checksummed reply, CPU0-only engine/queue publication, bounded main-loop collection and held-worker fallback. Actual cross-core/DMA behavior and latency UNVERIFIED. |
| Memory / storage / I/O | PASS for static/build scope — fixed buffer bounds and linker budgets, exclusive LCD/canvas owner with publication barrier, unchanged storage/clock/pins. Physical CPU1 peripheral operation, interrupt-mask latency and stack peak UNVERIFIED. |
| Dependencies / includes / licenses | PASS — no new libraries/assets, original notices/licenses retained. |
| Compatibility / package / recovery | PASS for build/site scope — same identity/loader/storage, checked pinned installer and Preview 20 rollback retained. Real flashing/recovery UNVERIFIED. |
| Host functional validation | PASS — full app, real wasm/worklet and bridge ASan/UBSan suites as above; host simulated concurrency limitations stated. |
| Hardware real-time validation | UNVERIFIED — measure audio/command latency, contention, faults, stack, dense MIDI plus LCD/USB and soak before qualification. No speedup/polyphony claim. |

Skipped: repeated DSP numerical equivalence/effects sanitizer suites (DSP sources
unchanged), hardware flashing, CPU1 LCD/DMA/concurrency validation, analog/USB
listening/recording, forced physical faults, boot/soak and worst-case measurements.
