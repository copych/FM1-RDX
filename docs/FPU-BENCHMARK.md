# FM-1 FPU and table-access diagnostic

This diagnostic measures the current compiler's kernels on the device. It is not an instruction-latency meter. No measured hardware results have been collected for this change yet.

## Running it

Hold SEL and tap REC once for Audio Timing, then repeat for DSP Benchmark. Press SAVE to start all 15 cases. Allow about 10 seconds; completion depends on main-loop cadence. SELECT browses the results, SAVE reruns, HOME exits. Leaving the screen pauses an unfinished run. The emulator exercises the same controls but deliberately reports zero timing and labels itself HOST: NO TIMING.

First run with no notes and both FX slots THRU. Record all cases' MIN and MAX ticks, plus the clock shown on Audio Timing. Then repeat while holding a four-note chord, with the same patch, and finally with the effects you normally use. Check XRUN and >90% counters before and after. Timings on loaded runs include audio interrupt interference; they are not isolated FPU timings.

## What is measured

Each main-loop tick times one bounded kernel with the existing 24 MHz platform timer. One warm-up invocation is discarded, then 32 elapsed samples are retained as minimum and maximum. Audio interrupts remain enabled; no locks, clock changes, allocation, storage or logging occur in the kernel. A volatile checksum consumes the result after the stop timestamp. Only the main loop owns the benchmark state and scratch table.

All kernels execute 64 iterations with four items per iteration (256 items). The target's empty inline-assembly register constraints keep arithmetic and conversions from being folded or moved out of their loops. Native/browser builds omit those barriers and cannot provide target timing. The generated pi32v2 object must be inspected with `objdump -d -mcpu=r3 -mattr=+fprev1`; omitting FPU decoding produces misleading unknown instructions.

Cases:

- EMPTY LOOP: same loop count and register barriers, without arithmetic.
- ADD / MUL / DIV DEP: four successive operations on one accumulator.
- ADD / MUL / DIV INDEP: four independent accumulator chains, exposing scheduling opportunities.
- FLOAT TO INT: float-to-signed-int truncation, with checksum accumulation and barriers.
- INT TO FLOAT: unsigned-int conversion, with float accumulation and barriers.
- FLASH / RAM READ: the same 256 float values and index sequence, with accumulation.
- FLASH / RAM INTERP: the same index sequence, two reads and linear interpolation, with accumulation.
- PHASE WRAP: the signed general wrap formula used for modulated lookup phase, including conversions.
- PITCH LUT: the existing semitonesToRatio function on a dependent input chain.

The flash table uses the first 256 entries of the existing sine LUT; the RAM copy is static and initialized before timing. Their 1 KiB working sets and repeated access are a warm-cache comparison, not a cold-flash or full-working-set benchmark. The interpolation tests have a fixed fractional weight and integer indexing; PHASE WRAP and PITCH LUT separately exercise conversion/indexing costs. Conversion cases include consumption overhead and must not be read as isolated instruction latency.

MIN ns/op = MIN ticks * 1000 / (24 * 256), rounded down. This includes loop, dispatch, entry/exit and timestamp overhead. EMPTY LOOP provides context, not an exact correction for every case. For relative comparisons, prioritize matched pairs: DEP versus INDEP, FLASH versus RAM. MAX also includes interrupts. The 24 MHz timer has 41.67 ns resolution; repeated operations amortize that quantization. No core-cycle counter or direct register access is added.

## Baseline and compliance

Parent main 2ff9f47, unchanged fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP f869dfa8dc3c081f040407e370d303349e4444d5. Existing AC79 SDK and pi32v2 clang 4.0.1/r3 flags, single audio owner, 44.1 kHz / 128 frames (2.90249 ms nominal deadline). The diagnostic is called only from app_frame, never app_render. Firmware DSP and adaptive-budget logic are unchanged.

- Target/build/ABI: PASS target compilation and retained register, RAM-closure, image and soft-double checks. Build logs retained under build/benchmark-target.log. Ten RAM functions close within 8,544 bytes of application RAM code; total RAM code 13,644/32,768; data+bss 29,516/98,304; pool 333,572/344,064. Existing source warnings remain.
- Hot path/numerics: PASS structural audit, no added renderer operations; kernel is bounded float-only arithmetic and existing LUTs. Target engine external references are memcpy, memset and plat_ticks24, with no soft-float/double/libm helpers. Diagnostic timing itself is UNVERIFIED until device runs.
- Ownership/synchronization: PASS source audit; all diagnostic state is main-loop-owned, interrupts stay enabled, no worker or shared audio scratch memory.
- Memory/storage/I/O: PASS static/link budgets and table bounds; no persistence or peripheral changes. Physical stack high-water and actual interference UNVERIFIED.
- Dependencies/licenses: PASS no new dependency or copied external implementation; existing component notices retained. Existing original-source licensing questions are unchanged.
- Compatibility/package/recovery: PASS target's existing package checks and unchanged patch format/loader; physical install/rollback UNVERIFIED.
- Host functional validation: PASS application suite including benchmark entry, selector wrapping, completion, rerun, exit and no storage writes. Matching browser build also required for delivery. Emulator performance is not hardware evidence.
- Hardware real-time: UNVERIFIED. Run the idle/chord/FX matrix above, record firmware identity and timer/clock assumptions, audible output and counters. No device was flashed here. No new polyphony claim is made.

Repeated DSP equivalence/sanitizer suites are skipped for this diagnostic-only change; the synthesis/effects algorithms are unchanged. Final artifact hashes and matching source hashes are recorded in build/evidence.json.
