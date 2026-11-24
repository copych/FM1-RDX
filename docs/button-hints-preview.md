# Contextual button hints

Local preview 1.0.2-dev, build 282CF1C8; baseline 594ccae. Normal editor screens no longer duplicate the original GUI knob labels in a four-cell footer. The bottom area contains monochrome bitmap [BUTTON] ACTION hints. Operator pages offer SEL, shared pages name the relevant section button, SAVE opens Store, SEL+FX names the resulting FX action and HOME exits. Home offers editing, envelopes, saving and recording. Store, Drafts and Looper have actual button hints, including state-dependent confirmation/finish/cancel/start/stop actions. Store completion/errors only offer HOME. Knob assignments on custom pages sit beside the values, explicitly labelled K1/K2/K3/K4. The original editor image and top assignments are unchanged.

Hints are composed with their background into the existing eight-row canvas, at y182..239, without a separate blanking pass. The new footer does not allocate or alter audio generation, quantization, control bindings, storage, DMA ownership or cooperative audio checkpoints.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: matched AC79 SDK/pi32v2 with existing O2/Os flags; package/integrity checks preserved. build/hints-target.log; button-hints-artifacts.json. |
| Audio hot path/numerics | PASS (source/target): UI-only integer glyph work; DSP and block dispatch unchanged. No soft-double or forbidden multiply pattern; RAM closure passes. |
| Execution ownership/synchronization | PASS (source/host): serialized UI job and immutable state snapshots retained; eight-row canvas/source lifetime and cooperative checkpoints unchanged. |
| Memory/storage/I/O | PASS (static/host): image 205500 B; RAM code 22656/32768, data+BSS 91596/98304, pool 333572/344064. Footer clipped to 240px screen; flash format/map unchanged. Hardware stack peak unmeasured. |
| Dependencies/includes/licenses | PASS: no dependency/font/assets added; original bitmap font and component notices retained. |
| Compatibility/package/recovery | PASS (source/package): unchanged controls, voice/draft schema and FM-1_9900 loader/recovery contract; pinned installer and corruption rejection checks pass. Public release unchanged. |
| Host functional validation | PASS: full tools/test-app.sh on initial layout (build/hints-host.log), then final-context native editor/store/transport/quantization reruns (hints-editor/store/transport/quant.log); final spacing rerun Store/quantization. tests/emu-test.mjs and tests/site-test.mjs build/feature-site pass. Native Home, Store and triplet-Looper screenshots visually inspected, final Store spacing checked. |
| Hardware real-time validation | UNVERIFIED for this layout: check physical readability of button brackets/actions, state transitions and seven-voice UI activity with XRUN/LATE/LCD and WORK/TURN/Q counters. |

44.1 kHz, 128-frame block deadline 2902.49 us. No device flash, new hardware soak/stack measurement or full unrelated BSP suite performed. Existing compiler warnings and optional screen-reader voice omission unchanged. Earlier hardware testing remains applicable to its tested builds.
