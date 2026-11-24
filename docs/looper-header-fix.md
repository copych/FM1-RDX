# Looper header redraw fix

Baseline: release 1.0.1, fca9040. Local build 8A20AAE2. The UI-only change removes the periodic LOOPER title repaint and composes header glyphs/background in each eight-row strip before submission. Header refresh still occurs on entry and explicit control invalidation. No separate blanking pass is sent. DSP, flash formats and synchronization contracts are unchanged.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: existing matched AC79 SDK/pi32v2 flags and manifest checks; build/header-target.log. |
| Audio hot path/numerics | PASS (source/target): UI-only integer work; no new DSP/math calls; target helper/RAM checks pass. |
| Execution ownership/synchronization | PASS (source): same serialized UI job, eight-row canvas lifetime and cooperative audio checkpoints. |
| Memory/storage/I/O | PASS (static): image 204600 B; unchanged RAM code 22656/32768, data+BSS 91628/98304 and pool 333572/344064. No storage changes. |
| Dependencies/includes/licenses | PASS: none added; existing notices retained. |
| Compatibility/package/recovery | PASS: unchanged persistent formats, FM-1_9900 package identity and update loader; pinned installer corruption checks pass. Public 1.0.1 artifacts are unchanged. |
| Host functional validation | PASS: tools/test-app.sh, tests/emu-test.mjs and tests/site-test.mjs build/feature-site; logs build/header-host.log, header-emu.log. |
| Hardware real-time validation | UNVERIFIED for this fix: check title stability while stopped, recording and playing; enter/exit Drafts/Home and rotate controls with seven voices. Observe XRUN/LATE/LCD errors. |

Full unrelated BSP suite, device flashing and hardware soak not performed. Existing compiler warnings and optional screen-reader voice omission unchanged. Exact local artifact hashes: build/evidence.json and build/feature-preview.json.
