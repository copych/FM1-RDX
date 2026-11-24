# Reface-style pixel GUI preview

Local version `1.0.0-beta.4-dev`, based on commit `17d7b16`.

Every RDX application screen now uses the existing original ESP `src/gui/font_mo.h` glyphs. The original editor drawings, topology, waveforms and envelopes remain unchanged. Surrounding headings, control labels, status text, Store, Looper, Drafts and diagnostic text use that same face instead of the platform's smooth fonts. Rendering is white on black, without antialiasing or coloured labels.

The outer text uses the same 15/8 magnification as the 128×64 editor on the 240-pixel LCD; large values use twice the logical font size. The main editor shows the current voice as context instead of repeating the original page heading. Store has a name cursor and divided CHAR/EDIT/BANK/SLOT controls. Looper has a progress bar and CLICK/REC/PLAY/TEMPO cells, with the same original bitmap type for tempo and voice names. Draft labels and confirmations are compact enough for the pixel grid. Existing physical button/knob bindings are retained.

Pixel drawing stays in the serialized CPU1 control/UI job. It uses the existing eight-row canvas and DMA lifetime rules, servicing the dry audio worker between glyphs and at canvas/DMA checkpoints. Glyph row/column positions are precomputed, avoiding divisions inside pixel loops. Control-cell borders use direct edges rather than testing every pixel for a grid boundary. No allocation, full-screen framebuffer, peripheral ownership or renderer/DSP change was introduced. All strips and screen-bottom drawing are clipped to bounds.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: FM-1, existing matched AC79 SDK/pi32v2 compiler and O2/Os flags; package, register-access, RAM closure and helper checks retained. See build/pixel-target.log. |
| Audio hot path/numerics | PASS (source/target scope): DSP/envelopes/audio callback unchanged; new integer glyph work is UI-only. Target reports no soft-double helpers or forbidden multiply pattern. |
| Execution ownership/synchronization | PASS (source/host scope): original immutable GUI/control replies, hashes, CPU1 serialization, cooperative audio checkpoints and LCD completion/source ownership retained. |
| Memory/storage/I/O | PASS (static/host scope): image 204552 bytes; RAM code 22656/32768, data+BSS 91628/98304, pool 333572/344064. Same eight-row scratch canvas; no storage writes or schema changes. Stack peaks and display timing remain unmeasured on hardware. |
| Dependencies/includes/licenses | PASS: reuses the existing bitmap font and original reference; no downloaded font/library or new runtime dependency. Existing source/asset notices retained. The BSP keeps its own system-UI fonts. |
| Compatibility/package/recovery | PASS (source/package scope): no MIDI, voice/draft format, object map, updater or bootloader change. Local firmware/emulator pins and installer corruption/identity checks pass. |
| Host functional validation | PASS: full tools/test-app.sh, including 1188 normal/dry exact cases and storage/control/LCD suites; final editor/Store/Looper/Draft native control reruns; tests/emu-test.mjs and tests/site-test.mjs. Visually inspected native screenshots and checked five editor/utility images contain only RGB black and white. |
| Hardware real-time validation | UNVERIFIED for the new UI rasterizer. The preceding firmware was reported working by the user. New physical readability and UI-under-seven-voice timing still need checking. |

Artifacts/hashes: `pixel-gui-artifacts.json`. Logs: build/pixel-host.log, pixel-controls.log, pixel-store.log, pixel-editor.log, pixel-emu.log, pixel-target.log and pixel-web.log. Review images are in build/editor-controls, build/store-looper-controls and build/transport-drafts-controls. Unrelated full BSP board suite and new device flashing, stack measurement and hardware soak were not performed. Existing upstream constructor-conversion/unused-variable and firmware-hub metadata warnings remain; the emulator retains its plain-text screen-reader fallback because voice assets are absent.

Hardware check: compare Home, operator/EG/LFO/FX, Store/Name, Looper and Draft pages against the supplied GUI reference. Confirm names/values and all four labels remain readable, and that rotation produces no blank intermediate frame. Run seven voices with intended heavy FX/modulation while rotating controls and changing screens; record XRUN/LATE/LCD errors, WORK/TURN/Q and stack figures. Check the physical LED behaviour and metronome are retained.
