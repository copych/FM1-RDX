# FM1-RDX v1.0.0-beta — 2026-11-23

Seven adaptive voices are the normal CPU1 ceiling. CPU1 starts automatically
when available and generates dry voices, including during cooperative LCD waits;
CPU0 owns mixing, FX and output. Target: 90% of a 2902.49 us block, with 0.5
percentage-point growth hysteresis and overload/late reduction. CPU0 fallback
retains its original six-voice ceiling/85% policy. The algorithm dial opens its
editor screen. Firmware version is 1.0.0-beta.

The author tests the firmware regularly on M-VAVE FM-1 hardware. Reported checks
include playback, USB MIDI and Soundmondo. The latest fixed-chord sweep reported:

| Voices | END / WORK us | Steady blocks | XRUN |
| --- | --- | --- | --- |
| 6 | 2192 / 2138 | 1034 | 0 |
| 7 | 2534 / 2491 | 1034 | 0 |
| 8 | 2842 / 2737 | 0 | 0 |

Seven completed the measured stage. Eight exceeded the 90% guard without steady
qualification and remains diagnostic-only. Earlier six-voice cooperative-wait
reports had LATE/XRUN/LCD ERR zero. These are user reports for those workloads,
not a universal bound across presets/effects.

Current website/README replace obsolete hardware-testing-pending notices with
this actual testing status. RAM-only edits, unavailable patch saving/phrase
looping and other real user-visible limitations remain documented.

Revisions: platform be6a5ca (full pin in source-lock.json), BSP 9dd6c11; published
on codex/fm1-base-rdx and codex/felucca-bsp-rdx in this repository. Target
F0CB9E88: image198612 B, RAM code22140/32768 B, data+bss68924/98304 B,
pool333572/344064 B, CPU1 stack8192 B. Matched AC79 SDK / clang4.0.1
pi32v2r3/fprev1, O2 engine/editor, Os platform/app, -ffp-contract=off,
44.1 kHz/128 frames. Product, loader, storage, clocks and pins retained.

Validation: target/matching wasm builds; full native app including default CPU1,
limit seven/seven actual voices, shortcut, CPU0 profiling, sweep/restoration,
MIDI/SysEx and bridge/policy/timing/LCD sanitizers; 1188 exact renderer cases;
browser tests across 33 programs, controllers, pipeline/sweep and panic; installer
package/assets/links/hash checks. Evidence hashes include modified platform
sources. Assets: firmware, web bundle, SHA256SUMS and build evidence.

Compliance:

- Target/build/ABI PASS: matched SDK/compiler/guards; version/startup/policy/UI
  changes, unchanged stored/USB wire formats.
- Audio numerics PASS: unchanged sample-rate DSP/FX and RAM code, no new double,
  allocation, I/O or waits in rendering; target audit and exact comparisons.
- Ownership/synchronization PASS in source/scoped tests: enables the existing
  acquired-buffer pipeline at boot, retains CPU0 fallback/overload handling.
- Memory/storage/I/O PASS for build/source: limits fit, no layout/pin/clock or
  loader changes. Hardware stack/contention coverage is workload-dependent.
- Dependencies/licenses PASS: publicly fetchable pinned modified MPL sources,
  notices retained, no new DSP libraries/assets or altered SDK checks.
- Package/recovery PASS for artifact/site checks: same product/loader, pinned
  hashes and retained prior local installers. No agent flashing/recovery.
- Host functional PASS for affected native/sanitizer/wasm/site suites. The
  previously recorded unchanged broader BSP audio mmap-fixture failure remains
  a host-test limitation outside this scoped suite.
- Hardware evidence PASS for the author's reported FM-1 playback/MIDI/Soundmondo
  and seven-voice measured stage. Additional default-startup/90%-policy soak,
  broad preset/FX extremes and boot/recovery/fault/recording comparisons are
  UNVERIFIED in this agent session. This is not a claim that the firmware has
  never been tested on hardware.

Skipped here: physical flashing/recovery/fault injection and an agent-controlled
hardware soak. This is the explicitly requested GitHub beta prerelease.
