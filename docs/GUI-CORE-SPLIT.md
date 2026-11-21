# GUI worker — Preview 2026.11.22-19

Local, host-tested hardware experiment. Not published or flashed by the agent.
This is the first actual GUI offload, not the complete requested control-core
architecture: input polling/decoding, event publication, MIDI/USB, peripheral
drivers and font/LCD strip emission remain on CPU0. CPU1 generates the original
128x64 editor image, scales it to 240x120 and captures its dynamic labels.
Synth, envelopes, modulation, effects, audio ISR and DMA submission remain CPU0.

## Hardware comparison

Install from local `/install/` (Preview 19). Single-core Preview 17 is retained at
`/compiler/o2/` for rollback via its pinned installer. Close installer MIDI ports
before testing MIDI or Soundmondo. No bootloader or persistent format change.

Hold SEL and tap REC **five times**, then release SEL: GUI CORE SPLIT.
SELECT toggles CPU1 GUI / CPU0 GUI. Default is CPU1. SAVE resets audio/GUI
counters. HOME returns to the normal screens to exercise real rendering.

For each mode, reset on this screen, return HOME, hold the same four notes on
the same preset/FX, and move knobs/change GUI pages for at least 30 seconds.
Return to GUI CORE SPLIT and record every row. In CPU1 mode, UI CPU1 and
increasing JOBS confirm actual offload; ERR/FAULT/XRUN should remain zero.
Repeat in CPU0 mode with comparable gestures. This experiment adds no audio
buffer or sample-rate change. An asynchronous GUI frame can be one frame stale.

GUI is the highest worker/local GUI computation time in microseconds (not the
CPU0 collection round trip used in Preview 18). STK is the BSP's CPU1 stack
high-water reading in bytes; allocation is 8192 bytes plus its 256-byte exception
stack. A CPU0 GUI run also updates GUI/JOBS, with UI CPU0. Diagnostic pages do
not generate editor jobs, so exercise the normal GUI before reading results.
APP and SYN retain maximum audio timing since SAVE; XRUN is a delta since SAVE.
Host/browser cores are simulated; their timing is not FM1 evidence.

## Source baseline and ownership

Unchanged fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP
f869dfa8dc3c081f040407e370d303349e4444d5. Existing pi32v2 clang 4.0.1,
AC79 SDK d179b4484759423312073f5fbb232501aa491047; r3/fprev1, FP contraction
off, O2 engine/editor. 44.1 kHz, 128-frame half blocks, deadline 2902.49 us.
No clock, voltage, interrupt bank, audio routing or platform API changes.

Preview 18 user-device reports: OFF/IDLE/BUSY APP 2291 us and SYN 1970–1971 us,
zero detected errors/faults/XRUNs across short runs. Those were user-reported,
not agent-instrumented measurements, and do not qualify this GUI build.

CPU0 takes a complete EditorSnapshot under its existing interrupt exclusion.
One immutable request (including page/operator/tune/sequence) is submitted via
the existing guarded BSP job protocol and its csync barriers. CPU1 copies and
checks the request before accessing its GUI-only state, creates a packed 3600-
byte image and four bounded labels, and publishes a sequence/checksummed reply.
The GUI implementation depends on its snapshot, RAM display and constant assets;
it does not access live DSP state, platform canvas, LCD, USB or input drivers.

CPU0 polls completion in app_loop/app_frame and checks the reply only after
the BSP completion barrier. It never reuses the mailbox while the job owns it.
LCD conversion/transfer uses the completed image. Newer patch revisions trigger
a subsequent frame; page/operator/tuning changes discard incompatible frames.
Neither worker collection nor any wait was added to app_render/app_overload.

An outstanding job is checked after 100 ms CPU0 elapsed time; main-loop forced
collection uses the unchanged BSP bounded wait/retirement API (up to its 8 ms
timeout, interrupts enabled). Completion/fault retirement precedes GUI reuse.
On rejected/corrupt output or retired worker, GUI state is reconstructed after
the worker has finished/been held and subsequent frames run on CPU0 until reboot.
Startup fallback also renders locally. Input continues polling while a GUI job
runs. Before main-loop update/recovery panic and UI reinitialization, outstanding
GUI work is collected/retired; synthetic work is also drained before update.
Existing flash gates forbid flash operations while CPU1 owns a job.

The synthetic test and GUI use the same worker exclusively: no new editor jobs
on diagnostic pages; synthetic dispatch waits for GUI collection. GUI fallback
can run locally while a synthetic job drains because that job has no GUI state.
Reported shared-RAM faults on some FM1 units remain a hardware limitation;
checksums/BSP guard words detect some faults, not every possible corruption.

## Validation and artifact identity

PASS: complete `bash tools/test-app.sh`, including new GUI core-switch scenario
and existing bank, editor, acceleration, MIDI/Soundmondo, profile, benchmark,
synthetic-core and voice-budget controls. New ASan/UBSan test:

```
g++ -std=c++17 -O2 -ffp-contract=off -fsanitize=address,undefined \
  -fno-sanitize-recover=all tests/gui_worker.cpp app/editor.cpp \
  -o build/gui-worker-test
build/gui-worker-test
```

It compares every scaled pixel and label for all 21 pages x 4 operators x 5 tune
positions (420 combinations); tests deferred completion/no early collection,
bad request checksum, corrupt output, wrong sequence, timeout retirement and
CPU0 fallback. Mocked jobs defer execution but are not a hardware concurrency
or cache-coherence test. Existing host and wasm functional tests cover real
factory patches; the 420-combination test uses a deterministic synthetic patch.

PASS: target build and all retained RAM closure, register, double-helper,
manifest, loader/package guards. Image 188376 B; RAM code 20684/32768 B;
data+bss 59196/98304 B; pool 333572/344064 B. CPU1 stack enlarged through the
supported cpu2_stack setting from 2816 to 8192 bytes, not an SDK patch.
Existing unused GUI variables and FX constructor conversion warnings remain.
GUI graph uses existing bounded formatters and static assets; no allocations
except existing placement construction into static GUI storage during boot/reset.

Engine object SHA256 is **identical** to Preview 17/O2:
`3abec4324cb8292ad3a24bcd3be6bf417da983d06cbba318ce422b3809016362`.
This establishes unchanged compiled DSP, not unchanged hardware interference.
Target disassembly retained in `build/gui-split.dis`.

PASS: matching `bash tools/build-emu.sh` and `node tests/emu-test.mjs` (all 33
presets, finite audio, panel controls, FX, bend/controllers, profile transitions,
panic); GUI worker host screenshot inspected. `node tests/site-test.mjs` checks
the actual pinned installer/package/links, corruption rejection, unsupported
browser and no automatic MIDI/bootloader offer. `git diff --check` clean.

Firmware SHA256: `9ee09581cb03666555152f3796dbe15eaae7565648598681b3f9bfc2d06e4ce0`.
Wasm SHA256: `47ea8779f5a6e35478500b3eea99ff54eee9cf0e549709090412a393a0e04c5d`.
Build ID 02107BAC; full source/artifact hashes in `build/evidence.json`.

## Compliance

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — pinned foundation/SDK, target build and existing ABI/build guards; supported CPU1 stack configuration. |
| Audio hot path / numerics | PASS — unchanged engine object and callback; no audio waits, new helpers or numeric changes. |
| Execution ownership / synchronization | PASS for source/host scope — exclusive immutable request/reply with BSP barriers, explicit retirement, fallback and update quiescence. Hardware concurrency/latency UNVERIFIED. |
| Memory / storage / I/O | PASS for static/build scope — linker budgets and image checks, bounded image/labels and independent GUI state; no storage format or peripheral ownership changes. Target stack peak/flash contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — no new external dependencies, assets or platform edits; existing notices/licenses retained. |
| Compatibility / package / recovery | PASS for build/site scope — same identity, loader and storage, pinned checked installer and local rollback available. Actual installation/recovery UNVERIFIED. |
| Host functional validation | PASS — full app suite, 420 pixel/label comparisons with sanitizers, deferred/fault scenarios, matching wasm and installer tests. |
| Hardware real-time validation | UNVERIFIED — run CPU0/CPU1 comparisons, maximum stack, repeated boots, rapid edits, USB recording/MIDI/Soundmondo and a longer soak. No speedup/polyphony gain claimed. |

Skipped: repeat DSP equivalence/effects sanitizer suites (compiled engine
unchanged); physical flashing, analog/USB audio listening/recording, real fault
injection, timing/cache measurements and soak unavailable to the agent.
Remaining architectural work: input/menu/control computation and suitable I/O
processing still need separate snapshots/queues before moving to CPU1. CPU1
placement of this GUI work must not be described as the entire control-core split.
