# Saving, phrase looper and GUI preview

Local version: `1.0.0-beta.2-dev`. Public v1.0.0-beta is unchanged.

## User controls

- SAVE opens Store. SELECT chooses one of 32 user slots, separate from factory voices. K1 selects a name character; K2 changes it; K3/K4 choose bank/slot. SAVE commits, with an extra confirmation for an occupied slot. HOME cancels. Flash writes deliberately mute audio; stop the phrase first.
- REC arms recording, starting on the first note. REC again closes to a whole-beat length and starts playback. PLAY stops/restarts from the beginning. SEQ opens/closes the transport view. HOME allows sound editing during playback.
- REC during playback arms one overdub take at the next loop boundary; recording ends after that pass or another REC. Four takes, 512 events each. K4 changes tempo (30–300 BPM). SEL+SAVE clears a stopped phrase.
- Records notes/velocity, bend, CC1/7/11/64. Patch changes and MIDI clock are not recorded. No guide/metronome or manual silent-first recording start in this preview. Maximum phrase length is 600 quarter notes (ten minutes at 120 BPM). Phrases stay in RAM; user voices persist in flash or browser site storage.
- SEL+GLO retains access to Phaser Lab. ARP remains unassigned.

Original-reference evidence: supplied `RefaceDX.pdf`, extracted manual pages 23–25 (`build/manuals/RefaceDX/page-8.png` through `page-10.png`), and supplied `refaceGUI.docx` Store/Name/Looper drawings. This implementation follows the key-on/transport model but has the explicitly listed limits above.

## Execution and storage

CPU1 owns modal input/drawing and produces immutable requests. CPU0 main captures snapshots and performs explicit saves only after the control job retires. It disables the dry pipeline, panics playback and waits for the actual BSP CPU1 return before entering the existing flash writer. Read-back length/hash and patch validation precede cache publication. CPU1 mode is restored afterward. No flash/UI work enters the renderer.

CPU0 audio consumes commands and advances the phrase once per 128-frame block at 44.1 kHz (2.90249 ms). Integer 96-PPQN clock retains fractional time without cumulative block-rounding drift. Event timing is quantized to 96 PPQN (about 5.21 ms at 120 BPM), with up to one additional audio block of dispatch latency. Four cursors avoid event sorting/scanning in the hot path; at most 32 playback events are dispatched per block. Denser simultaneous bursts stop the loop and release its notes. Capacity-full takes reject further events. Wrap/stop release loop-owned notes while preserving live-held notes; all-notes-off retains the previous release semantics, while panic silences immediately.

Payload format is explicit RDU1/version 1, two banks of sixteen 150-byte patches, validity/trim masks, reserved-byte checks and full occupied-patch validation. Each bank is 2416 bytes, below PLAT_STORE_MAX 3840. Final output trim for MoDemLead survives storage. Factory indices 0–32 stay unchanged; user indices 33–64 follow them. MIDI programs 0–31 remain factory voices, 32 Init, and 33–64 user slots.

Existing object IDs/map are preserved: settings FC000/FD000, panel DC000/DD000, BLE DE000/DF000. New bank objects 3 and 4 use D8000/D9000 and DA000/DB000, inside the supported 97000–DFFFF data window. Nothing touches the E0000–FBFFF loader staging area. Existing CRC/A-B commit-last storage is retained. Do not reinterpret this format as another firmware's bank. Rollback to the published beta ignores these new objects; it does not export them. The emulator's existing localStorage handoff preserves user banks across reloads; clearing site data removes them.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS: FM-1, matched AC79 SDK and pi32v2 Clang 4.0.1; existing manifest, RAM closure, product, register-access and package checks retained. Engine/editor O2, app/platform Os, single precision, contraction disabled. See build/features-target.log and build/evidence.json. |
| Audio hot path/numerics | PASS (source/build scope): bounded static events/cursors, integer musical clock with constant divisor, no allocation, I/O, logging, blocking or added floating-point math. Target audit reports no soft-double routines or forbidden 64-bit multiply pattern. The bounded clear/reset and 128-note release scan still require device timing measurement. |
| Ownership/synchronization | PASS (source/host scope): existing snapshot/reply hashes and audio ownership lock retained; main-only flash after full worker retirement; audio never waits for optional UI/flash. Bridge failure/queue tests and live-versus-loop note ownership checks pass. |
| Memory/storage/I/O | PASS (static/host scope): image and linker budgets checked, two 2416-byte A/B objects, reserved maps unchanged, explicit audio pause, semantic decoding and failed-save/reboot tests. Physical flash interruption and peak stacks UNVERIFIED. |
| Dependencies/includes/licenses | PASS: no new dependency/runtime/asset; source manuals used as references, not redistributed; existing notices retained. |
| Compatibility/package/recovery | PASS (source/package/host scope): versioned bank payload, existing slots/protocol/factory numbering preserved; installer pins, damaged package rejection and absence of a bootloader offer tested. Real save/rollback/power-cycle UNVERIFIED for this change. |
| Host functional validation | PASS: tools/test-app.sh (including 1188 exact normal/dry scenarios, bridge, budget, UI/LCD, codec/looper sanitizers); targeted final controls; tests/emu-test.mjs includes phrase audio, save and restored-bank playback; tests/site-test.mjs validates the local package. Logs under build/features-*. |
| Hardware real-time validation | UNVERIFIED for the added saving/looper paths. Earlier beta's seven-voice hardware results remain historical evidence, not a measurement of this change. |

No deliberate power-loss test, device flashing, USB recording soak, MIDI/USB contention run, live stack measurement or new hardware timing measurement was performed. Browser screen-reader voice assets are absent; the emulator uses the existing plain-text fallback. Full unrelated BSP board tests were not rerun (the known Linux fixed-address audio test limitation remains); focused LCD transport/cooperative-wait suites pass.

Target totals: image 204120 bytes; RAM code 22140/32768; data+BSS 89388/98304; pool 333572/344064, including the existing 8192-byte CPU1 stack reservation. Artifact hashes are in `saving-looper-artifacts.json`.

## Hardware check

1. Save an edited/named voice to each user bank, cancel an overwrite, then confirm an overwrite. Power off/on and verify names, sound and factory voices. Check an imported Soundmondo voice and MoDemLead output level.
2. Record/replay a phrase, change tempo, overdub four takes, stop/restart and clear. Hold the same live pitch while stopping the loop; it should remain held until released. Confirm phrase loss at power-off is expected.
3. Run seven voices with the heaviest intended FX/modulation, USB MIDI/audio recording and active editing/looper screens. Record END/WORK/Q/TURN, XRUN/LATE/LCD errors and stack figures over at least five minutes. Compare with the published beta; note any automatic voice shedding.
4. Verify explicit saving pause/recovery and close the installer before MIDI/DAW use. Do not intentionally interrupt flash power on the sole unit as a routine test.
