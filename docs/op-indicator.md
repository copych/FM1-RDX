# Per-operator page indication

Baseline 6a1a301; local build 41009A7B. Per-operator frequency, level/feedback, key scaling, AEG rates and AEG levels now identify OP 1..4 beside the page title in the top header. Header uses the same immutable GUI request operator as the displayed page, and is composed with its background on each change. Shared four-operator overview pages retain their existing column labels; shared pitch envelope/LFO pages do not imply a selected single operator. Factory patches, effects calibration and all audio processing are unchanged.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: matched AC79/pi32v2 toolchain, flags/submodules and existing manifest checks retained; target/web/native builds pass (build/op-indicator-target.log, op-indicator-web.log, op-indicator-host.log). |
| Audio hot path/numerics | PASS (source structural scope): only bounded UI header formatting changed, outside renderer. No DSP/math changes; target no-soft-double/RAM closure checks pass. |
| Execution ownership/synchronization | PASS (source/host): header derives operator from the existing completed immutable GUI snapshot; existing UI serialization/checkpoints unchanged, no new queues/waits. |
| Memory/storage/I/O | PASS (build/source bounds): image 210948 B; RAM code 22656/32768, data+BSS 92764/98304, pool 333572/344064. Context buffer grows from 11 to 40 bytes, bounded formatter writes at most39 plus NUL. No persistent/peripheral changes; device peak stack not remeasured. |
| Dependencies/includes/licenses | PASS: no dependencies, fonts, assets or includes added. |
| Compatibility/package/recovery | PASS (source/package): no patch/draft/settings/USB/loader changes; site hashes/corruption checks pass. Exact package/WASM identities in op-indicator-artifacts.json. |
| Host functional validation | PASS: tests/emu-test.mjs asserts all five per-op headers differ across four selections and return pixel-identically after wrap; full existing emulator suite passes. Native select-context and new op-indicator-controls scenarios pass; key-scaling OP4 screenshot visually inspected. Site tests pass. |
| Hardware real-time validation | UNVERIFIED for this header revision: cycle all four operators in the five pages using SELECT and SEL, check correct indication and clean replacement while playing, inspect XRUN/LATE/LCD counters. No new hardware measurements. |

Skipped: full unrelated DSP/BSP/storage suites and physical flashing; this patch changes UI only. Existing warnings remain. Local preview updated; no public release change.
