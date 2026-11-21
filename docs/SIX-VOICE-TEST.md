# Six-voice budget experiment — Preview 2026.11.22-20

Local preview based on Preview 19 GUI offload. Admission budget now starts/resets
at six and can recover up to six. Existing 85% target, 80% growth margin,
1.5-second recovery delay and platform >90% overload shedding remain active.
This is not a forced six-voice mode. A conservative prediction or actual overload
can still reduce the current count. The eight-slot physical pool remains for
existing retirement fades; no engine ABI or persistent format change.

Audio Timing (SEL + REC once, profile OFF) now displays **6V** prediction and
MAX 6. Hold six notes on the same preset, reset peak readings with SAVE once
steady, and report APP, SYN, FX, 6V prediction, active/admission counts, CPU,
XRUN and >90%. SAVE resets displayed cost peaks, not the learned budget model;
patch changes reset the budget. Compare equal workload/FX and state the actual
voice count; a reduced count is not a measured six-voice workload.

Changed: src/rdx_budget.h (constant/initial admission limit), app/engine.cpp
(six-voice prediction), app/app.c (labels), matching budget expectations/tests.
No envelope, oscillator, routing, modulation cadence, GUI ownership or audio
buffer change. Prediction remains historical peak fixed cost plus peak
per-voice synthesis cost multiplied by six, not direct six-voice timing.

Build baseline/toolchain/source pins and ownership are as recorded in
GUI-CORE-SPLIT.md: fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319,
BSP f869dfa8dc3c081f040407e370d303349e4444d5, existing clang 4.0.1 r3/fprev1,
O2 engine/editor, FP contraction off, 44.1 kHz/128 frames, 2902.49 us deadline.
Target build retains all package/loader/manifest, RAM closure, register and
no-software-double guards. Image 188376 B, RAM code 20684/32768 B,
data+bss 59196/98304 B, pool 333572/344064 B. Build ID 9BBAADEE.

Validation: full tools/test-app.sh; tools/test-polyphony.sh with ASan/UBSan/
float-cast checks (six ceiling/reset, prediction, shedding, recovery/wrap,
idle/transition exclusion and unchanged eight-slot engine tests); matching
emulator build and tests/emu-test.mjs (33 presets, finite audio, FX/controllers/
profile/panic); pinned installer tests/site-test.mjs and git diff --check.
An initial ad hoc test compile omitted -Isrc and failed; the repository's test
script with the correct include path was then run successfully.

Firmware SHA256: 17276db79f5e86fb620b4bbad8802a793627f24ed2bd0aed682bb2f0206db782.
Wasm SHA256: 22bb6e5dca14ac9f0c430504ba511cd5999d4ce0b3971de8616b5205f5d3a4ca.
Full source/artifact hashes: build/evidence.json. No publication/flashing by agent.

| Compliance category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target build, foundation and ABI checks retained. |
| Audio hot path / numerics | PASS — bounded integer budget constant change, same DSP and margins; no new allocation/I/O/waits/double/math calls. |
| Execution ownership / synchronization | PASS (source scope) — existing CPU0 budget ownership and GUI-only worker retained; no synchronization changes. |
| Memory / storage / I/O | PASS (build/source scope) — same static pool/layout/buffer bounds and storage; budgets fit. Hardware contention remains UNVERIFIED. |
| Dependencies / includes / licenses | PASS — no dependencies/assets or license changes. |
| Compatibility / package / recovery | PASS (build/site scope) — same identity/loader/storage and pinned installer; physical installation/recovery UNVERIFIED. |
| Host functional validation | PASS — app, sanitizer polyphony, wasm and installer suites listed above. |
| Hardware real-time validation | UNVERIFIED — measure real six-note admission, maxima, shedding, underruns, USB and GUI contention. No six-voice real-time guarantee claimed. |

Skipped: repeat oscillator/envelope/effects equivalence suites (DSP unchanged),
physical flashing/listening, on-device timing/soak and recovery tests. Existing
GUI/FX compiler warnings remain. The previous local Preview 19/17 packages are
retained for rollback; this experiment is not hardware-qualified.
