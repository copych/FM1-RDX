# Applied effect calibration after DC removal

Baseline 60e5196. This supersedes that revision's decision to retain the old tables. Fresh DC-filtered, AC-RMS effect measurements have now regenerated src/fx/level_calibration.h and are included in the normal firmware and emulator. Factory preset bytes, operator levels, patch gain and preset bank content are unchanged. Calibration is strictly post-effect makeup plus input-power interpolation; no per-preset normalization was added.

454 training requests cover both effect parameters at 0/32/64/96/127, four signal levels for distortion/wah and representative levels for linear effects. 444 requests were valid; ten delay time127 requests were rejected by the existing arena budget. The time127 table endpoint retains the earlier policy of using the highest measured supported endpoint (96). No memory limit was weakened. The full raw inputs are grid.json and extra.json. Seventeen captures initially reached the output ceiling; they were remeasured with 10x capture headroom and restored to the original units before deriving gains. The generator rejects nonfinite/zero/peak-limited training rows. No peak-limited capture is used as a training value.

The offline WebAssembly capture build bypasses only the calibrated effect makeup. All DC blockers and original effect processing remain active. In the current reproducible capture build, engine output is attenuated by 0.1 after FX; measurement tools accept captureScale=10 and restore that factor before computing per-channel mean-removed RMS. The capture macro is rejected by non-Emscripten compilation, is absent from normal app/fm1.toml, and the build helper restores flags with an EXIT trap. Never package the capture emulator. Its assets are isolated in build/calibration-emu; the installer uses the separately rebuilt normal app/build output.

Input power knots are now expressed on the pre-FX bus: measured dry output power divided by 0.90 squared, reversing the common final output scale. This corrects the previous scale mismatch between the table knots and the runtime pre-FX power estimate. The reciprocal spans were regenerated consistently. THRU gains remain unity, table bounds remain 0.25..4, and touch-wah retains its existing live power-based compensation. 695 of 800 stored gain coefficients changed; coefficient deltas versus the previous tables range from -0.040 to +0.355 dB. The power-scale correction also changes interpolation between input levels.

Independent normal-build validation: 24 maximum-output cases, 156 FX settings outside the training grid, 64 serial chains (all accepted), and 132 factory/Init dry/wet DC cases. AC RMS effect differences are -1.37..+1.88 dB relative to dry. All sample-peak ceiling and DC assertions pass. This is RMS matching on test signals, not equal perceived loudness for every timbre or preset. Factory presets intentionally retain their own dynamics and levels.

Reproduction (run builds sequentially; the capture helper temporarily edits and restores app/fm1.toml):

```sh
bash tools/build-calibration-emu.sh
node tools/measure-levels.mjs build/calibration-emu 10
node tools/measure-extra-levels.mjs build/calibration-emu 10
node tools/derive-level-calibration.mjs build/fx-level-baseline.json build/fx-level-extra.json
bash tools/build-fm1.sh
bash tools/build-emu.sh
node tools/test-levels-emu.mjs
node tools/audit-dc-emu.mjs
```

For this recorded sweep, most measurements preceded the added 0.1 capture attenuation, so their implicit capture scale is 1; the seventeen replacement rows explicitly record captureHeadroom=10. tools/remeasure-clipped-levels.mjs performs that selective recapture. Both sets use identical RMS units. The generator defaults to the checked-in DC-corrected dataset; older datasets remain available explicitly by path. Firmware/WASM/table/preset/capture hashes and validation summary are in artifacts.json. Built-in build ID remains 79B2B265; use the new package SHA256 0cd1c31143525723d93fbb508de1dd022447785575d0edd50cb65ace54cbf7bc to identify this preview.

| Compliance category | Status / evidence |
| --- | --- |
| Target/build/ABI | PASS: unchanged matched AC79/pi32v2 SDK, flags, submodules, board/bootloader contract and manifest checks; tools/build-fm1.sh and tools/build-emu.sh pass, build/recalibration-target.log and recalibration-web.log. Capture flags restored and absent from production build. |
| Audio hot path/numerics | PASS (source/target structural): production processing is unchanged except table constants and input-power knots. No new production per-sample operations, doubles, libm, allocation, I/O or waits. Target no-soft-double, RAM closure and register checks pass. Offline math uses host JS only. |
| Execution ownership/synchronization | PASS (source): existing CPU0 FX/makeup and CPU1 dry synthesis ownership, checkpoints, queues and publication unchanged. Capture-only code is excluded from normal firmware. |
| Memory/storage/I/O | PASS (build/sanitizer bounds): image 211188 B; RAM code 22656/32768, data+BSS 92764/98304, pool 333572/344064. Table dimensions, arena size, state and storage layout unchanged. Effect arena canaries and rejection cases pass. |
| Dependencies/includes/licenses | PASS: no dependencies or imported assets; existing notices retained. Offline build helper and measurement tools only. |
| Compatibility/package/recovery | PASS (source/package): factory preset source unchanged, hash retained; no user-bank/draft/settings/USB/loader/bootloader changes. Pinned site package/asset hashes, corruption rejection, unsupported browser and no automatic device command checks pass. Public release unchanged. |
| Host functional validation | PASS: tests/emu-test.mjs, tools/test-effects.sh under ASAN/UBSAN, tools/test-output-level.sh, 244 independent level/ceiling cases and 132 DC cases. Logs build/recalibration-emu-test.log, recalibration-effects.log, recalibration-output.log, recalibration-validation.log and recalibration-dc.log. Full previous baseline tools/test-app.sh passed for the unchanged production control/DSP logic; not repeated for this coefficient-only production change. |
| Hardware real-time validation | UNVERIFIED for these coefficients: compare dry/wet across both FX slots and parameter extremes on FM-1, especially distortion/wah, low bass and sustained/released notes; check transitions, output headroom and XRUN/LATE with seven voices, GUI and USB capture. No new device timing, peak stack, analog output or soak measurements. |

Skipped: ten unsupported delay settings (arena rejection, documented above), exhaustive perceptual loudness tests, full BSP/source-ESP suites, physical flashing/power-loss testing and public deployment. Existing SDK/compiler warnings and optional screen-reader voice omission remain. This preview changes only effects calibration relative to the preceding DC-fixed firmware.
