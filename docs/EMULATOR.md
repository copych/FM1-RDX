# Browser emulator — 2026-11-20

Current GUI: [original ESP screens and bindings](ORIGINAL-GUI.md), including live parameter editing. FX now opens the effect pages; SEL + FX toggles bypass. Initial sizes and feature omissions below are historical.

Update: the emulator now includes both source effect slots and an FX bypass button. See [sound-matching evidence](SOUND-MATCHING.md) for the current firmware/emulator validation. Earlier dry-build sizes and effect omissions below describe the initial milestone.

The current FM1-RDX application and DSP compile to WebAssembly using fm1-base's existing `--web` build. Its AudioWorklet runs the synth and app clock, renders the LCD, and feeds stereo browser audio. The full FM-1 panel supports mouse/touch, keyboard focus and shortcuts. This is a source-compiled simulator, not a pi32v2 CPU emulator or firmware-package loader.

Build: `wsl -d Ubuntu -- bash tools/build-emu.sh`. Test: `node tests/emu-test.mjs`. Launch: `./Start-Emulator.ps1` from PowerShell. Default address: http://127.0.0.1:8788/. `-Rebuild` rebuilds first; reload the page afterward. The local server remains available after this chat and serves only `app/build/emu`. Its PID is recorded in `build/emu-server.pid`; close it using Task Manager if no longer needed. Do not open `index.html` through a `file:` URL: AudioWorklet needs HTTP localhost or HTTPS.

Emscripten 3.1.69 was already installed. Its Ubuntu Node executable needs `/lib64/ld-linux-x86-64.so.2` when launched from this WSL1 setup; `tools/node-wsl.sh` implements the existing earlier project's workaround. No system toolchain or configuration was changed. The upstream builder, worklet and panel drawing are unmodified. `tools/prepare-emu.py` adapts the upstream page with RDX-specific instructions and removes external font requests; its MPL notice is retained in the generated page. The precompiled wasm is 134,469 bytes. Current source/artifact hashes are in `build/evidence.json`.

Validation performed:

- Actual WebAssembly plus the upstream AudioWorklet harness: PASS for boot readiness, LCD framebuffer changes, initial silence, panel note audio, PRESET encoder behavior, all 33 MIDI program changes followed by notes in the same block, finite/bounded samples, and panic returning exact silence. Tests use the default 44.1 kHz worklet configuration.
- Visible browser: PASS for Switch on reaching On, the FM1-RDX display appearing, PRESETS keyboard increment selecting WobbleBass, and a piano shortcut dispatching through the panel. A screenshot is saved at `build/emulator.png`. Audio samples are verified through the worklet test; no claim is made about subjective listening or the user's playback volume.
- Local server: HTTP root and WebAssembly MIME type checked; bound to 127.0.0.1. Launcher reuse checked against its health endpoint. The server sends no device commands.
- Firmware and DSP source files were not changed for this task. The previously generated `.fwsc` remains unchanged; target compilation was not repeated because only build/launch tools, emulator page generation and tests changed.

Compliance for this emulator-only change:

| Category | Status and evidence |
| --- | --- |
| Target/build/ABI | Web build PASS; device ABI not changed or requalified. |
| Audio hot path/numerics | Worklet sample checks PASS; firmware DSP unchanged. Device timing remains UNVERIFIED. |
| Ownership/synchronization | Existing upstream worklet model retained; input travels through its message port. |
| Memory/storage/I/O | Local static serving and browser memory only; no firmware storage changes. Browser/target memory limits are separate. |
| Dependencies/includes/licenses | Existing installed Emscripten and upstream MPL assets retained. Original RDX/voice licensing remains UNVERIFIED as before. |
| Compatibility/package/recovery | No package or installer changes, no flashing; hardware recovery remains UNVERIFIED. |
| Host functional validation | PASS for the WebAssembly/worklet cases and visible browser checks above. |
| Hardware real-time validation | UNVERIFIED; this simulator does not measure FM-1 execution or physical peripherals. |

Skipped: cross-browser/mobile testing, non-44.1-kHz resampling validation, hardware MIDI input, sensed velocity/pressure, subjective audio listening and all hardware timing/recovery tests. Effects/full editing remain absent just as in the firmware prototype. The optional built-in screen-reader voice is not bundled; the original app also does not yet publish its screen text to the reader.
