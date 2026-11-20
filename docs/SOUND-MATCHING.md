# ESP32 sound-matching correction — 2026-11-20

Follow-up on 2026-11-21: [envelope timing and readable GUI](ENVELOPES-AND-GUI.md) supersedes the mono-release repair and GUI/build-size statements below. The earlier port forced mono/glide note-off to release early; that behavior is now restored to the ESP source and explicitly tested.

The user confirmed that ESP32-S3 and ESP32-P4 sound the same apart from polyphony, and that ESP effects are enabled. The supplied YouTube short shows **WobbleBass** on the instrument display: https://www.youtube.com/shorts/Vu2NzcPyU-I. Its source patch selects distortion (92,56), then delay (40,5). The first prototype omitted both. The video was used to identify the patch visually, not as a decoded digital-audio or note-event reference.

## What changed

All seven source effects are now adapted, in their original two-slot order: distortion, touch wah, chorus, flanger, phaser and its internal flanger, delay, and reverb. Thru remains available. The existing firmware and emulator share this effects implementation. The panel FX button toggles bypass and the LCD shows FX ON/OFF; the browser was left on WobbleBass with FX ON.

The internal mix coefficient now retains the S3 reference value, `0.2f / sqrtf(8)`, independently of the port's four-voice ceiling. This preserves drive into nonlinear effects. Generated tuning, detune, gain and sine tables now respect the source's float rounding boundaries, rather than evaluating all intermediate expressions as JavaScript doubles. Envelope segment endpoints use a denser 1/16-level lookup for the source exponential formula, replacing the coarse interpolation introduced by the initial port. Bounds fixes and the source-floor uninitialized-variable repair remain. Final PCM conversion clamps to full scale, as the ESP I2S converter does.

No runtime libm, allocation, logging, filesystem access or waits were added to audio. Distortion tone and reverb feedback coefficients are generated offline. The source effect headers and their notices are retained through `tools/import-effects.mjs`; `effects-lock.json` records every input SHA-256.

## Memory and ownership

The audio interrupt remains the sole synth/FX owner. New hot path: `app_render` → bounded event/MIDI handling → `rdx_render` → synth, soft clip, slot 1, slot 2, program fade → full-scale clamp/master/PCM. Effects use float arithmetic and fixed-lifetime storage. Boot explicitly constructs the effects in fixed storage; no C++ global constructor support is assumed on FM-1. Effect dispatch is an explicit switch, without vtables.

The shared arena contains 82,432 floats (329,728 bytes) in the BSP pool. For a static delay tap, the ring needs only the selected tap plus one sample per channel: changing ring capacity does not change the delayed sample's time or feedback. The source's one-second tap clamp and its cached-value-64 reset behavior are preserved. No quantization/downsampling is introduced. Reverb uses the full nominal stereo topology for every supplied patch; it is not scaled down to fit.

All 33 source combinations fit. DynaLead is the largest, at **82,410 floats**. Other future long-delay combinations can exceed the arena: preparation rejects them and the renderer remains silent with `FX MEMORY LIMIT`. It never silently reduces their delay time. The present public program-change interface selects only the supplied bank.

Arena clearing is limited to 4,096 floats per callback; notes remain frozen during preparation. At most 21 such blocks are needed, about 61 ms nominally, plus fades. New program changes can replace a preparation in progress. Panic clears synth state and schedules fresh zeroed FX state so tails cannot restart. Each patch load starts fresh FX state, including when slot IDs match; this differs from the ESP host's retention of tails for unchanged slot IDs. The preset and deferred-note queues remain bounded at 32 entries each, with panic/discard overload behavior. No flash writes occur.

The LCD now draws text in 8-row strips to leave the required pool spare intact. Its rendered text was checked visually. No clock, bootloader, flash layout, linker regions, second-core scheduling, or upstream size/integrity check was changed.

## Baseline and comparison method

The previous `.fwsc` and wasm are retained under `build/before-effects`. Platform/BSP/toolchain/SDK revisions remain those recorded in PORT.md: fm1-base `7932dff6f1474cc8c3420e0df75eb75879e47319`, BSP `f869dfa8dc3c081f040407e370d303349e4444d5`, existing pi32v2 clang 4.0.1/r3/FPU and AC79 SDK `d179b4484759423312073f5fbb232501aa491047`.

`bash tools/test-sound.sh` builds a separate host reference from the original ESP snapshot. Only host/platform/diagnostic adaptation and correction of the undefined `fast_floorf` local are applied to that reference. It is **not an ESP hardware recording**. Reference rendering uses four voice slots for a controlled comparison and the S3 gain coefficient. Both builds use `-ffp-contract=off`.

Each patch is compared over 98,304 stereo frames (about 2.23 seconds). Dry tests use the same MIDI note 60/velocity 100 with its patch routing/modulation. Effects tests isolate the chain with identical deterministic stereo input followed by silence, checking actual source effects output rather than just checking that the port sounds. Reference effects start from a fresh load and full nominal buffers. The test does not establish compatibility with every parameter history, active ESP build flag, source-reverb memory scaling, or the user's analog output.

Acceptance thresholds: dry relative RMS error below `1e-5` and effects maximum absolute float sample error below `1e-6` for all 33 supplied patches. Results in `build/sound-comparison.json`:

| Comparison | Worst result | WobbleBass |
| --- | --- | --- |
| Dry relative RMS | 5.52e-6 (0.000552%) | 5.73e-8 (0.00000573%) |
| Effects maximum absolute error | 5.22e-8 | 2.98e-8 |

These are controlled host/source results, not proof of perceptual equivalence or FM-1 real-time safety. Source sample/hold random-state histories, high-note phase-wrap fixes, mono-release repairs, voice stealing beyond four voices, and live parameter sequences need separate comparison.

## Verification

- `bash tools/test-host.sh`: PASS, existing sanitized core bounds/routing/voice/release/preset tests after the float/gain changes.
- `bash tools/test-effects.sh`: PASS with AddressSanitizer, undefined-behavior and float-conversion sanitizers; all 33 chains, all eight effect IDs including Thru, parameter extremes, arena canaries and explicit budget rejection. Leak detection disabled; no heap is used by the effects implementation.
- `bash tools/test-sound.sh`: PASS against the thresholds above; raw renders, JSON and log retained under `build`.
- `bash tools/test-app.sh`: PASS, including FX on/off/on, panel input, program changes/MIDI order, panic silence and zero app storage writes. Simulated duration 2,670 ms. Host CPU percentages are not FM-1 measurements.
- `node tests/emu-test.mjs`: PASS, actual WebAssembly/worklet tests for all 33 presets, finite/bounded audio, panic, and WobbleBass FX toggle. Mono listening samples: `build/wobble-wet.wav` and `build/wobble-dry.wav`. These are generated test-note renders, not captures of the YouTube clip.
- Browser: visibly switched on, selected WobbleBass, verified FX OFF then ON, and checked all LCD text after strip rendering. Screenshot: `build/emulator-effects.png`.
- Target: PASS with the retained manifest/size/register/RAM-code/software-double/wide-multiply checks. Image **175,316 bytes**, `.ram_text` **5,100 / 32,768**, `.data+.bss` **19,044 / 98,304**, pool **333,572 / 344,064 bytes** (10,492 spare; the 8,192-byte requirement is retained). Loader 12,903 bytes; package 616,059 bytes. Explicit target symbol audit found no `__cxa`, global-constructor or software-double matches. Sources/artifact hashes: `build/evidence.json`.

## Compliance gate and limits

| Category | Status | Evidence / remaining limit |
| --- | --- | --- |
| Target/build/ABI | PASS for build | Actual pi32v2 build and retained integrity checks; installed-device ABI/bootloader remains unverified. |
| Audio hot path/numerics | PASS for source/build/host scope | Bounded clearing and loops, source comparisons, no runtime libm/doubles/vtables/heap; physical timing/division/XIP costs unmeasured. |
| Ownership/synchronization | PASS for source/host scope | Audio owns synth/FX; unchanged bounded publication/queues, no worker/wait; real interrupt stress unverified. |
| Memory/storage/I/O | PASS statically and under host sanitizers; runtime UNVERIFIED | All supplied effects fit; canaries/sanitizers/budget rejection and retained pool spare pass. No flash writes. Device stack/pool peaks unmeasured. |
| Dependencies/includes/licenses | UNVERIFIED overall | No new external runtime dependency; source notices retained, including MIT notices in delay/chorus. Overall RDX/patch licensing remains unresolved. |
| Compatibility/package/recovery | UNVERIFIED overall | Existing packager/loader and 150-byte voice format retained; no data writes/migration. Patch tail-reset behavior differs as described. No installation, backup or rollback test. |
| Host functional validation | PASS for listed cases | Core, FX sanitizers, source A/B, app and wasm/worklet tests; browser controls visually checked. |
| Hardware real-time validation | UNVERIFIED | No unit flashed or measured. Must repeat PORT.md's timing, stress, analog output and recovery plan with both FX slots active. |

Skipped: subjective listening assessment, digital recording of the user's ESP hardware, matched note sequence from the video, physical FM-1 output/MIDI, exact S3/P4 compiler-output comparison, cross-browser/mobile and alternate sample-rate tests, queue/interrupt fault injection, runtime stack measurements, installation/rollback and soak. The build is review-ready and host-tested, not hardware-qualified.
