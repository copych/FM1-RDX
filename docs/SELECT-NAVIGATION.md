# Contextual SELECT — Preview 2026.11.21-10

SELECT no longer traverses all 21 screens. Dedicated EDIT/ENV/LFO/FX/GLO/REC buttons still enter and cycle their sections.

| Current screen | SELECT rotation |
| --- | --- |
| AEG Rates / AEG Levels | Wrap operators 1–4, retaining the current page |
| OP Frequency / OP Level-FB / Key Scaling | Wrap operators 1–4, retaining the current page |
| Home / Level / Feedback / Frequency / Algorithm | Cycle these five overview pages |
| LFO / LFO Pitch On / LFO Amp Depth | Cycle these three pages |
| PEG Rates / PEG Levels / Pitch EG On-Off | Cycle these three shared pitch-envelope pages |
| FX1 / FX2 | Cycle the two effect pages |
| Voice / MIDI | Cycle the two global pages |
| Phaser Lab | Wrap the five tuning selections |
| Audio Timing | Ignored |

Negative and multiple-detent rotation wrap within that context. Holding SEL while rotating consumes the button gesture, preventing an extra operator increment on release. Changing page/operator resets parameter-knob acceleration. Existing short SEL taps and dedicated-button behaviour remain. PEG rates/levels belong to the shared voice envelope; its last page enables that envelope separately for each operator.

Implementation is main-loop-only integer navigation; no DSP, USB, MIDI, storage, queue, interrupt or parameter layout changes. Parent 1522f60, unchanged FM-1 submodules/toolchain/SDK/flags (pi32v2 clang 4.0.1/r3 and existing AC79 SDK). Single-owner audio 44.1 kHz/128 frames; deadline 2.90249 ms.

Compliance: target/build/ABI PASS build  EBB3B0FE, SDK/manifest checks retained; hot-path/numerics PASS scoped source/generated review (no audio change, bounded main-loop integer wrap, no doubles or new runtime dependency); ownership/synchronization PASS same main-loop UI state; memory/storage/I/O PASS static (image 210744 B, RAM data+bss 23452/98304 B, pool 333572/344064 B, RAM code 5100/32768 B; no layout/write change); dependencies/licenses PASS no runtime libraries/assets/submodule changes, existing notices retained; compatibility/package PASS local identity/checksum/loader/site pins, no saved format change; host validation PASS full test-app, contextual forward/backward/multiple-detent/operator/button-release assertions, unchanged original controls/preset-home/timing, wasm 33 programs/MIDI/FX/encoder and assembled site tests. Hardware ABI, sound, real-time, stack/soak and actual install/recovery remain UNVERIFIED. No flash performed by agent.

Skipped repeat DSP/envelope/long-hold and full platform suites because renderer and platform sources are unchanged. Physical follow-up: enter AEG, rotate SELECT in both directions, confirm OP1–4 and stable rate/level page; test LFO/FX/global page loops, parameter edits on each selected operator, and preset return Home. Firmware SHA-256 1b69c55df672ecbdee7656df9041225c0340ff63c763026fafa9fe3cecde6353.
