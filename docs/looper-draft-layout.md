# Looper layout from the user's draft and original references

Baseline 04a3a0d; local 1.0.2-dev build 006E51CE. The looper now has a centered title/state, narrow progress bar, CLICK/QUANT/TEMPO labels and values aligned in K1/K2/K4 positions, thin vertical rules, LOOP/LIVE voice names and the established uppercase-button/lowercase-action hints with three-pixel bottom margin. The redundant beat/instruction row is removed; transport state and take-full indication remain.

References: the user's supplied draft and the original LOOPER screen extracted as build/manuals/image28.png from refaceGUI.docx, whose original hash is recorded in manual-sources.json. Originals are not redistributed. The original CLEAR/REC/PLAY slider assignments are adapted to our click/quant/tempo controls, as requested; FM-1 transport remains on physical buttons. This changes layout only, not parameter bindings, sequencing, DSP or calibrated output.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: matched existing AC79/pi32v2 build and integrity checks, build/looper-draft-target.log. |
| Audio hot path/numerics | PASS (unchanged source/target): no audio or calibration changes; target helper and RAM closure checks retained. |
| Execution ownership/synchronization | PASS (source/host): same serialized control/UI job, immutable snapshot, eight-row canvas, cooperative glyph/DMA checkpoints and source lifetime. Header remains outside periodic redraws. |
| Memory/storage/I/O | PASS (static): image 209860 B; RAM code 22656/32768, data+BSS 92716/98304, pool 333572/344064. No heap, storage or peripheral changes. |
| Dependencies/includes/licenses | PASS: existing bitmap glyphs and user/reference layout only; no new runtime asset or dependency. |
| Compatibility/package/recovery | PASS (source/package): original voice/draft formats, calibrated audio, USB/SysEx, update loader and SHA-pinned installer checks unchanged. Public release untouched. |
| Host functional validation | PASS: native transport/draft and quantization scenarios; tests/emu-test.mjs; tests/site-test.mjs build/feature-site. Visual stopped/OFF and 1/32T screenshots checked against draft and original looper reference. Logs build/looper-draft-controls.log, looper-draft-quant.log, looper-draft-emu.log and looper-draft-shots.log. |
| Hardware real-time validation | UNVERIFIED for the new layout: check readability, parameter changes, progress/transport transitions and seven-voice UI load with XRUN/LATE/LCD counters. |

Full DSP/application and unrelated BSP suites were not repeated for this layout-only change; previous calibration validation remains recorded in level-calibration/README.md. No device flashing, timing/stack measurement or hardware soak performed. Exact hashes: looper-draft-artifacts.json. Native review screenshots: build/looper-draft-shots/looper-stopped.png and looper-32t.png.
