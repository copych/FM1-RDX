# Compiler optimization comparison — Preview 2026.11.22-17

Four isolated candidates were built with the existing pi32v2 Clang 4.0.1/r3 toolchain. Production fm1.toml was restored after the experiment. No device was flashed or public site updated. Candidate installers are local test artifacts, not a selected faster release.

| Candidate | C++ optimization | Explicit loop hint | Image bytes | Total RAM code bytes |
| --- | --- | --- | ---: | ---: |
| O2 | -O2 | none | 184004 | 20264 |
| O3 | -O3 | none | 188916 | 20264 |
| O2 U4 | -O2 | selected four-operator loops | 184824 | 21084 |
| O3 U4 | -O3 | selected four-operator loops | 189736 | 21084 |

`o2_cflags` applies to both engine.cpp and editor.cpp. The platform/app unity build remains at its original -Os level. The selected pragma applies to the per-sample updateMods four-operator loop and the diagnostic's four-operator envelope/pitch loops; it does not request unrolling of the large dynamic active-voice loop. `RDX_TEST_UNROLL` is off in production. Clang uses `#pragma clang loop unroll_count(4)`; native GCC comparisons use its corresponding unroll pragma. No new restrict aliasing promise, fast-math, FP reassociation/contraction, clock or RAM-layout policy was introduced. FP contraction stays off.

## Findings

- All four pass the target integrity checks, full host application controls, all-33-preset audio comparison, 1,188 UBSAN equivalence scenarios per candidate, matching browser builds and worklet tests.
- The normal renderer is 6,866 bytes for every candidate. Its object instruction-byte SHA256 is identical for all four: 8330fe9a26819823bf53b6ddb81f99b329245708354b167550793c8ae9f282f5.
- O2 and O3 have identical instruction bytes across all engine-object RAM sections, and identical normalized renderer relocation records. O3 adds 4,912 image bytes outside those kernels. There is no code-generation evidence of a faster normal RAM renderer from -O3 alone.
- U4 changes some diagnostic kernels and adds 820 bytes to linked RAM code. The normal renderer instruction bytes remain unchanged; its relocation records differ with the changed object layout. Identical instruction bytes are not a proof of identical linked addresses, helper placement, cache behavior or physical execution time.
- All four separate-process host audio streams are byte-identical: 25,952,256 bytes, SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645. This covers the original random LFO patches as well as deterministic ones. It does not establish target bit identity or real-time safety.

These selected hints have not exposed a clear normal-render improvement. Do not choose a winner from host timing, image size or diagnostic-stage timings. Local device comparisons remain optional evidence about linked-layout effects; no speedup is claimed.

Subsequent user-device report: all candidates showed approximately 1986–1987 us, with no observed distinction. The report does not specify the screen/field, workload, run duration or counters. This range is also close to the previously reported Fine Render TOTAL, so normal Audio Timing SYN versus diagnostic TOTAL must be distinguished before treating it as a normal-render A/B result. As reported, it provides no evidence of a compiler-option speedup. O2 remains the baseline; no optimization level has been promoted on the basis of these numbers.

## Device A/B

Open the local `/compiler/` comparison page. Start with O2, then O3; U4 candidates are also available. Each installer uses the existing updater/package checks and pins its own package hash. The Audio Timing footer identifies O2, O3, O2 U4 or O3 U4 alongside the clock. Normal production configuration remains O2.

Close the installer tab after each install. Use exactly the same patch, four notes, velocity, FX and USB state, with Render Profile off. SEL + REC once opens Audio Timing; SAVE clears peaks. Hold/retrigger the same workload for an equal duration and record SYN, APP MAX, voices, CPU, XRUN and >90%. Avoid changing patches or enabling the profiling/benchmark pages during a run. Note sound or recording changes. A repeat O2 run after candidates helps distinguish build effects from run variation.

First compare with both FX slots THRU to isolate synthesis. Then repeat with the usual heavy effects and record FX as well as SYN: effects/control code outside the RAM kernels can change under O3 even though the normal RAM synth instructions do not. No timing benefit for that surrounding code has been measured here.

## Reproduction and provenance

Run `wsl -d Ubuntu -- python3 tools/compiler-experiment.py` from the workspace. It builds candidates sequentially, records flags/config and artifacts under build/compiler-experiment, and restores the original fm1.toml in a finally block. A WSL PATH Node executable initially failed to launch; the completed run uses the existing tools/node-wsl.sh loader. Initial incomplete output is superseded by results.json.

`tools/inspect-compiler-experiment.py` parses ELF symbol section indices (the compiler emits several sections with the same .app_ram name), extracts the actual renderer bytes, and compares relocation targets by symbol name. Its code-comparison.json reports raw section bytes without linker alignment, so their total can be two bytes below the linked RAM size. `tools/build-compiler-comparison.mjs` refuses candidates missing any required successful check, validates package identity/checksums/loader and hashes, and produces a separate local installer for each.

The main Preview 17 installer is rebuilt at restored O2 and its own matching emulator. Exact source/artifact hashes are in build/evidence.json; each candidate retains its effective fm1.toml, target log, engine object, ELF/disassembly, symbols/helper list, native tests, matching emulator and results.json entry. fm1-base and BSP remain pinned at 7932dff6f1474cc8c3420e0df75eb75879e47319 and f869dfa8dc3c081f040407e370d303349e4444d5. Existing AC79 SDK/flags, one audio core, 44.1 kHz / 128 frames (2.90249 ms nominal) remain.

## Compliance

- Target/build/ABI: PASS all four builds and retained image, RAM closure, register and soft-double guards. Largest image 189736 bytes; RAM code at most 21084/32768; data+bss 46460/98304; pool 333572/344064. Actual device ABI execution and stack high-water UNVERIFIED.
- Hot path/numerics: PASS bounded optional loop directives, no new heap/I/O/logging/waits/math calls; helper references remain memcpy, memset and plat_ticks24. Host numerical evidence above. Target output and operation timing UNVERIFIED.
- Ownership/synchronization: PASS source audit; no change to audio/main-loop ownership, queues, locks or core startup. Build label is a constant returned to UI.
- Memory/storage/I/O: PASS static/link budgets and unchanged storage/peripherals. Candidate RAM/code growth is recorded; physical cache/stack/contention effects UNVERIFIED.
- Dependencies/includes/licenses: PASS no external dependency or implementation; original notices retained. Core importer regeneration is identical with the optional hint hook. Existing original-source licensing uncertainty remains.
- Compatibility/package/recovery: PASS isolated configs, automatic restoration and retained package/loader validation; same patch format and voice budget. Physical install/rollback and USB recording retest UNVERIFIED.
- Host functional validation: PASS full application suite and actual WebAssembly worklet checks for each candidate, 4752 total equivalence scenarios with UBSAN, and four matching full audio streams. The candidate-specific logs and result booleans are retained.
- Hardware real-time: UNVERIFIED. No device has run these candidates under agent observation, and no compiler winner or higher polyphony has been established. Use the same-workload procedure above.

Broader effects/manual sanitizer suites were not repeated separately because their source and numerical flags are unchanged; full app/browser tests still exercise effects and MIDI behavior. No global production optimization level was raised.
