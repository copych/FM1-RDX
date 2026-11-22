# Cached modulation coefficients — local Preview 2026.11.22-23

Based on checkpoint 15ec677 / Preview 22. Local installer `/install/`; previous
checked installer `/install-preview22/`. Timing footer **O2 CCH** identifies the
candidate. Six-voice ceiling and adaptive overload protection remain enabled.

## Change

PEG/LFO pitch enables are now cached as exact float 0/1 at parameter-update
time instead of converting eight integers per voice/sample. AMD's table
coefficient is cached from the original patch index; its enable is a separate
boolean, so index 0 retains the old bypass behavior. Cache refreshes in both
cacheParams and updateModParams. Modulation arithmetic/order, per-sample update
rates, shared pitch ratio, envelopes, waves and feedback are unchanged. No
additional block scratch, I/O, allocation, locks or waits in the hot path.

The actual target compiler audit found no double-promotion warnings and no
double/fpext/fptrunc operations in normal render IR before or after. Static
integer-to-float conversion sites in renderBlock fall from 95 to 79 (the mono
and poly bodies each lose eight). These are static code counts, not runtime
per-sample counts. Float-to-int sites remain 52. Object helper references remain
only memcpy, memset and plat_ticks24. Audit files are retained under build/.

Final target RAM code 21008 B versus 21328 B in Preview 22 (-320 B); renderer
local stack allocation 72 versus 76 bytes. Data+bss increases by 32 bytes for
eight cache/alignment additions. Smaller code and fewer conversions are build
evidence, not proof of device speedup. See FM6-SOURCE-REVIEW.md for the external
code comparison; no external code was imported into the firmware.

## Evidence

Source/platform baseline unchanged: fm1-base
7932dff6f1474cc8c3420e0df75eb75879e47319, BSP
f869dfa8dc3c081f040407e370d303349e4444d5, existing AC79 SDK / pi32v2 clang 4.0.1,
r3/fprev1, O2 engine/editor, FP contraction off, 44.1 kHz/128 frames,
2902.49 us block deadline. Same CPU0 audio / CPU1 application control ownership,
clocks, voltage, storage and loader. Build ID 88918BA5 is not a complete external
core source identifier; package/source hashes below identify the candidate.

PASS: tools/test-routing.sh and tools/test-profile.sh, 1188 exact scenarios each
with sanitizers, live algorithm/pitch-enable changes, new live AMD=127 change,
path switches, retirement, modulation, release/panic. Complete factory streams
(33 presets including random LFOs), each 25,952,256 bytes, compare against saved
Preview 22 output both dry and wet:

- Dry SHA256 f80985d8ceafee41b5fd4573506f68ff37f6f972c01c8b3496eeb165ce94e645.
- Wet SHA256 69baa9fae9c697d0176e3bcb295c665a38cbf67d26ea5883c9bd235a73668ac7.

Independent live-edit regression tests/modulation_cache.cpp is compiled against
the archived **15ec677** engine sources and separately against current sources
(UBSan/float-cast checks on current). Across 33 presets x 3 modes it changes
PEG/LFO flags, AMD 0/1/intermediate/127, wheel, bend, releases/retriggers while
voices run. Streams compare exactly: 6,488,064 bytes, SHA256
0b62df3c042c6a56e75a057b90460a161072140cf52eca4be7a8ccca53b63b5c.
Unlike paired tests sharing a new updateMods implementation, the archived
reference independently exercises the old integer flags and per-sample AMD LUT.

PASS: full tools/test-app.sh including controls/MIDI/Soundmondo/isolation and
bridge sanitizer checks; matched wasm and tests/emu-test.mjs (all 33 presets,
finite audio, FX, controllers, encoders, profile and panic). Importer changes
reproduce the new fields/initializers; repeated import makes no further change.
The final generated-header whitespace/label-only rebuild was checked with the
matching wasm. Pinned installer/package/links and git diff --check pass.

PASS: target build retains ABI/manifest, RAM closure, register, no-double-helper,
64-bit-load hazard, loader/package checks. Image 189996 B, RAM code
21008/32768 B, data+bss 61980/98304 B, pool 333572/344064 B. Existing FX/GUI and
compact-C host warnings remain. No new dependencies/assets or serialized changes.
Final disassembly/helpers: build/cached-target.dis, build/cached-helpers.txt.
Full source/artifact hashes: build/evidence.json.

Firmware SHA256 a0354624ad1cbe0463cef1529afa51fd4a8a539d7c294ac7f094143af6939900.
Wasm SHA256 33d7c2126d3771125fc5d8e7d1667b8ee50856495017f366f1bf57971e90089e.
No publishing/flashing by agent. Previous user-reported Preview 22 SYN 1926 us,
APP 2253 us, FX 341 us, 6V prediction 3370 us; active voices/XRUN not confirmed
in that run. Compare the same patch/chord/FX with profile OFF on normal SYN,
and report actual voices, APP/FX/6V/XRUN/over90 alongside it. Speedup is UNKNOWN.

## Compliance

| Category | Status/evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target compiler/platform and retained build guards; internal cache only, patch/command ABI unchanged. |
| Audio hot path / numerics | PASS — exact cached coefficients, unchanged sample cadence/order, target IR/helper checks and independent old/new audio comparisons; no new double/libm/I/O/waits/allocation. |
| Execution ownership / synchronization | PASS (source scope) — audio owner refreshes coefficients on existing parameter paths; no core/queue changes. |
| Memory / storage / I/O | PASS (static/build scope) — bounded caches/linker budgets, smaller code/stack, storage/peripherals unchanged; actual timing/contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — no new dependencies/assets or imported external implementation; original notices retained. |
| Compatibility / package / recovery | PASS (build/site scope) — same checked identity/loader/storage, pinned installer and Preview 22 rollback. Physical installation/recovery UNVERIFIED. |
| Host functional validation | PASS — exact routing/profile/factory streams, independent 99-case live edits, full app/bridge, final wasm and installer tests as described. |
| Hardware real-time validation | UNVERIFIED — normal SYN/headroom, workload/voices, XRUNs, live edits/listening and GUI/USB/MIDI soak needed. No extra-polyphony guarantee. |

Skipped: repeated full effects-specific sanitizer suite (effects unchanged; wet
equivalence run), external Felucca/Melodee builds/device benchmarks, physical
flashing/listening/recording, on-device timing, fault injection and soak.
