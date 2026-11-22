# Sample-local staged renderer — Preview 2026.11.22-22

Local, audio-equivalence-tested timing candidate based on c63a147 (Preview 21).
No hardware performance improvement is claimed. Installer `/install/`; previous
Preview 21 pinned installer `/install-preview21/` for comparison/rollback.
Audio Timing identifies the candidate as **O2 STG**.

## Changes and numerical contract

RdxEngine::renderBlock -> RDX_Voice::stepRouted now performs, within **each
sample**, updateMods/PEG/LFO/glide, four AEG advances, four phase advances, then
four feedback/wave/gain evaluations in OP4 -> OP3 -> OP2 -> OP1 routing order.
Old oscillator phases and envelope gains are sample-local floats. The existing
profile primitives implement exactly the same arithmetic as compute(). Feedback
recurrences, lookup wrapping, sine interpolation and carrier addition order are
unchanged. No envelope, modulation or pitch value is held for an audio block.
No new block scratch buffers, allocation, I/O, synchronization or waits in DSP.

A parameter-time boolean records whether all four operators have identical PEG
and LFO pitch-enable coefficients. If so, their phaseMod inputs are identical
(PEG, LFO pitch, wheel, bend and optional glide additions have the same order).
The semitone LUT is evaluated once **for that sample** and the exact ratio is
reused for four independent phase advances. Otherwise the original four LUT
evaluations remain. cacheParams/cacheRouting and updateModParams refresh the
boolean, including live edits. Original phaseMod calculations and their order
remain unchanged. Per-operator frequency/detune increments remain independent.

Initial pure staging built successfully but increased the renderer's local stack
allocation from 56 to 76 bytes (+20), indicating more register pressure, not
proof of any speedup. The final candidate retains that allocation and adds the
shared-ratio path. Normal renderer size is 7512 bytes versus baseline 6866.
Target RAM code rises from 20684 to 21328 bytes (+644); data+bss rises by 32 bytes
for eight voice flags/alignment. These tradeoffs must be measured on the FM1.
Routing selection branches remain; algorithm-specialized kernels are not part
of this experiment. The hardware gain may depend on the patch's pitch flags.

## Build baseline

Same fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP
f869dfa8dc3c081f040407e370d303349e4444d5, existing AC79 SDK and pi32v2 clang
4.0.1, r3/fprev1, engine/editor O2, FP contraction off, app/platform Os.
44.1 kHz/128 frames, 2902.49 us deadline. CPU0 audio and CPU1 application
controls/LCD ownership from Preview 21 are unchanged. Six-voice starting/maximum
admission budget, adaptive protection, clocks/voltage and loader/storage retained.

User-device baseline from Preview 21: CPU1 controls APP 2296/SYN 2004 us,
CPU0 controls APP 2405/SYN 2012 us, one million control jobs with zero reported
errors/faults/XRUNs over 290 seconds, control max 41501 us, stack 1400 bytes.
Workloads were not strictly matched; these are user reports, not agent-run tests.

## Validation — host/build evidence

PASS: tools/test-routing.sh, 1188 bit-exact reference-step/routed comparisons
(33 presets x 12 algorithms x 3 modes), 160 blocks each, algorithm changes,
attack/hold/release, mono retrigger, plus new live PEG/LFO enable divergence at
block 55 and convergence at block 65. One million positive-wrap comparisons and
boundary cases retained. UBSan/float-cast checks enabled.

PASS: tools/test-profile.sh, 1188 normal/coarse/fine bit-exact scenarios including
live path switches, retirement/stealing, eight-note input, modulation, release
and panic. No assertion failures or sanitizer errors. Continuous/random LFO
handling follows those suites' documented paired-state limitations.

Separate-process streams cover all 33 original factory presets including random
LFOs: normal staged render versus the unchanged coarse profile operator path,
each 25,952,256 bytes. Dry streams compare identical and match the prior baseline
SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645.
Wet streams also compare identical, SHA256
69baa9fae9c697d0176e3bcb295c665a38cbf67d26ea5883c9bd235a73668ac7.
Executables/audio retained as build/staged-{render,reference,normal,wet}*.

PASS: tools/test-app.sh, including Soundmondo, controllers/UI, profile, polyphony,
control-core fallback/ownership and bridge sanitizer tests. Matching wasm build
and tests/emu-test.mjs pass all 33 presets, finite audio, FX/controllers, panel,
encoder acceleration, profile transitions and panic. Final label-only edit was
checked with the rebuilt wasm; it does not affect DSP/control behavior.
tools/import-core.mjs regeneration preserves the current RDX_Voice.h hash
e0c38366ddeb59d53f0c5e5160c12e474898487430b4435494ba2135ac53a601;
no unintended regenerated-header changes. Importer retains cache invalidation.

PASS: target build/ABI/manifest, RAM closure, register-access, no-software-double,
64-bit-load hazard, package/loader guards. Image 190204 B; RAM code
21328/32768 B; data+bss 61948/98304 B; pool 333572/344064 B. Existing warnings
remain; no new library/helpers or dependencies. Final linked stack/size evidence
in build/staged-target.dis and build/staged-symbols.txt; baseline object/ELF in
build/staged-baseline. Source and artifact hashes: build/evidence.json.

Firmware SHA256 fa38111d7439c4cc4c306eafdc565c5861674a9d423921ee577aa5038546a6b8.
Wasm SHA256 ac630734558663e5a78cc27a9b21a4c201b1d05c9eebec2293fa936720b08308.
Build ID remains 88918BA5 because the platform's ID does not track all external
core inputs; the package hash and full source evidence identify this build.
Pinned installer/links/package tests and git diff --check pass. No publishing or
flashing by agent.

## Hardware check

Use the same preset, notes, FX and CPU1-control setting as Preview 21. Keep
Render Profile OFF and compare normal **Audio Timing -> SYN**, not Fine TOTAL.
Reset peaks with SAVE once held notes are steady. Record APP/SYN/FX, 6V estimate,
active/admitted voices, CPU, XRUN and >90%, plus audible behavior. Try both a
common-pitch patch and a patch with differing per-operator pitch-enable flags;
exercise live flag changes, bend, modulation, mono glide and release. Keep the
candidate only if measured benefit justifies added code/stack traffic. Run
longer combined GUI/USB/MIDI stress and listening checks before qualification.

## Compliance gate

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — pinned foundation/compiler, target build and retained guards. Internal voice size changes are static-only; patch/command ABI unchanged. |
| Audio hot path / numerics | PASS — independent stages reordered sample-locally, exact shared-ratio reuse, no new double/libm/I/O/allocation/waits; sanitizer/equivalence and target guards pass. Hardware speed UNVERIFIED. |
| Execution ownership / synchronization | PASS (source scope) — same CPU0 DSP/CPU1 controls and publication as Preview 21; cached flag updates remain audio-owned with parameters. |
| Memory / storage / I/O | PASS (static/build scope) — code/data/stack growth recorded, linker limits fit, no new block buffers or storage/peripheral changes. Actual stack/performance under load UNVERIFIED. |
| Dependencies / includes / licenses | PASS — existing primitives/headers and notices, no new external dependencies/assets. |
| Compatibility / package / recovery | PASS (build/site scope) — same identity/loader/storage, checked pinned installer and Preview 21 rollback. Physical flashing/recovery UNVERIFIED. |
| Host functional validation | PASS — routing/profile exact tests, full dry/wet factory streams, app/bridge and final wasm tests described above. |
| Hardware real-time validation | UNVERIFIED — normal SYN, latency/headroom, XRUNs, live edits, listening and soak required. No voice-count or speedup guarantee. |

Skipped: repeat full effects-specific sanitizer suite (effect algorithms unchanged;
wet stream equivalence run), physical flashing/listening/recording, device timing,
register-spill performance, real fault injection and hardware soak.
