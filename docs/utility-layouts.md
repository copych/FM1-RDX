# Utility screens and name cursor

Baseline d0bfde8; local 1.0.2-dev build 79B2B265. Store and Draft save/load now share the original bitmap font, four equal columns with central vertical rule, large voice name and bracketed button hints. Store shows actual K1 position/K2 character/K3 bank/K4 slot assignments. Drafts shows four occupied/empty slots with an outlined selection; SELECT remains the slot dial. Confirmation and error messages remain contextual. Reference: original STORE/NAME images 24–27 extracted locally from the documented user manual sources. No reference assets redistributed.

The name cursor now composes a black 240x2 strip and the current white underline before one blit. This erases the former position in the same transfer. Rendering remains bounded strip-based control/UI work with existing audio checkpoints and canvas ownership; no renderer or synchronization changes.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: matched AC79/pi32v2 SDK, existing flags and manifest checks retained. Target/web/native builds pass; build/utility-target.log, utility-web.log, utility-native.log. Baseline submodules and toolchain unchanged from docs/draft-quantization.md. |
| Audio hot path/numerics | PASS (source/target structural scope): only control_frame and bounded utility drawing changed; no DSP, audio math, clocks or floating-point changes. Target no-soft-double/RAM closure checks pass. |
| Execution ownership/synchronization | PASS (source): existing serialized control/UI snapshot and cooperative audio checkpoints retained. No new waits, queues or cross-core state. |
| Memory/storage/I/O | PASS (static/build): image 210612 B; RAM code 22656/32768, data+BSS 92748/98304, pool 333572/344064. Small bounded stack strings and strips only; no flash layout, storage payload or peripheral changes. Device peak stack not measured. |
| Dependencies/includes/licenses | PASS: no dependencies, includes, fonts or runtime assets added; notices preserved. |
| Compatibility/package/recovery | PASS (source/package scope): existing voice/draft formats, loader/product and bootloader policy unchanged. Site tests verify pinned package hashes, corruption rejection and no automatic device commands. Artifact identity in utility-layout-artifacts.json. |
| Host functional validation | PASS: native tests/store-looper-controls.txt and tests/transport-drafts-controls.txt exercise save/overwrite/reboot/load; tests/emu-test.mjs passes including framebuffer assertions at cursor positions 0,2,9,1,0, browser persistence and quantization recall. Store and saved Draft screenshots visually inspected under build/store-looper-controls and build/transport-drafts-controls. |
| Hardware real-time validation | UNVERIFIED for this revision: on hardware move K1 across all name positions, edit/save/overwrite voices, select all four draft slots and confirm save/load/cancel. Check readability, old underline removal and XRUN/LATE/LCD counters while playing seven voices. No new hardware timing, peak stack or soak measurement. |

Skipped: full unrelated DSP/BSP suites (no DSP/platform edits), physical flashing and power-loss tests. Existing SDK/compiler warnings remain. Local preview only; public release unchanged.
