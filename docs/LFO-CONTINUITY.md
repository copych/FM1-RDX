# WobbleBass isolated OP1 — Preview 2026.11.21-8

The user's dry isolated-operator waveform shows short runs of steps affecting both channels. Offline reproduction identifies a delayed-LFO fade interpolation bug, separate from the sine-table seam fixed in preview 7. The old LFO scales its published start value and end value by fade gain, but computes the increment using the unscaled start value. A per-block ramp then resets at the next boundary. The original source also has the inconsistent interpolation; the port's later change from immediate fade to a genuine delay exposes it after note onset instead of near attack. No claim is made to have reproduced the ESP hardware comparison locally.

Continuous-wave interpolation now uses waveform(start phase) times the start-of-block fade gain, and waveform(end phase) times end-of-block gain. Adjacent blocks share the same endpoint. Delay, fade duration, rate, depth and note envelopes stay unchanged; square and sample/hold intentionally retain their stepped behaviour. The source importer reproduces the correction. Changes run once per voice/block, no new per-sample work, dependencies or allocations.

Measured offline (host, not FM-1): tests/lfo_continuity.cpp checks sine/triangle at rates 0/73/127, delays 1/43/100, 10000 blocks per combination. Published preview 7 produces 901 seam/bounds violations, worst LFO gap 0.999997139. Corrected code has zero violations and worst gap 0.000000060. tests/op1_continuity.cpp renders WobbleBass at MIDI 48, disables operators 2–4, and compares amplitude-modulated output against the identical unmodulated carrier away from zero crossings. Preview 7 has a 0.241075695 gain step at sample 16768 (a 128-frame boundary, 0.380227 s). Corrected maximum per-sample gain change is 0.000145435; the regression threshold is 0.002. This directly reproduces and removes the isolated OP1 discontinuity without FX, USB or hardware scheduling. User confirmation of the exact photographed segment remains pending.

Run bash tools/test-wobble.sh for these assertions plus ASan/UBSan/float-cast-overflow 30-second WobbleBass holds across MIDI 36/48/60/72 wet/dry and release (240 seconds audio). First attempted invocation was edited while running and failed with a shell offset error; the final unedited script was rerun. Standalone before/after tests use the same compiler flags and only the LFO header differs. Temporary baseline source is under ignored build/lfo-before.

## Compliance

Parent c95e6d5; unchanged FM-1 platform/BSP/SDK/compiler and flags from USB-RECORDING.md. Single audio owner, nominal 44.1 kHz/128 frames, 2.90249 ms deadline. Four provisional voices and USB recording remain enabled.

| Category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS tools/build-fm1.sh with retained integrity checks, existing pi32v2 clang 4.0.1/r3 + AC79 SDK. Device ABI UNVERIFIED. Platform build ID B581E75F omits external C++ inputs; artifact/source hashes identify this patch. |
| Hot path/numerics | PASS scoped source/generated review: control-rate float endpoint scaling only, no doubles/libm/heap/I/O/logging; target checks no soft-double helpers or RAM-code calls. Actual cycle cost UNVERIFIED. |
| Ownership/synchronization | PASS source: same per-voice audio owner, no new shared state, task, interrupt or lock. |
| Memory/storage/I/O | PASS static build: image 210440 B, RAM data+bss 23452/98304 B, pool 333572/344064 B, RAM code 5100/32768 B. No storage, clock, USB configuration or peripheral changes. Device stack/contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS scoped: no runtime dependency/asset added; existing notices, original snapshot and submodules retained. Original-source licensing remains unresolved. |
| Compatibility/package/recovery | PASS local package/installer/hash checks; no state/layout/loader changes. Actual install/rollback UNVERIFIED. |
| Host validation | PASS before/after isolated OP1 and continuous-LFO regressions; final long-note sanitizer script, core bounds/algorithms/33 patches/stress, manual behaviour, matching wasm and assembled site tests. |
| Hardware real time | UNVERIFIED: physical playback/USB capture/timing/stack/soak/recovery not performed by agent; update and repeat the isolated dry capture plus full patch, then check timing under USB recording/MIDI/UI load. |

Skipped full platform suite and repeated unrelated editor/panel suites because patch changes only block-rate LFO arithmetic; manual and actual wasm MIDI/UI regressions retained. Nominal sample rate is not a measured device-clock result. Firmware SHA-256 0ae3b9cab895b2a7bba221e3d045c23d810cac57088dd489452baf8388430737. Host-tested preview, not hardware qualification.
