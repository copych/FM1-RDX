# FM1-RDX 1.0.1 release validation

Release baseline: 6e2aa30; release changes are version identity, user documentation and publishing metadata/templates. DSP, UI rasterizer, execution model and storage formats are unchanged from that baseline. Build F47F6685; exact artifact hashes are in release-1.0.1-artifacts.json.

| Compliance category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS: matched AC79 SDK/pi32v2 compiler, existing O2 engine/editor and Os platform flags, FM-1 product. Target build and integrity checks in build/release-101-target.log. |
| Audio hot path / numerics | PASS (source/target): no DSP changes; target RAM closure and no soft-double / prohibited multiply checks pass. |
| Execution ownership / synchronization | PASS (unchanged source): CPU1 synth/control cooperative execution, CPU0 mix/FX/output and bounded handoffs retained. |
| Memory / storage / I/O | PASS (static/host): image 204536 B, RAM code 22656/32768, data+BSS 91628/98304, pool 333572/344064. Voice/draft schema, flash pause and updater layout unchanged. New peak hardware stack measurements not taken. |
| Dependencies / includes / licenses | PASS: no new runtime dependencies; component notices and bundled licences retained. |
| Compatibility / package / recovery | PASS (package/host): FM-1_9900 identity, loader integrity, SHA pins, corrupted-package rejection, no bootloader offer, no automatic MIDI request. Existing installation backup/stock recovery guidance retained. |
| Host functional validation | PASS: tools/test-app.sh, tests/emu-test.mjs and tests/site-test.mjs build/release-1.0.1. Logs build/release-101-host.log and release-101-emu.log. |
| Hardware real-time validation | Prior seven-voice hardware results and ongoing user testing retained. UNVERIFIED for the latest pixel UI: physical readability and seven-voice UI stress/stack check remain to be measured. No new flash, power interruption test or hardware soak performed in this release task. |

44.1 kHz, 128 frames per block; 2902.49 us block period. Existing hardware evidence and detailed feature validation: pixel-gui-preview.md, transport-drafts-preview.md and saving-looper-preview.md. Full unrelated BSP board suite was not rerun; focused LCD tests passed. Existing compiler warnings and missing optional screen-reader voice assets remain as documented in pixel-gui-preview.md.

Hardware follow-up: seven voices with heavy FX/modulation while rotating controls and switching Store/Looper/Draft/editor pages; inspect readability, XRUN/LATE/LCD errors, WORK/TURN/Q and stack peaks. Confirm persisted voices/drafts after reboot, USB MIDI/audio and metronome/LED behaviour.
