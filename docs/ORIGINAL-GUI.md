# Original ESP GUI restored — 2026-11-21

The earlier prototype imported the sound engine but left behind the completed ESP GUI. This patch reuses its drawings, widgets, font, algorithm diagrams and effect/waveform bitmaps. The original snapshot is unchanged. `tools/import-gui.mjs` produces `src/gui`; `gui-lock.json` records hashes of every imported input.

All 13 original main pages are reachable: home, level, feedback, frequency, algorithm, voice, three operator pages, three LFO pages and Phaser Lab. The existing effect drawing is exposed for both slots. Four new page wrappers use the existing envelope graph helper for AEG/PEG rates and levels, making 19 pages. Phaser Lab displays its five tuning values and explicitly labels unavailable meters.

The original 128×64 layout renders into a clipped 1,024-byte monochrome buffer and scales to 240×120. Titles use 23 px text and knob/status labels 15 px. The enlarged live browser LCD remains. Drawing reads a patch snapshot rather than changing audio state.

| Control | Binding |
| --- | --- |
| SELECT | Browse all 19 pages |
| K1–K4 | Edit displayed parameters |
| SEL tap | Next operator |
| SEL held + frequency knob | Fine frequency |
| EDIT | Cycle level, feedback, frequency, algorithm, operator pages |
| ENV | Cycle AEG rates/levels, PEG rates/levels |
| LFO / FX | Cycle LFO pages / effect slots |
| GLO / REC / HOME | Voice / Phaser Lab / home |
| ALGORITHM | Change algorithm from any page |
| SEL + FX | Wet/dry bypass |
| PRESET | Load bank preset, discard RAM edits |
| SAVE | RAM-only notice; no persistent write |

Edits use the existing bounded panel queue and single audio owner. Patch edits coalesce once per block, validate bounds and arena requirements, then update cached voice parameters. Ordinary edits preserve phases and held gates. Effect-type, delay-size and mono/poly changes use the existing fade/clear/reinitialize path. Excessive effect-memory requests are rejected visibly. Queue overflow retains the panic fallback. UI drawing runs outside render; its bounded snapshot uses the platform lock.

Envelope metadata applies to subsequent segments/notes without retriggering. Pitch-envelope and LFO delay changes take effect on subsequent notes. Rate scaling remains editable metadata because the ESP DSP did not apply it. Pitch-bend range is editable but MIDI pitch-bend input remains pending. Not every possible patch field or source CC is bound. Naming, persistent save/load, SysEx and sequencer are pending. Phaser Lab tuning is temporary and resets when its effect is rebuilt.

Baseline: pinned fm1-base/BSP, pi32v2 clang 4.0.1 and AC79 SDK in PORT.md; 44.1 kHz, 128 frames. Changed paths: panel → queue → block-boundary patch update → cached parameters; UI → snapshot → source raster. Sample processing and envelope tables are unchanged.

| Compliance category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS for target build/integrity checks; image 204,236 B. Installed-device ABI UNVERIFIED. |
| Audio hot path/numerics | PASS for source/build/host scope: bounded edits, fixed storage, LUT-derived parameters, no new heap/log/storage/blocking work. Target audit has no soft-double helpers. Physical cost UNVERIFIED. |
| Ownership/synchronization | PASS for source scope: existing bounded queue, one audio writer, locked snapshot, separate drawing. Device interrupt stress UNVERIFIED. |
| Memory/storage/I/O | PASS statically: data+bss 21,016/98,304 B, pool 333,572/344,064 B, RAM code 5,100/32,768 B. No writes; stack/display contention UNVERIFIED. |
| Dependencies/includes/licenses | Adaptation audit PASS: hardware display driver, Arduino and tasks omitted, available notices retained. Original RDX/assets/voices licensing UNVERIFIED. |
| Compatibility/package/recovery | Package checks PASS; persistent format, platform/BSP and loader unchanged. Installation/backup/recovery UNVERIFIED; no flash performed. |
| Host functional validation | PASS: ASAN/UBSAN all 19 pages × four operators, live level editing, bounds, pending FX edits, memory rejection, Phaser tuning; full-app panel scenarios, core regression, WebAssembly all 33 presets and wet/dry. Browser graph and 127→126 knob edit visually verified. |
| Hardware real-time validation | UNVERIFIED: measure render maxima, stack, underruns, fast UI/MIDI contention and soak under four-voice/FX load before qualification. |

Commands: `bash tools/test-editor.sh`, `bash tools/test-app.sh`, `bash tools/test-host.sh`, `bash tools/build-emu.sh`, `node tests/emu-test.mjs`, `bash tools/build-fm1.sh`. Logs, screenshot and source/artifact hashes are in `build`. Skipped: hardware sound/timing/output/recovery/soak, exhaustive parameter combinations, cross-browser/mobile checks and repeated unchanged envelope/source-chain comparisons. Host tests do not establish device timing or sound indistinguishability.
