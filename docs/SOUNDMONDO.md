# Soundmondo compatibility preview — 2026.11.21-11

Implemented the ESP RDX_Midi.h protocol baseline on the pinned FM-1 platform, plus the missing checked bulk receiver. USB product is now `FM1-RDX reface DX`, preserving the existing manufacturer and USB VID/PID. Current Soundmondo public client matches `/reface DX/` in both input/output names; its panel parser requires a 15-byte Yamaha identity reply with family 00 41 and model 53 06. Those checks were inspected in the live public scripts on 2026-11-21. No Yamaha USB IDs or manufacturer string are borrowed. Name detection on actual OS MIDI ports remains hardware-dependent.

Primary references: [Yamaha Data List pp.6–9](https://data.yamaha.com/files/download/other_assets/7/794817/reface_en_dl_b0.pdf), [Soundmondo public client](https://soundmondo.yamahasynth.com/assets/soundmondo-client-d0bac5d24560ba0c59c9a17117922f40.js), [Soundmondo panel MIDI parser](https://soundmondo.yamahasynth.com/components/reface-panel/reface-panel.js). These were inspected, not copied into the application or redistributed. Original ESP snapshot remains unchanged. The earlier feasibility discussion is superseded by this implementation.

## Behaviour

- Universal identity request returns the same 15-byte Reface DX identity used by ESP (including its version bytes).
- Voice dump request 0E 0F 00 returns header, common, four operators, footer with Yamaha lengths/checksums. System request returns system then voice, matching ESP behaviour.
- Bulk receiver checks model, seven-bit data, exact size and checksum. It stages a header/footer transaction, accepts common/operators in any order, supports partial subsets as documented, rejects transactions containing invalid/irrelevant blocks and expires incomplete transactions after two seconds. Standalone common/operator blocks are supported.
- Voice parameter changes merge into the current staged edit, preserving consecutive updates before audio consumes them; normal edits use the existing editor path instead of restarting every held note. Structural effect/mode changes use existing fade/clear handling. Parameter requests return parameter-change replies (four bytes for master tune, two for tempo, one for other addresses).
- Entire candidate voice must pass engine bounds and fit the fixed effect arena before publication. Full imports replace the current RAM edit buffer, mark it dirty, return Home and fade through the existing prepared-effect path. They do not overwrite factory voices or flash. Imported patches use standard gain; the bank-specific MoDemLead trim is not applied to arbitrary downloaded voices based solely on the previously selected preset number.
- System RX channel and FM control fields affect the engine. Other system bytes are retained for exchange only; this does not implement master tuning/transposition, looper, local-off or other absent Yamaha system features. Reface voice KSC-R remains metadata pending calibration.
- USB recording remains 16-bit PCM stereo/44.1 kHz. Its device name also changes with the USB product string. Browser MIDI part is enabled for complete SysEx transport, without creating a virtual system MIDI port.

## Ownership, bounds and failure handling

APP_SYSEX runs on main loop, at most 640 bytes; parser, validation, checksums, snapshots and reply generation stay outside audio. Main publishes a 150-byte voice or 32-byte system through a mailbox protected by the platform's short interrupt exclusion. Only app_render consumes it. Full imports and ordinary edits have distinct handling; multiple main-loop updates coalesce. The renderer adds constant-time pending checks and bounded copies/dispatch; no logging, heap, file access or waits.

Replies use a fixed 1024-byte queue. Requests reserve room for a complete largest response; excess requests are ignored instead of emitting partial voice dumps. app_loop sends at most one frame per pass after checking rounded USB-ring space, so the existing send API does not wait for host consumption. Unread queued replies expire after two seconds. USB session changes discard pending imports/transactions/replies. SysEx input is independent of channel-voice RX filtering. Both transport rings and updater handling remain upstream code; unrecognized messages fall through to platform handlers.

The current preset label still denotes the last factory slot; imported name and dirty marker denote its RAM edit. A separate persistent imported bank and saving are not implemented. Parameter SysEx edits do not force Home; full voice imports do. No bank dumps, live parameter transmission from panel edits, DIN SysEx or Yamaha firmware updates are claimed.

## Validation / compliance

Parent ef5ea1b; unchanged fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 / BSP f869dfa8dc3c081f040407e370d303349e4444d5, pi32v2 clang 4.0.1/r3 and existing AC79 SDK. Single audio owner, nominal 44.1 kHz, 128 frames, deadline 2.90249 ms. Four provisional voice slots.

| Category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS target build with retained manifest/SDK/helper/register checks; build 738D8A5E. USB product/APP_SYSEX/APP_LOOP hooks intentionally enabled, emulator MIDI part enabled. Actual OS enumeration/ABI UNVERIFIED. |
| Audio hot path/numerics | PASS scoped source/generated audit: parser/response work main-only, bounded audio handoff to existing patch/editor routines, no new per-sample DSP/doubles/libm/heap/I/O/logging. Target reports no soft-double helpers or RAM-code calls. Actual cycle cost UNVERIFIED. |
| Ownership/synchronization | PASS source: single audio owner, short interrupt exclusion for mailbox/snapshots, main-loop response pump checks capacity; timeout/disconnect paths tested. Mixed channel-message/SysEx ordering is block-level, not sample-accurate. Hardware contention UNVERIFIED. |
| Memory/storage/I/O | PASS static build: image 213224 B, data+bss 24892/98304 B, pool 333572/344064 B, RAM code 5100/32768 B. Fixed mailbox/RX/TX buffers; no storage writes/layout changes. Main/audio stack peaks, USB timing and analog behaviour UNVERIFIED. |
| Dependencies/includes/licenses | PASS scoped: existing pinned platform SysEx/MIDI APIs, no external library, asset or modified submodule; ESP source baseline and upstream notices retained. Original-source licensing remains unresolved. |
| Compatibility/package/recovery | PASS local package identity/checksum/loader/site pin and installer regression; no state/bootloader changes, unrecognized SysEx forwarded. Physical updater coexistence/install/rollback UNVERIFIED. |
| Host functional validation | PASS tools/test-soundmondo.sh ASan/UBSan/float-cast-overflow: 33 authentic fixture byte-exact round trips, identity, audio-owner handoff, malformed/checksum/transaction/timeout/session/backpressure cases. PASS full test-app with generated real-hook load/capture/parameter/RX/no-write scenario. PASS actual wasm MIDI transport identity and all 33 round trips, plus existing wasm program/controller/FX/navigation checks and assembled site tests. |
| Hardware real time / live service | UNVERIFIED: no local FM-1 or authenticated live Soundmondo transfer; no device flash, timing, USB record+MIDI/SysEx contention, stack, long session or recovery measurements performed by agent. Protocol tests do not establish service compatibility or Yamaha audio equivalence. |

One malformed-fixture test initially included the complete frame in its truncation loop; its boundary was corrected and tests rerun. Device-number changes are intentionally accepted under omni; corruption checks exclude that valid change. Skipped full platform, repeat envelope/long-hold and unrelated sound-identity suites because underlying DSP is unchanged; existing application/wasm regressions used. SysEx transport code and numeric bounds were newly audited.

Hardware follow-up: install, unplug/replug, close installer, open Soundmondo in desktop Chrome/Edge and permit SysEx; verify green connection, download a distinct voice, play it, capture it back without publishing anything, edit parameters, repeat across multiple voices and reconnect. Check 60-second sustained playback plus UI/MIDI/USB recording load using Audio Timing. Report connection step, OS/browser, received name and failure text if any. Do not accept complete compatibility until live load and capture succeed.
