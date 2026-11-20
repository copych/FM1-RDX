# Envelope timing and readable GUI — 2026-11-21

GUI milestone below is historical. The subsequent [original ESP GUI restoration](ORIGINAL-GUI.md) replaces its playback screen with 19 editor pages and panel bindings; the envelope results remain current.

The user reported that sound was much closer but still distinguishable, suspected envelope timing, and could not read the small GUI font. This change addresses both without tuning rates by ear or altering the original ESP snapshot.

## Envelope findings and correction

`tools/test-envelopes.sh` builds separate host executables against the original-source reference and the port. It tests 33 presets × four operators × three mono/poly modes × three gate durations (44, 1,323 and 13,230 samples: about 1, 30 and 300 ms). Each case observes the hold and another 176,400 samples (four seconds) after note-off. It records every amplitude-envelope and pitch-envelope stage transition at its exact sample and compares amplitude/pitch checkpoints every 2,048 samples. The patch's portamento setting is retained when exercising the three modes. These are 1,188 cases, not an exhaustive sweep of every possible parameter/retrigger history.

Before the correction, amplitude transitions differed in 44 cases, all belonging to WobbleBass and MoDemLead in mono/glide modes. Pitch-envelope transitions already matched in every case. The importer had introduced immediate release on mono/glide note-off. In the ESP source, gate-off lets the current segment finish and its existing `advanceStage()` chooses release afterward; sustain enters release on its next processed sample. The source behavior is now retained. For example, WobbleBass operator 2 with a very early note-off waits until sample 32,252 to enter release in the reference; this is intentional source behavior, not a rate to accelerate.

After removing the forced-release branch, six amplitude cases still ended release one sample late. The remaining discrepancy came from the port's reordered inverse-gain expression. It now uses the source order, `value / LEVEL_LUT_MAX * 191.0f`, with the existing bounds guards retained.

Final result: **zero differing amplitude or pitch transition sequences in all 1,188 cases**, with maximum checkpoint amplitude error **4.77e-7**. `build/envelope-before.json` preserves the initial mismatch summary; `build/envelope-comparison.json`, the two CSV traces and `build/envelope-test.log` contain the corrected result. The checker asserts exact transition sequences and a checkpoint error below 1e-5. Reference construction remains described in SOUND-MATCHING.md: a host adaptation of the snapshot, not an ESP32 capture. No claim is made about unobserved tails beyond the bounded window, every retrigger sequence, or all hardware/compiler configurations.

Only two DSP behaviors changed: restored source gate handling and inverse-gain evaluation order. Rate tables, sample rate, stage increments, effects, voice capacity and gain are unchanged. `tools/import-core.mjs` owns both corrections so regeneration preserves them.

## GUI

The firmware screen uses a 31 px preset-name font, with a 23 px fallback if a name does not fit the 220-pixel text region. All 33 current names fit at 31 px; WOBBLEBASS is widest at 173 pixels. Title and FX status use 23 px; supporting labels use 15 px. Display names are capitalized in a local copy because the larger font roles contain capitals. Stored patch bytes are unchanged. The layout gives the preset name visual priority and uses more of the 240×240 LCD.

The existing 8-row canvas is retained; larger text is drawn in clipped strips, avoiding any increase in framebuffer RAM. The browser has an additional 480×480 backing canvas mirroring the same LCD, shown beside the hardware panel on wide screens and below it on narrow screens. Its responsive size makes the instrument display readable without enlarging or moving the hardware hit targets. Browser mirroring runs on the page thread and does not change AudioWorklet processing.

This remains a playback/control GUI: keys, preset selection, octave, master and FX bypass. Full operator/envelope editing and the original ESP editor are still not ported.

## Checks and compliance

Source/build baseline remains the pinned fm1-base/BSP, r3 pi32v2 clang 4.0.1, AC79 SDK and 44.1 kHz/128-frame interrupt model recorded in PORT.md. No clock, loader, flash, linker, persistent format or multicore changes were made. Hot path: render → voice → operator → amplitude envelope; only its gate semantics and a bounded float expression changed. UI rendering and browser mirroring remain outside sample processing.

| Category | Status | Evidence / limits |
| --- | --- | --- |
| Target/build/ABI | PASS for build | Actual FM-1 build and retained integrity checks. Image 180,180 bytes; installed device/bootloader ABI remains unverified. |
| Audio hot path/numerics | PASS for source/build/host checks | Exact stage-transition comparison, finite checkpoint values, bounded code, no new allocation/logging/I/O/libm/doubles; physical timing unmeasured. |
| Ownership/synchronization | PASS for source scope | Existing single-owner engine and queues unchanged. Browser canvas reads only published framebuffer data. Real interrupt stress remains unverified. |
| Memory/storage/I/O | PASS statically; runtime UNVERIFIED | `.data+.bss` 19,044/98,304 B, pool 333,572/344,064 B, RAM code 5,100/32,768 B; canvas unchanged, no persistent writes. Device peaks unmeasured. |
| Dependencies/includes/licenses | UNVERIFIED overall | Existing bundled fonts/MPL page notices retained; no new dependency. Original RDX/patch licensing remains unresolved. |
| Compatibility/package/recovery | UNVERIFIED overall | Source glide release restored; no patch-format or package/loader change. Device installation/recovery not exercised. |
| Host functional validation | PASS for listed scope | Envelope comparison, existing core and app regressions, WebAssembly/worklet tests; larger LCD and mirror visually inspected. |
| Hardware real-time validation | UNVERIFIED | No hardware timing, analog output, stack or soak measurement; use the existing plan with the new UI load. |

Commands: `bash tools/test-envelopes.sh --check`, `bash tools/test-host.sh`, `bash tools/test-app.sh`, `bash tools/build-emu.sh`, `node tests/emu-test.mjs`, `bash tools/build-fm1.sh`. Logs and current artifact/source hashes are under `build`.

Skipped this turn: physical ESP32/FM-1 envelope recording and perceptual comparison, hardware output/timing/recovery/soak, exhaustive rate/level and retrigger sweeps, cross-browser/mobile testing, and repeat FX sanitizer/source-chain tests because effect processing was unchanged. The focused envelope traces cover note-off behavior that the earlier sustained-note sound comparison did not test. Hardware sound indistinguishability remains unverified.
