# 32-sample operator-block experiment — local Preview 2026.11.22-24

Local timing candidate, not hardware-qualified. `/install/` is Preview 24;
`/install-preview22/` retains the faster measured Preview 22 for comparison and
rollback. Timing footer: **O2 B32**. No publishing or flashing by agent.

The renderer/modulation baseline is **15ec677 / Preview 22**, not Preview 23's
unsuccessful coefficient-cache experiment. RDX_Voice.h and its importer were
restored to the Preview 22 integer pitch flags/per-sample AMD table calculation
before adding this experiment. Shared pitch-ratio reuse from Preview 22 remains.
Application controls/core ownership stay as in c63a147 and subsequent previews.

## Execution and numerical boundary

The 128-frame block still selects active slots and advances their block LFOs in
the original order. It then processes four chunks of 32 samples. For each chunk,
voices remain in their original addition order:

1. Generate 32 individual PEG/LFO/glide/pitch/amp modulation samples. Compute
   each sample's pitch ratio, sharing only when enable coefficients match.
2. Run OP4 over those samples, then OP3, OP2 and OP1. Select input routes and
   rectification mode once before each operator loop.
3. Sum carrier buses with original parentheses, apply the existing retirement
   fade, and add into that chunk's sample sums in voice order.
4. After all chunks, apply the original output gain/soft saturation and stereo
   copy. Effects remain outside this synth render, as before.

Every AEG, PEG, pitch, amplitude and feedback update still occurs **per sample**.
There is no chunk-held envelope, pitch increment, interpolated control substitute
or quantized phase. An operator's feedback/filter and phase recurrence retain
sample order. Sine/lookup/AEG multiplication order and signed wrapping are
unchanged. Disabled operators still advance all hidden state. Phase, feedback,
filter and static coefficients are local to the small operator kernel until its
loop completes. This is safe under existing sole audio ownership: commands and
patch/parameter changes cannot arrive halfway through rendering.

The operator has four RAM template kernels (input/no-input x rectify/no-rectify),
480/488/486/496 bytes respectively. Routing/rectification-mode branches are outside
their sample loops; sign tests, envelope transitions and other state-dependent
branches remain. Two RAM modulation-preparation variants are 880/988 bytes;
renderer/orchestration is 2198 bytes. These seven functions total 6016 bytes,
versus Preview 22's monolithic 7512-byte renderer. No external source code was
copied; the layout is informed by FM6-SOURCE-REVIEW.md.

One **1664-byte** static scratch object contains operator-major per-sample ratios,
amplitudes, four wave buses and a temporary input bus. It is reused serially
across chunks/voices by CPU0; CPU1 cannot access it. No per-voice block allocation,
dynamic allocation, I/O, new synchronization or waits in audio. Retiring voices
render only their remaining fade length; no extra state steps past retirement.

Initial target linking revealed that the old compiler outlined wrap01 into XIP
from a small RAM kernel. fast_floorf/wrap01 are now explicitly always_inline,
with unchanged equations. The full original RAM closure checks pass; no
allowlist, indirect RAM-to-flash escape or build guard was weakened.

## Baseline/build

fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319; BSP
f869dfa8dc3c081f040407e370d303349e4444d5; existing AC79 SDK / pi32v2 clang 4.0.1,
r3/fprev1, engine/editor O2, FP contraction off. Same 44.1 kHz, 128 frames and
2902.49 us deadline. Same CPU0 audio/CPU1 controls/LCD, six-voice admission ceiling
and overload policy, clocks, voltage, loader, storage and patch/command ABI.

Final image 188708 B; total RAM code **19832/32768 B** (1496 B less than Preview
22); data+bss **63612/98304 B**; pool **333572/344064 B**. Main renderer local
stack allocation is 76 bytes plus its callees/save frames: actual CPU0 peak stack
must still be measured. Smaller individual kernels do not prove faster execution;
scratch traffic and additional calls can counteract the register/routing savings.

## Host/build validation

PASS: tools/test-profile.sh, 1188 exact normal/coarse/fine comparisons, covering
all presets/algorithms/modes, live path switches, eight-note input, budget
retirement/stealing, modulation, release and panic, with UBSan/float-cast checks.
The profile retains the independent sample-oriented operator path.

PASS: new tools/test-block.sh, 1188 cases x chunk lengths **0..32**. Reference
stepRouted samples compare bit-for-bit with renderChunk, while sentinel tails
verify no writes beyond n in ratio/amp/wave/input arrays. Gate/retrigger,
operator disable/enable, modulation-enable changes and cache refresh occur live.
ASan/UBSan/float-cast checks pass. Paired tests substitute continuous waveforms
for process-global random LFOs; separate-process original-preset streams below
cover those random patches independently.

PASS: all 33 factory preset streams compare exactly against saved Preview 22,
25,952,256 bytes each:

- Dry SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645.
- Wet SHA256 69baa9fae9c697d0176e3bcb295c665a38cbf67d26ea5883c9bd235a73668ac7.

PASS: tests/modulation_cache.cpp, current block renderer with UBSan versus
independently compiled archived Preview 22: 99 preset/mode live-edit cases,
6,488,064-byte stream, SHA256
0b62df3c042c6a56e75a057b90460a161072140cf52eca4be7a8ccca53b63b5c.

PASS: full tools/test-app.sh, including MIDI/Soundmondo, controllers/UI, profile,
voice budget, CPU0/CPU1 fallback and bridge sanitizer tests. Matching wasm and
tests/emu-test.mjs pass all presets, finite audio, FX/controllers, encoders,
profile transitions and panic. Importer emits both new fragments reproducibly;
misc.h remains hand-maintained. Original source snapshot untouched.

PASS: target ABI/manifest, RAM closure (27 app RAM functions), register access,
no-software-double, 64-bit-load hazard, loader/package guards. Object undefined
references remain memcpy/memset/plat_ticks24. Existing GUI/FX and compact-C host
warnings retained. Evidence: build/block32-{target.dis,symbols.txt,helpers.txt},
build/block32-*.log, build/evidence.json. Pinned installer/package/link tests
and git diff --check pass. No new external dependency or imported assets.

Firmware SHA256 b67ffbc513c903eac70d5f3d8f937ff28a7a835a58f58463e666fd9886df602d.
Wasm SHA256 44f6cdc1702918bdd949efde7a88b6e0bb972d99f13de2dd4de3bc76188b9c0a.
Build ID 88918BA5 still omits external core inputs; use full source/package hashes.

## Hardware comparison

Use normal Audio Timing -> SYN, with Render Profile OFF, same preset/chord/FX,
CPU1-control setting and actual/admitted voices. SAVE resets displayed peaks;
wait until held voices are steady. Compare against Preview 22's user-reported
SYN 1926 us, APP 2253 us, FX 341 us, six-voice prediction 3370 us (that run's
voice count/XRUN were not supplied). Preview 23 reported SYN 1941 us and was not
selected as the performance baseline. These are user reports, not agent-run
benchmarks. Record SYN/APP/FX/6V/voices/XRUN/>90 plus audible behavior. Exercise
live pitch/modulation edits, sustain/release, dense MIDI/USB recording, GUI redraws,
multiple boots and a longer soak. No speedup/polyphony increase is claimed yet.

## Compliance gate

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — pinned foundation/compiler, supported flags and original guards, static-only scratch/internal methods; wire/patch ABI unchanged. |
| Audio hot path / numerics | PASS — exact per-sample recurrence/order, no coarsening/double/libm/allocation/I/O/waits, reference/bounds/sanitizer/stream tests and target helper/RAM guards. Device speed/behavior UNVERIFIED. |
| Execution ownership / synchronization | PASS (source scope) — single audio-owned scratch and voice state, same CPU0/CPU1 command boundary; no new synchronization. |
| Memory / storage / I/O | PASS (static/build scope) — fixed 1664 B scratch and explicit n limits, linker/code budgets fit, peripherals/storage unchanged; actual CPU0 stack/contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — self-contained block header/fragments, no external implementation/assets imported, original notices retained. |
| Compatibility / package / recovery | PASS (build/site scope) — same identity/loader/storage, checked pinned installer and Preview 22 rollback. Physical flashing/recovery UNVERIFIED. |
| Host functional validation | PASS — exact 1188 profile and 1188 variable-length block cases, independent factory/live streams, full app/bridge and matching wasm/installer suites. |
| Hardware real-time validation | UNVERIFIED — normal SYN/stack/headroom/XRUNs, listening, live edits, I/O stress and soak required. No higher-polyphony guarantee. |

Skipped: repeat effects-specific sanitizer suite (effects unchanged, wet stream
equivalence run), physical flashing/listening/recording, CPU0 stack/peripheral
timing measurements, real fault injection and hardware soak.
