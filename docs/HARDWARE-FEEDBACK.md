# First hardware feedback and preview 2026.11.21-2

The user reports that the first preview boots and plays on an FM-1. USB MIDI also works after closing the browser installer tab; the earlier host error was resolved without a USB driver change. This is a user-reported smoke test, not a measured real-time qualification. The revised build below still needs a physical retest.

## Changes

The UI previously sent 30 completely black 8-row strips before redrawing the title, source screen and labels on every change. The physical panel exposed that intermediate black frame. Those blank transfers are removed. Each content strip already contains its own background; the initial UI clear remains. This requires no framebuffer allocation or extra RAM. Titles and status use fixed-height background-filled strips, so shorter replacement text clears the previous text.

Init is now outside the four banks of eight. DigiChord is 1–1; D'n'Beats is 1–8; BeginSweep is 2–1; Chopper is 4–8. Init is labelled INIT PATCH. Internal source asset indices stay unchanged for reproducibility. MIDI raw program bytes 0–31 now select the 32 bank sounds (internal indices 1–32); raw 32 selects Init (internal 0). These are normally displayed as MIDI programs 1–32 and 33. PRESET still reaches Init immediately before DigiChord. This intentionally changes the previous preview's program mapping to align it with the bank numbering.

MoDemLead measured RMS 0.11359056 with effects, against median 0.02310633 for the other 31 bank sounds. The audit renders notes 48/60/72 separately at velocity 100 for about one second, measures RMS after 200 ms and peak throughout, and tests dry and wet outputs. This is an RMS comparison, not perceptual loudness metering. A final 0.25 gain (-12.04 dB) brings its wet RMS to 0.02839764 and peak from 0.17221898 to 0.04305474. Gain is after the effect chain, retaining source operator levels, feedback, distortion drive and patch bytes. Its dry output receives the same trim. Both RMS and peak for every other preset are unchanged in this test. The trim follows the active patch across fades, rather than the newly requested patch during fade-out.

The installer and user README explain closing the installer tab before opening a MIDI app. The README is now a user guide; technical material is in BUILD.md. The site supplies the browser synth and a checksum-pinned experimental installer, with stock restore and existing confirmation retained; it offers no bootloader replacement. See PUBLISHING.md for assembly/deployment details.

## Validation and compliance

Baseline: pinned fm1-base 7932dff / felucca-bsp f869dfa, pi32v2 clang 4.0.1, existing AC79 SDK, FM-1, 44.1 kHz / 128 frames. No SDK, platform, bootloader, clock, flash layout or persistent format changes. Changed paths: UI strip rendering; bounded MIDI program mapping; one float gain per channel/sample after effects. No new allocation, I/O, logging, locks, libm or software-double operations in rendering. The offline RMS audit uses doubles only in its host test executable.

| Category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS for target build/integrity audit; image 204,560 B, build BFFC5529. Installed-device ABI/retest UNVERIFIED. |
| Audio hot path/numerics | PASS for source/build/host scope: fixed post-FX gain, bounded loop, no soft-double helpers; 66 preset dry/wet RMS/peak comparisons. Hardware cycle cost UNVERIFIED. |
| Ownership/synchronization | PASS for source scope: unchanged queue/audio ownership; gain commits with patch application, display remains outside rendering. Device interrupt stress UNVERIFIED. |
| Memory/storage/I/O | PASS statically: data+bss 21,016/98,304 B, pool 333,572/344,064 B, RAM code 5,100/32,768 B. No writes or new display buffers. Stack peaks/display contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS for retained notices and unchanged dependencies/platform. Original RDX/voice licensing remains UNVERIFIED; no blanket licence asserted. |
| Compatibility/package/recovery | PASS for UFW/loader/identity checks, pinned SHA-256 and corrupted-download rejection. Program mapping changes explicitly above. Actual new install/restore/recovery UNVERIFIED. |
| Host functional validation | PASS: app/editor/bank scenarios; ASAN/UBSAN editor coverage; all 33 MIDI programs, finite audio, panic and wet/dry in both built and published WebAssembly; site/installer asset/hash/unsupported-browser checks. Init and Bank 1–1 screenshots inspected. |
| Hardware real-time validation | UNVERIFIED: first preview user-reported boot/playback/USB MIDI success; no cadence, worst render/interrupt latency, headroom, underruns, stack, analog measurements or soak. New gain/redraw require physical retest. |

Commands: `bash tools/test-app.sh`, `bash tools/test-editor.sh`, host compile/run of `tests/preset_levels.cpp` with `app/engine.cpp`, `bash tools/build-emu.sh`, `bash tools/build-fm1.sh`, `node tests/emu-test.mjs`, `node tools/build-site.mjs`, `node tests/site-test.mjs`, `node tests/emu-test.mjs build/site/emu`. Logs and before/after level CSVs are in build. Pinned firmware SHA-256: `309f440c80e0350f96dcc67ee382daef793e891e362f68f05346d0fe93ab0509`.

Skipped: repeated unchanged core envelope/source-chain comparisons; exhaustive note/velocity/patch-edit combinations; hardware retest/restore/soak, subjective loudness balance and multi-application USB testing. Before qualification measure the existing four-voice/FX/UI/MIDI stress plan and confirm that redraws no longer blink on the physical LCD.
