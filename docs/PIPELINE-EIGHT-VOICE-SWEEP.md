# Guarded CPU1 polyphony sweep — Preview 2026.11.23-30

Local preview based on 40ba6c0 / Preview 29. Normal playback remains capped at
six; eight physical engine slots already exist. The new experiment forces six,
seven and eight notes only while its diagnostic runs. Nothing published/flashed
by agent. Installer: http://127.0.0.1:8790/install/;
rollback: http://127.0.0.1:8790/install-preview29/.

User's latest reported workload reached 6/6, WORK 2139 us, TURN 2207 us, QMAX
73 us at CONTROL, with LATE/XRUN/LCD ERR all zero. This is a hardware report for
that workload, not proof of eight voices or qualification across all patches.

## Hardware procedure

1. Install and close the installer. Choose the same sustained POLY patch, FX
   and controller settings used for the six-voice result; release your notes.
2. Hold SEL and tap REC seven times. Release SEL; turn SELECT to SYN CPU1.
3. Press HOME, then hold SEL and tap REC six times. The title must say
   **CPU1 VOICE SWEEP**. If it says VOICE COST SWEEP, CPU1 mode is not enabled.
4. Press SAVE. The instrument generates the test chord itself; do not play MIDI
   notes or change parameters during the sweep. It spends two seconds settling
   and three seconds measuring each of six, seven and eight voices, about
   fifteen seconds total unless stopped earlier. Internal patch modulation and
   existing FX/controller settings remain active.
5. After DONE or LIMIT, turn SELECT to rows 6, 7 and 8. Report status, requested
   ROW/actual ACT, END, WORK, Q, TURN, STEADY count and XRUN for each recorded row.
   Then report LATE and LCD ERR from the pipeline/LCD pages. Take screenshots
   before another SAVE, because SAVE starts a fresh sweep after completion.

SAVE stops an active run; HOME cancels/exits. Incoming note/patch/controller/
editor events also cancel it. The test releases its notes and restores the
previous normal admission limit and sustain pedal state on completion/cancel.
It does not recreate held notes from before the test. FX tails are not reset.
Mono patches are rejected. A naturally decaying patch that loses voices ends
with VOICE LOSS rather than reporting a falsely qualified steady occupancy.

The guard is 90% of 128/44100 seconds: approximately **2612 us**. It applies to
warmup too. It checks each completed job before launching another forced block.
An observed late job or outer CPU0 overload also ends the run. The first tested
high-cost block can still overrun or cause an audible dropout: the guard cannot
retroactively prevent it. LIMIT/zero STEADY means the stage was not sustained
successfully. If seven trips the guard, eight is not attempted; its empty row
must not be interpreted as a timing result.

## Metrics

For CPU1 rows, END is max(CPU0 callback time, publication-offset + CPU1 TURN)
for a real completed service period. TURN includes queue delay plus generation.
Its end timestamp precedes the short counter/publication tail; LATE checks
actual completion availability, including that tail. This is not cycle-exact
measurement of the completion barrier itself.
WORK is CPU1 generation time; Q is publication-to-start wait. The publication
offset is retained in the snapshot as LEAD. Thus overlapping CPU0 FX/output and
CPU1 work are not added as if serial, but a late dispatch does not receive a
fresh full-block allowance. END/WORK/Q/TURN fields are independent row maxima;
do not add maxima. Paired WORK/FX at the END maximum are retained internally.
All samples, including the settling interval, contribute to maxima. STEADY
counts only measurement blocks after settling. Actual occupancy is required to
match the requested count on every accepted sample.

Host/wasm exercise states and sound but report zero END/WORK/Q/TURN device
timings. The existing CPU0-only sweep still tests 0..6. Its snapshot now has nine
rows of eleven words (105 words including the six-word header), so unused rows
7/8 remain available for CPU1 results. The normal six-voice budget is unchanged.

## Ownership and safety — VERIFIED-SOURCE

The normal pipeline policy is frozen while testing. At an acquired boundary,
CPU0 initializes the stage's fixed notes (48,55,60,64,67,72,76,79; velocity 100),
sets the temporary limit, discards the preceding stage's output and records the
sequence of the first new job. Older completions cannot count toward a new stage.
Early dispatch before CPU0 FX/output remains enabled, so the test retains the
normal overlap. Completed-job measurements occur in CPU0 preparation before the
next dispatch; they never wait for CPU1. The completion-sequence comparison is
wrap-safe over this bounded test. END addition saturates rather than wrapping.

A late/outer-overload signal may arrive while CPU1 owns voices. CPU0 changes
only diagnostic flags then; it does not panic or mutate those voices. After
acquiring the completed job it records that failed job's data, releases the test
notes and restores normal admission. The ordinary budget cannot learn forced
test costs or count cleanup as a second overload. Manual cancel similarly
discards the forced completion. A retired/faulted worker aborts the diagnostic
without treating an incomplete buffer as a measured successful job. Existing
BSP startup/retirement/flash guards, cooperative LCD wait and queue bounds remain.

No envelope/modulation rate, operator math, gain staging, FX, normal voice
ceiling or audio buffer latency changes. The stage timer uses the existing
platform milliseconds; durations are bounded, but very slow envelopes longer
than five seconds are not fully characterized by this test.

## Build and validation

FM-1 / matched AC79 SDK / clang 4.0.1 pi32v2r3/fprev1; engine/editor O2,
platform/app Os, -ffp-contract=off. Platform 10c06ba5e59aeb5ef938337f1a1a9c0b7f0f9ebb,
BSP 9dd6c11d54011af6abc9ce5aaf1ba6da2f28a85f. 44.1 kHz, 128-frame block,
2902.49 us period. Product FM-1_9900, loader/storage/clocks/pins unchanged.
Target build 27FA05A6: image 198532 B; RAM code 22140/32768 B;
data+bss 68924/98304 B; pool 333572/344064 B; CPU1 stack allocation 8192 B.
Build checks retain SDK/manifest, soft-double, unsafe wide-multiply, direct
register and memory guards. Target disassembly shows bounded measurement paths;
DSP RAM code size is unchanged. Full source/artifact hashes: build/evidence.json;
installer pins: site/preview.json. Build ID alone omits external include inputs.

Commands: bash tools/build-fm1.sh; bash tools/build-emu.sh;
bash tools/test-app.sh; node tests/emu-test.mjs; node tests/site-test.mjs.
Sanitized tests cover all nine rows/bounds, timer wrap, default CPU0 0..6,
CPU1 6..8, actual occupancy, completion and late recording, deferred overload
recovery, preserved admission after cancel, endpoint overflow and snapshot sizes.
Native app controls run the full fifteen-second sweep, inspect seven/eight rows,
restore limit six and verify normal MIDI after cancel. Matching wasm checks
audible automatic test notes, completion/release and return to normal MIDI.
Existing bridge/measurement/policy/LCD and 1188 exact renderer tests pass.

The broader BSP audio fixture has a previously recorded unchanged Linux mmap
assertion failure (see COOPERATIVE-LCD-WAIT.md). It was not rerun as part of this
change; focused BSP LCD regressions remain in the affected suite. Browser/native
timing and simulated safety tests do not qualify physical concurrency/deadlines.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — matched target/guards, fixed internal diagnostic snapshot expansion; persisted/wire ABI unchanged. |
| Audio hot path / numerics | PASS (source/build/scoped host) — bounded rows/notes/sequences and saturating 32-bit endpoint math; no new allocations/I/O/libm/doubles/waits, original DSP and exact comparisons. Target overhead UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/scoped host) — force/restore only after acquisition, late signals defer mutation, failed completion captured once, preserved early overlap and normal admission; physical behavior UNVERIFIED. |
| Memory / storage / I/O | PASS (static/build) — expanded rows/snapshot fit limits, unchanged storage/peripheral/loader contracts and command bounds. Actual stack/I/O contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — local diagnostic/API/test changes; unchanged pinned MPL dependencies/notices, no imported assets/libraries. |
| Compatibility / package / recovery | PASS (build/site) — checked product/loader/package and Preview 29 rollback retained; physical flashing/recovery UNVERIFIED. |
| Host functional validation | PASS for affected app/sanitizer/wasm/LCD/installer suites; previously documented broader BSP audio mmap fixture limitation remains. |
| Hardware real-time validation | UNVERIFIED — measure rows 6/7/8 with actual occupancy, settling/steady counts, guard status, LATE/XRUN/LCD ERR and fixed patch/FX/controllers; no eight-voice claim yet. |

Skipped: physical flashing/recovery/fault injection, actual target sweep and
listening/USB recording, repeated boots and soak. A first over-budget test block
remains possible; this preview is a guarded measurement tool, not an eight-voice
production release.
