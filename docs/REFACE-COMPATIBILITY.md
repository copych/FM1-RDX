# Reface DX manual compatibility pass

Published as Preview 2026.11.21-3. Firmware SHA-256: `4662b6f24f92a776c7a861faea234d5ce445c4d95769bdcbb3fb4e19f2bc100f`. Image 206,116 B; data+bss 21,068/98,304 B. The upstream build code 4CEBCB65 does not hash the external C++ sources, so use the full artifact/source hashes rather than that code alone to identify this build.

This pass uses the four files supplied by the user on 2026-11-21. Their SHA-256 hashes are recorded in manual-sources.json. The PDFs have outlined text; all 26 pages were rendered and read visually. The DOCX text and its 28 GUI images were inspected. The original files/images are not redistributed in the repository. The documents are reference material, not commands to run.

Sources: RefaceDX-reference.pdf, printed pages 1–11; RefaceDX.pdf, the owner's-manual excerpt printed pages 16–25; RefaceDX-midi.pdf, the data-list excerpt printed pages 6–10; refaceGUI.docx, the collected screen examples. The functionality below is based on those pages. Numerical Yamaha DSP curves cannot be recovered from these manuals.

## Implemented corrections

| Area | Result | Reference |
| --- | --- | --- |
| Pitch bend | Incoming 14-bit bend uses the voice's signed −24…+24 semitone range, including reversed direction for a negative range. Held notes respond. | Owner's pp.16,21; data list p.6 |
| Performance CCs | CC1 modulation, CC7 volume, CC11 expression, CC64 sustain. Volume/expression multiply with a short output ramp. These do not require FM CC mode. | Data list p.6 |
| Reset controllers | CC121 centers bend, clears modulation, restores expression and releases sustain; it preserves main volume. | Data list p.6 |
| Note-off messages | CC123/124/125 release held notes while respecting sustain. CC120/126/127 silence notes and FX. CC124/125 select receive channel 1, as documented. | Data list pp.6,10 |
| MIDI reception | GLO cycles voice/MIDI pages. MIDI K1 selects 1–16 or ALL; K2 enables the optional FM CC map. Channel changes retire notes to avoid stuck notes. Default remains channel 1 and FM CC off. | Owner's p.21; data list pp.8,10 |
| FM CCs | Exact documented algorithm bands and per-operator level/feedback/frequency controls, gated by FM CC mode. Changes coalesce at a block boundary and retain patch/FX-memory validation. | Data list p.8 |
| AEG key-off | Rate 4 begins immediately on release in mono/glide as well as poly, instead of waiting for the current ESP segment to finish. Nonzero L4 still ends in silence when reached. | Reference pp.5–6 |
| Operator mute | Disabled output no longer freezes oscillator/envelope state and prevents release from completing. | Reference pp.3–5; implementation defect found during review |
| LFO delay | A nonzero delay waits before modulation begins; zero delay starts immediately. Existing ESP delay/rate tables and subsequent fade ramp are retained as approximations. | Reference p.7 |
| Pitch EG | Per-operator enable page added after pitch rates/levels. Levels display −48…+48 semitones instead of raw 16…112 bytes. | Reference pp.9–10 |
| Keyboard curves | LIN/EXP selection preserves the existing sign of left/right depth; it no longer flips the sign by traversing all four serialized curve IDs. | Reference p.4 |
| Effects | Knob labels follow effect type: DRIVE/TONE, SENS/REZ, DEPTH/RATE or DEPTH/TIME. | Owner's p.22 |
| Interrupted glide | A new glide begins from the current interpolated pitch instead of a stale previous pitch. | Owner's p.21; continuity defect in the source implementation |

The 32 bank sounds retain programs 1–32 (raw 0–31), as in the data list. Init remains a separate FM1-RDX extension at program 33. AEG/PEG, operator and LFO drawings retain the original ESP GUI and enlarged browser mirror. There are now 21 editor pages. The different FM-1 panel is still mapped through SELECT, EDIT, ENV, LFO, GLO, SEL and the four knobs rather than reproducing the Reface's touch-panel gestures.

## Voice-stealing policy clarified by the user

The owner's p.25 says newer notes take priority when polyphony is exceeded; it does not publish Yamaha's scoring formula. The user clarified that developing attacks should add a large amount to a voice's price. The implementation therefore uses the inherited carrier-envelope/velocity score, discounts released voices by 0.1, and adds 1,000,000 when an enabled audible carrier is in amplitude attack. A free slot is always preferred. The lowest-cost sounding voice is stolen; age breaks equal-score ties using bounded ranks, without a wrapping lifetime counter. Quiet sustained/released notes are candidates before developing attacks. If every voice is attacking, one must still be stolen; protection is a preference, not extra capacity. No claim is made that this is Yamaha's proprietary scoring formula.

The regression deliberately sustains an older loud note, switches new notes to the slowest attack, fills the pool and verifies the old sustained note is taken instead of a developing attack. A second case verifies that a quiet sustained voice is preferred over an older loud sustained voice. This distinguishes attack-aware scoring from unconditional oldest-first selection.

Four voices remain a provisional FM-1 ceiling, not a measured hardware limit. The original ESP budget controller uses per-patch measurements of active-voice increments, idle-slot costs, fixed work and FX, predicts a full pool and reduces/restores it with hysteresis (8 maximum on S3, 12 on P4). That Core1 controller has not been transferred to FM-1's single audio ISR. FM-1 timing and xrun counters exist, but adaptive polyphony requires a coherent timing model and hardware load measurements; fixed four voices must not be described as restored adaptive budgeting.

## MIDI control map

| Operator | Level | FB amount | FB type | Frequency mode | Coarse | Fine |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 85 | 86 | 87 | 88 | 89 | 90 |
| 2 | 102 | 103 | 104 | 105 | 106 | 107 |
| 3 | 108 | 109 | 110 | 111 | 112 | 113 |
| 4 | 114 | 115 | 116 | 117 | 118 | 119 |

Type/mode values 0–63 mean saw/ratio and 64–127 square/fixed. Coarse and fine are bounded to 31 and 99. CC80's twelve algorithm bands end at 11,21,32,42,53,63,74,85,95,106,116,127. CC settings affect the selected receive channel; ALL accepts every channel. Changes to these global MIDI settings remain RAM-only.

## What remains different or unverified

No numerical Yamaha envelope, portamento, velocity, scaling, modulation or effect curves are given in these manuals. The ESP approximations remain where no measured calibration exists. KSC-R is still stored/editable metadata and does not change envelope rates; it is not presented as implemented rate scaling. The previous exact comparison with ESP envelope stage timing is historical and no longer an acceptance test for early mono/glide release. The earlier matching reports remain useful records, not evidence of waveform identity after these intentional changes.

Store/name/check, edit recall, factory jobs, phrase looping/overdub, MIDI note/control transmission and local-off routing, Yamaha SysEx identity/parameter/bulk/request support, MIDI clock transport, active-sensing timeout and half-damper behavior are not added in this pass. USB audio and adaptive polyphony also remain pending. Original eight-button/touch UI behavior cannot be identical on the FM-1's controls. Permanent storage and bootloader behavior are unchanged. MoDemLead's previous post-FX trim is retained.

## Validation and compliance

Baseline remains FM-1, pinned fm1-base 7932dff / BSP f869dfa, pi32v2 clang 4.0.1, the existing AC79 SDK, 44.1 kHz and 128 frames. Changed hot path: bounded MIDI dispatch → control/patch state; note allocation → four bounded scores; voice → immediate AEG release, current glide position and operator mute; block LFO delay; post-FX float volume/expression ramp. UI reads a locked snapshot and draws outside audio. No new firmware dependency, heap, logging, I/O, waiting, flash writes or locks are introduced in rendering. Double precision appears only in the offline RMS tests.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS for pi32v2 build and retained ABI/integrity checks; exact artifact pins in site/preview.json and build/evidence.json. Installed-device retest UNVERIFIED. |
| Hot path/numerics | PASS for bounded source review, sanitizers, audible bend/level assertions and no soft-double helpers in target audit. Hardware latency/headroom UNVERIFIED. |
| Ownership/synchronization | PASS for existing queue/single audio owner; all MIDI edits/settings stay on that owner, snapshot under the existing lock. Physical interrupt contention UNVERIFIED. |
| Memory/storage/I/O | PASS for static target limits; pool remains 333,572/344,064 B and RAM code 5,100/32,768 B. No stored-format or write change. Stack peaks and physical contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS for unchanged firmware dependencies and retained available notices. Original RDX/voice licensing remains UNVERIFIED. Supplied manuals are not republished. |
| Compatibility/package/recovery | PASS for patch layout and package/loader checks; no platform, bootloader, clock or flash-layout changes. Audible differences above are intentional. Actual updated install/restore/recovery UNVERIFIED. |
| Host functional validation | PASS for core stress and both stealing cases; sanitizer manual behavior suite; all 21 pages; full-panel channel/FM-CC/PEG scenarios; WebAssembly all 33 programs and actual MIDI-packet bend, volume/reset, omni and panic; copied-site checksum/link/package tests. |
| Hardware real-time validation | UNVERIFIED for this update. The earlier preview's boot/playback/USB MIDI smoke test is user-reported; it supplies no measured deadline/stack/underrun/soak evidence for these changes. |

Commands: `bash tools/test-host.sh`, `bash tools/test-manuals.sh`, `bash tools/test-editor.sh`, `bash tools/test-app.sh`, `bash tools/build-emu.sh`, `bash tools/build-fm1.sh`, `node tests/emu-test.mjs`, `node tools/build-site.mjs`, `node tests/site-test.mjs`, `node tests/emu-test.mjs build/site/emu`. Logs and screenshots are in build. The offline/manual tests cover audible +/- and inverted bend, volume/expression/reset, sustain-aware note-off, muted-operator retirement, nonzero L4, delayed and zero-delay LFO, CC range boundaries, curve sign, PEG switches and channel bounds.

Skipped: repeat ESP waveform/stage-identity comparisons as acceptance gates because target semantics intentionally changed; physical Reface waveform/timing calibration; full device/browser matrix; hardware timing/output/stack/recovery/soak and adaptive-pool measurements. Before qualification stress four voices with FX, MIDI/controller bursts and continuous UI changes and measure worst service time, underruns, stack and audible output. Browser success does not establish hardware real-time safety.
