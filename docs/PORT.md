# Port and validation record — 2026-11-20

This records the initial dry prototype. The later effects-enabled build, tighter source matching, revised memory figures and current compliance gate are in [SOUND-MATCHING.md](SOUND-MATCHING.md). Historical build sizes and skipped-effect statements below are superseded by that record.

## Evidence and baseline

`VERIFIED-SOURCE` means inspected source/configuration; it does not imply measured device behavior. `MEASURED-LOCAL` below is explicitly host-only. Hardware observations, installed bootloader, exact chip suffix, recovery path, and actual CPU clock are `UNKNOWN`.

- User-selected source: `RDX_before_UAC`, an unversioned ESP32 snapshot. Individual input SHA-256 values are recorded by the import scripts. No original files were edited. An executable ESP32 baseline was not built; there is no prior FM-1 implementation to compare. Host repeatability is checked within the new implementation, not against a Yamaha or the original ESP32 output.
- fm1-base: `7932dff6f1474cc8c3420e0df75eb75879e47319`; felucca-bsp: `f869dfa8dc3c081f040407e370d303349e4444d5`. Both checkouts remain unmodified.
- Existing WSL Ubuntu, version 1. Host GCC: Ubuntu 15.2.0. JieLi toolchain directory: `jieli-linux-toolchains-20260730.1`; pi32v2 clang reports 4.0.1. Existing installed toolchain archive provenance was not independently rehashed.
- Existing AC79 SDK: `d179b4484759423312073f5fbb232501aa491047`, under `FM1-X0X-mod/.toolchains/AC79_SDK`. All three packaging files passed fm1-base's retained reference hash checks (no mismatch warnings).
- Device build: `bash tools/build-fm1.sh`, calling the upstream builder unchanged. `-target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off`; DSP at `-O2`, unity at `-Os`; DSP additionally uses `-std=c++1z -fno-exceptions -fno-rtti -fno-threadsafe-statics -DRDX_FM1`. No target C++ standard library or heap is required.
- Selected board: standard FM-1 BSP, not SMK-37. No application override of clock, voltage, bootloader, or linker layout. CPU2, radio, USB audio, and persistent app writes are disabled/unused in this prototype.
- `VERIFIED-SOURCE`: `device/main_fm1.c` and `app/app.h` set stereo signed 24-bit samples in `int32_t` words, 44,100 frames/s, 128-frame audio halves and callback length. Nominal service period: 2.902494 ms. Actual cadence, interference, output level and worst service time remain unmeasured.

## Implementation boundary

Hot path: FM-1 audio interrupt → `app_render` → bounded panel-event/MIDI consumption → `rdx_render` → `RdxEngine::renderBlock` → voice modulation/routing → operator phase/feedback/sine lookup → amplitude/pitch envelopes. Envelope stage transitions are included in this audit. Program-change patch commit and deferred events are also bounded interrupt work and must be included in device timing measurements.

The original ESP32 task scheduling, GUI, I2S, PSRAM effects, filesystem, program-change atomics and adaptive CPU controller are not imported. One engine owner processes all synth mutations in the audio callback. Boot constructs it in fixed storage. `app_panic` excludes interrupts using the existing platform API while clearing the engine and event queue. No multicore protocol is introduced.

The UI publishes through a 32-entry ring under `plat_lock`/`plat_unlock`; the interrupt consumes at most 32 panel events and 32 MIDI packets per block. Full panel rings request panic and discard the batch. A separate bounded 32-entry note-event buffer preserves ordering after program changes during the outgoing fade; overflow panics and clears it. No spin/wait loop is used. PRESET's shared UI value is aligned, volatile and updated from the main loop under the same interrupt exclusion; the renderer never waits for the UI. MIDI accepts channel 1; disconnect/active-sensing timeout handling is still missing.

Changes to inherited DSP:

- Fix `fast_floorf` reading its own uninitialized local; wrap oscillator phase even when its increment exceeds a full cycle.
- Clamp interpolation endpoints, including modulation/portamento extremes, to keep adjacent table reads in bounds.
- Replace sample-path envelope `powf` calls and compiler-specific constexpr libm evaluation with offline-generated single-precision tables. Control-frequency/detune formulas also use tables.
- Remove logging and the invalid-algorithm sleep; reject invalid patches before mutation and fail invalid routing silent.
- Handle equal zero-level envelope stages without dividing by zero; ensure mono glide voices release on note-off.
- Remove Arduino/ESP attributes and incidental headers. Plain aggregate arrays preserve constexpr tables without a target C++ standard library.

These repairs and interpolated envelope curves can change sound relative to the source. Aliasing and analog/Yamaha fidelity have not been characterized. The provisional four-voice ceiling is a test configuration, not a performance finding. Source sample-and-hold PRNG behavior is retained; the bit-repeatability test covers the synthetic init voice, not all random LFO histories.

Offline preset import validates frame identity, seven-bit data, frame bounds, checksums and complete common/four-operator blocks. It retains the packed 38/28/150-byte layouts with static assertions. This is not a live SysEx receiver. No new compiler enum layout is serialized.

## Validation

`MEASURED-LOCAL`, host only:

- `bash tools/test-host.sh`: PASS using `-O2 -ffp-contract=off -fsanitize=undefined,bounds,float-cast-overflow -fno-sanitize-recover=all`. Covers lookup endpoints, malformed patch rejection, uninitialized silence, all 12 algorithms, extreme modulation/high notes, four-slot stealing stress, sustain/release in all three modes, exact panic silence, init-voice bit-repeatability, and renders of all 33 imported patches. Every checked sample is finite, bounded and identical between left/right. Host engine object: 3,664 bytes (not total firmware RAM).
- `bash tools/test-app.sh`: PASS. Real app logic in upstream simulator; panel key edges, octave change, PRESET and MIDI program changes, invalid program rejection, MIDI note audio, panic silence, channel filtering, same-block program-change/note ordering and zero app store writes. See `build/app-test.log`. Simulated duration: 2,450 ms; screenshots and `digichord.wav` under `build/app-smoke`. Host CPU percentage is not device timing evidence.
- `bash tools/build-fm1.sh`: PASS. See `build/target-build.log`. Target image: 152,096 bytes; `.ram_text`: 5,100 / 32,768 bytes; `.data + .bss`: 17,328 / 98,304 bytes; pool: 23,044 / 344,064 bytes. These are separate linker regions; do not interpret them as a single available heap. Update loader: 12,903 bytes; package: 616,059 bytes. Runtime peak stacks/pools remain unmeasured.
- Retained upstream checks: no software-double helpers; no flagged interleaved wide multiply/load; RAM code call closure; register access confined to BSP; image and linker-region limits. The synth runs from flash/XIP in this version, not in the 5,100-byte RAM code region. Its cache/timing behavior needs target measurements.
- Screenshot inspected: title, development status and selected voice render without clipping.

Skipped: ESP32 reference build/render, Yamaha/device sound comparison, AddressSanitizer, live SysEx malformed-input testing (feature absent), queue-overflow fault injection, interrupt preemption simulation, USB/TRS hardware input, disconnect recovery, effect compatibility, save/load migration, installation/rollback, and all device timing/soak/analog tests. No skipped check is a pass.

## Mandatory compliance gate

| Category | Status | Evidence / remaining limit |
| --- | --- | --- |
| Target / build / ABI | PASS for the build | Actual pi32v2 toolchain and matched SDK; linked/package build and retained upstream checks. Installed unit/bootloader ABI remains untested. |
| Audio hot path / numerics | PASS for structural/build/host checks | Entire affected renderer/envelope path reviewed; bounded loops/tables; no allocation, I/O, logging, waits, runtime libm or software doubles. Division, XIP and overall cost require target measurement. |
| Execution ownership / synchronization | PASS for source review | Audio owns synth, bounded rings and interrupt-excluded publication; boot/panic contexts identified. Physical interrupt stress/fault injection remains unverified. |
| Memory / storage / I/O | PASS for static build; UNVERIFIED for runtime | Linker budgets passed, fixed lifetime storage, bounded events. No app flash writes. Peak stack/pool and real peripheral contention unmeasured. |
| Dependencies / includes / licenses | UNVERIFIED overall | Platform/BSP notices and license files retained, no radio libraries added. Original RDX and voice-asset licensing is not established; SDK/font terms recorded upstream. No release license is invented. |
| Compatibility / package / recovery | UNVERIFIED overall | Existing packager/loader/layout retained and package generated; packed voice layout asserted. Provisional identity, no migration, no installed-device backup/recovery evidence. |
| Host functional validation | PASS for the listed scope | Sanitized core suite and complete-app smoke scenarios, as above. Remaining skips explicitly listed. |
| Hardware real-time validation | UNVERIFIED | No FM-1 timing, underrun, stack, soak, output or recovery measurements. Compilation/simulation does not qualify the firmware. |

## Hardware test plan and next development work

Before any installation, identify the unit and installed bootloader/firmware, export and verify its data, obtain a matching rollback image and confirm the recovery procedure for that exact setup. The build does not install anything. The development package identity `FM-1_9900` must not be treated as a reserved public identity. The placeholder words URL is not a hosted screen-reader vocabulary.

Measure the complete 128-frame service against 2.902494 ms, including event bursts, patch commits and interrupt interference. Initial engineering acceptance target: observed maximum service ≤80% of that interval (2.322 ms) and zero deadline misses/underruns during the specified run; this is a project target, not a measured guarantee. Run every source patch with four held voices, sustained/releasing voices, stealing, rapid preset changes, dense USB/TRS MIDI and continuous UI activity. Record test duration, maximum latency/jitter, deadline misses, stack/pool high water, clipping/DC and master-gain transitions using the platform's bounded counters, reported outside the renderer. Include at least a 30-minute stress run, a two-hour soak and repeated power cycles. If four voices miss the margin, profile before adjusting voice count or placement; do not change clock or bootloader speculatively.

Then port pitch bend/mod wheel and the parameter editor/CC map, design an FM-1 memory budget for each effect, add validated preset import/export and persistence, and evaluate velocity/pressure, USB audio and BLE individually. Live hardware and source-sound comparisons are required before claiming feature parity or a hardware-qualified release.
