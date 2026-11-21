# Four-voice timing baseline — Preview 2026.11.21-6

Hold SEL and press REC to open Audio Timing. SAVE resets counters; HOME exits. Keys, MIDI and PRESET still work. Parameter encoders are ignored on this screen. No storage writes occur. Timings accumulate while the screen is closed, allowing normal editing/redraw contention tests.

APP MAX is the maximum app_render time, in microseconds rounded upward. CTRL includes queued panel and MIDI handling; DSP includes the complete rdx_render call (synthesis, both effects, transitions and edits); OUT covers master smoothing and conversion. Stage maxima can come from different blocks and must not be added together. Four timer reads per callback use the platform's 24 MHz crystal timer with unsigned wrap-safe subtraction. Instrumentation bookkeeping after the last read is excluded from APP MAX. The platform's full render timer includes that bookkeeping and its surrounding work/interference.

CPU is the platform's smoothed full-render percentage, not a peak. XRUN counts platform DMA-half misses since reset. >90% counts full platform renders exceeding 90% of its budget via its existing APP_OVERLOAD hook; this is a warning count, not an underrun count. The nominal deadline is 128/44100 = 2902.49 us. Interrupt interference occurring inside timed spans contributes to their elapsed time. There is no voice shedding, idle skipping or polyphony change in this build.

Hardware test: close the installer after installation to release USB MIDI. Select a preset and let transitions settle, then reset. Record 30 seconds idle, reset and record 60 seconds sustained four-note chords and repeated notes with long release/sustain, then repeat with FX bypassed using SEL+FX outside the timing screen. Include slow attacks, MoDemLead and patches with delay/reverb. Test rapid preset changes and knob editing separately, resetting before each run; inspect the screen afterward. Report preset, FX, duration, APP MAX, CTRL/DSP/OUT, CPU, XRUN and >90%. Test the heaviest patches longer. An observed maximum is workload-specific, not a worst-case proof; compare an instrumentation-free build before final qualification.

Baseline: source parent 7c07e5e; unmodified fm1-base 7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP f869dfa8dc3c081f040407e370d303349e4444d5. Existing JieLi pi32v2 clang 4.0.1/r3 toolchain and AC79 SDK; unchanged app flags, single audio owner, 44.1 kHz, 128 frames, four rendered poly slots. Board FM-1; actual unit/bootloader UNKNOWN until user testing.

| Compliance category | Status and evidence |
| --- | --- |
| Target/build/ABI | PASS build: tools/build-fm1.sh, unchanged SDK/BSP/flags and integrity gates; build 5349DE60. Hardware ABI remains UNVERIFIED. |
| Audio hot path/numerics | PASS source/generated checks: four bounded timer reads, integer maxima and saturating counters only; no new allocation, I/O, float/double/math calls. Target audit finds no soft-double helpers or RAM-code calls. |
| Ownership/synchronization | PASS source: counters owned by audio ISR; main snapshots/resets under existing interrupt exclusion; UI formatting/drawing outside audio; overload hook constant-time. |
| Memory/storage/I/O | PASS static: image 207764 B, data+bss 21132/98304 B, pool 333572/344064 B, RAM code 5100/32768 B. No storage/layout changes. Device stack and contention UNVERIFIED. |
| Dependencies/includes/licenses | PASS change scope: no added libraries/assets; upstream notices and submodule retained. Existing original-source licensing remains unresolved. |
| Compatibility/package/recovery | PASS build package checks and site pin tests; no state format/loader changes. Real installation/rollback UNVERIFIED for this build. |
| Host functional validation | PASS full test-app scenarios, dedicated timing-controls entry/reset/knob isolation/exit/no writes, visual screenshot, WebAssembly 33 programs/MIDI/FX/encoder regressions. Host timing is not device timing. |
| Hardware real-time validation | UNVERIFIED: user device measurements, instrumentation overhead comparison, analog check, soak, stack and recovery pending. |

Skipped repeat DSP/envelope suites because DSP is unchanged; no device flash or measurements performed by the agent. Source and artifacts are hashed in build/evidence.json; published firmware SHA-256 is e9c8ef42afd222832f06af9f94435b81325ed6953fa429a8b1f10b91577d43d8.
