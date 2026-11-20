# FM1-RDX

First playable FM-1 prototype of the user's `RDX_before_UAC` ESP32 synth, built on fm1-base. This is a development package, not a hardware-qualified release or a claim of Yamaha sound equivalence.

Clone with submodules: `git clone --recurse-submodules https://github.com/copych/FM1-RDX.git`. For an existing clone, run `git submodule update --init --recursive`. Generated builds, local toolchains and emulator output are excluded from version control. Build prerequisites are Node.js, Python 3 with Pillow, a C++ compiler for host checks, Emscripten for the emulator, and the matched JieLi toolchain/AC79 SDK for the device build. The current scripts use WSL Ubuntu on Windows.

Latest update: [original ESP GUI restored and bound](docs/ORIGINAL-GUI.md): 19 pages reuse the source drawings, with live operator, envelope, LFO and effect editing. Larger titles and control labels surround the scaled source screen; the browser retains its enlarged LCD view. The earlier [envelope correction](docs/ENVELOPES-AND-GUI.md) remains: all 1,188 tested AEG/PEG transition sequences match the host source reference sample-for-sample.

Implemented: the inherited four-operator engine and 12 algorithms, four provisional voice slots, 33 source voices, both serial source effect slots, panel keys with fixed velocity 100, octave buttons, PRESET encoder, channel-1 USB/TRS MIDI notes, sustain, program changes, and panic. Audio is stereo, 44.1 kHz, with 128 frames per callback. SEL + FX switches the effects on/off for comparison. Patch changes fade out, prepare the bounded effect arena while silent, then fade in; following note events are retained.

Not yet ported: the original CC map, pitch bend/mod wheel input, live SysEx, patch saving/naming, sensed key velocity/pressure, USB audio, BLE, and adaptive voice budgeting. SAVE shows a RAM-only notice. The PRESET encoder clamps to bank bounds; MIDI program numbers 0–32 select the same bank and discard RAM edits. The default voice is DigiChord. All supplied effect combinations fit without shortening their delay taps or reverb topology; excessive edited combinations are rejected. See [sound matching](docs/SOUND-MATCHING.md).

The original snapshot is unchanged. `src/core`, `src/fx` and `src/gui` hold reproducibly adapted DSP and GUI; `app` supplies the FM-1 adapter; `platform/fm1-base` is an unmodified recursive upstream checkout. Revisions and input hashes are in `source-lock.json`, `preset-lock.json`, `effects-lock.json` and `gui-lock.json`.

## Browser emulator

Run `./Start-Emulator.ps1` in PowerShell, then open **http://127.0.0.1:8788/**. The launcher builds the emulator if needed, starts a server bound only to localhost, and opens your browser. Use `./Start-Emulator.ps1 -Rebuild` after firmware changes. A running emulator needs a page reload after rebuilding.

Click **Switch on**. Click the piano keys, or focus the panel and use `Q`–`I` / `A`–`K` for white keys and `1`–`0` / `-` for black keys. Drag **PRESET**, scroll over it, or focus it and press arrow keys to choose a voice. **MASTER** changes volume; **OCT− / OCT+** transpose. **SELECT** browses pages; **K1–K4** edit parameters. Tap **SEL** for the next operator. **EDIT**, **ENV**, **LFO** and **FX** cycle their pages; **SEL + FX** toggles bypass. Full bindings and limits are in [the editor record](docs/ORIGINAL-GUI.md).

This runs the current app and synth code compiled to WebAssembly; it does not load or emulate the pi32v2 instructions in the `.fwsc` package. All 33 voices have passed a browser-worklet regression. See [emulator validation](docs/EMULATOR.md). To rebuild/test directly:

```powershell
wsl -d Ubuntu -- bash tools/build-emu.sh
node tests/emu-test.mjs
```

From PowerShell in this directory:

```powershell
node tools/import-core.mjs
node tools/import-presets.mjs
node tools/import-effects.mjs
node tools/import-gui.mjs
wsl -d Ubuntu -- bash tools/test-editor.sh
wsl -d Ubuntu -- bash tools/test-host.sh
wsl -d Ubuntu -- bash tools/test-effects.sh
wsl -d Ubuntu -- bash tools/test-sound.sh
wsl -d Ubuntu -- bash tools/test-app.sh
wsl -d Ubuntu -- bash tools/build-fm1.sh
```

The build uses the existing `/root/.jieli/toolchain` and the SDK in the earlier FM1-X0X project. Override `JIELI_TOOLCHAIN` and `AC79_SDK` on another machine. Output: `app/build/rdx.fwsc`. No install or flash command is run by these scripts.

Read [the port and validation record](docs/PORT.md) before device testing. In particular, package identity `FM-1_9900` and the screen-reader URL are development placeholders. Release metadata and source/asset licensing remain unresolved; the upstream release gate is retained. No firmware has been installed on an FM-1.
