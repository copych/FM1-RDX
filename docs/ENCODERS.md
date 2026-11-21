# Speed-sensitive parameter knobs

Preview 2026.11.21-5 accelerates continuous K1–K4 parameters. Slow motion stays at one unit per detent. After three consecutive same-direction motion observations, gaps of up to 35, 70 or 120 ms select 8×, 4× or 2× steps. A gap longer than 120 ms or direction reversal returns to 1×. Deltas are clamped to ±127 before/after scaling and the editor retains its parameter bounds.

The BSP exposes accumulated detents, not individual detent timestamps. Timing between nonempty UI reads approximates rotation speed; it is not a measured instantaneous shaft velocity. Each knob has independent history. Page/operator/preset changes, Phaser Lab selection and SEL transitions reset history. SEL suppresses acceleration and retains its existing fine-frequency behavior. Switches, types, curve selection, LFO wave, MIDI channel, presets, global algorithm and page selectors remain unaccelerated. These thresholds are a UI design choice, not a claimed Yamaha calibration.

The Home-to-Level shortcut is preserved. Acceleration happens in app_input_only on the UI/main owner, before its existing bounded editor event is queued. It does not create extra events. The audio owner, renderer, effects, voice allocation and numeric tables are unchanged. Host `turn` simulates timed detents; explicit `spin` requests exact unaccelerated steps.

| Compliance category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS for existing FM-1/pi32v2/SDK build and retained integrity checks; image 206,532 B. Physical retest UNVERIFIED. |
| Hot path/numerics | PASS for source scope: acceleration is bounded integer UI code; audio DSP unchanged; target audit has no soft-double helpers. Device latency UNVERIFIED. |
| Ownership/synchronization | PASS for source scope: four histories owned by main/UI; existing single-producer event queue and audio owner retained. |
| Memory/storage/I/O | PASS statically: data+bss 21,084/98,304 B, pool 333,572/344,064 B, RAM code 5,100/32,768 B; no allocations, storage or peripheral changes. Stack/physical contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS for no added firmware dependencies and retained notices. Original RDX/voice licensing remains UNVERIFIED. |
| Compatibility/package/recovery | Package checks PASS; existing delta bounds, patch format and loader retained. Installation/restore/recovery of this update UNVERIFIED. |
| Host functional validation | PASS: UBSAN slow/2×/4×/8×, reversal, pause, override, independent history, batching, extreme deltas and uint32 timer wrap; full-panel scenarios; WebAssembly compares fast/slow LCD output to exact-step references and verifies SEL; all existing app/worklet regressions and site checks pass. |
| Hardware real-time validation | UNVERIFIED: physical turning feel, interrupt/UI contention and load/soak still need testing. No physical flash performed here. |

Commands: `bash tools/test-encoders.sh`, `bash tools/test-app.sh`, `bash tools/build-emu.sh`, `bash tools/build-fm1.sh`, `node tests/emu-test.mjs`, `node tools/build-site.mjs`, `node tests/site-test.mjs`, `node tests/emu-test.mjs build/site/emu`. Tests/logs are under build. Skipped repeated DSP/envelope/FX sanitizer suites because their code is unchanged, and all hardware timing/stack/soak/recovery tests. Current source/artifact hashes are in build/evidence.json and site/preview.json.
