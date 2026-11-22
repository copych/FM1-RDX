# Cooperative LCD DMA wait — local Preview 2026.11.22-29

Based on f0635b7 / Preview 28. Local installer:
http://127.0.0.1:8790/install/; rollback:
http://127.0.0.1:8790/install-preview28/. Nothing published or flashed by agent.

User-reported Preview 28 repeat: WAIT 915 us, CLEAR 405 us, TEXT 387 us,
SUBMIT 12 us, EDITOR 422 us / CPU1; QMAX 688 us at WAIT, WORK 1435 us,
TURN 2114 us, LATE 0, XRUN 0, current voices 0/4. Maxima can include earlier
sounding notes, and individual maxima are not paired. These are hardware reports
from the user, not a controlled agent measurement or a worst-case bound.

## Device comparison

Install Preview 29 and close the installer. Hold SEL and tap REC seven times,
release SEL and turn SELECT until SYN CPU1. Turn K1 once for GUI / LCD PROFILE.
Press SAVE to reset profiling maxima, hold the same chord for 30 seconds, then
report WAIT/CLEAR/TEXT/SUBMIT/EDITOR, QMAX/AT, WORK/TURN, voices, LATE, XRUN and
LCD ERR. Repeat with encoders/screens active. K1 returns to the paired queue
view, where the WORK adjacent to QMAX belongs to that delayed job.

WAIT now excludes time spent servicing audio INSIDE the DMA wait; a reduction
can mean useful overlap rather than a faster SPI transfer. QMAX/TURN/LATE and
voice admission are the useful end-to-end comparison. LCD ERR is the existing
BSP timeout counter since boot, not cleared by SAVE. The additional checkpoints
and polling overhead remain part of the real workload. Do not claim improved
polyphony until the same workload is measured without late blocks or LCD errors.

## Ownership and implementation — VERIFIED-SOURCE

The BSP adds fm1_lcd_wait_service(service, clock, timeout_ticks). Its status poll
checks for completion before calling service and invokes service every 64 polls.
The service callback runs only pending CPU1 dry generation, never LCD commands,
canvas changes, storage, USB transport or GUI work. SPI DMA continues transmitting
the existing canvas while synthesis runs. The existing completion publication /
voice ownership barriers govern audio exactly as before. No wait moves into the
CPU0 audio callback, and the callback never needs an unfinished worker buffer.

The original fm1_lcd_wait remains unchanged for legacy/loader/CPU0 callers.
The cooperative variant retains the 4,000,000-poll ceiling, counted timeouts,
pending-bit clear and SPI-on behavior. A 100 ms elapsed ceiling additionally
limits time extended by audio service; it is checked every 64 polls and cannot
preempt a running callback. Unsigned timer subtraction handles wrap. The platform
adds lcd_sync_service: normal completion precedes deselection/busy-clear, so the
canvas stays owned by DMA during callbacks. The existing best-effort behavior
after a hardware timeout is retained; this patch does not redesign LCD recovery.

Only CPU1 GUI waits use the cooperative path. CPU0 waits, loaders, initializers,
SPI clocks/pins, LCD command/submission paths and original transport timeouts
are otherwise retained. Screen clearing starts after the sync returns. The
callback's elapsed ticks are accumulated by the serialized UI owner and excluded
from WAIT with a bounded saturating subtraction. Sampling/checkpoint overhead
inside that callback is excluded too. Other timing sections are unchanged.
LCD ERR snapshots occur on CPU0 after the UI job has retired; the GUI receives
only that captured count. No register access is added to application code.

The synth kernels, envelopes, modulation, FX, adaptive model and one-block voice
pipeline are unchanged from Preview 28. Other gaps (CLEAR/TEXT/EDITOR) remain
and may become the next scheduling limit after DMA waiting is improved.

## Build and revisions

Local dependency commits on codex/rdx-lcd-wait:
fm1-base 10c06ba5e59aeb5ef938337f1a1a9c0b7f0f9ebb;
felucca-bsp 9dd6c11d54011af6abc9ce5aaf1ba6da2f28a85f.
Both are pinned by the root submodule/source-lock and have not been pushed.
Existing MPL notices are preserved. Build evidence now hashes the changed LCD
platform/BSP files too, and the site refuses stale platform-source evidence.

FM-1 / matched AC79 SDK / clang 4.0.1 pi32v2r3/fprev1, engine/editor O2,
platform/app Os, -ffp-contract=off; 44.1 kHz, 128 frames, 2902.49 us period.
Target build A5800905: image 197336 B, RAM code 22140/32768 B,
data+bss 68572/98304 B, pool 333572/344064 B; CPU1 stack still 8192 B.
Product FM-1_9900, loader, storage, clocks and peripheral configuration unchanged.
Full artifact/source hashes are in build/evidence.json and installer pins in
site/preview.json. Target disassembly confirms the CPU1 status-poll loop contains
the audio service call and both bounds; CPU0 still calls the original lcd_sync.
RAM/DSP guards, soft-double/wide-multiply checks and SDK/manifest checks pass.

## Tests and limits

Commands: bash tools/build-fm1.sh; bash tools/build-emu.sh;
bash tools/test-lcd-wait.sh; bash tools/test-app.sh;
node tests/emu-test.mjs; node tests/site-test.mjs.

Focused BSP LCD tests use the real driver with fake registers. They check service
before simulated completion, unchanged DMA address/count/CS ownership, immediate
completion, elapsed and original poll ceilings, callback time included in the
elapsed bound, clock wrap, timeout counters, SPI-on/pending-clear behavior and
null-service legacy fallback. Existing panel bounds/RGB565/timeout tests pass.
UI timing tests check subtraction of nested audio and invalid-exclusion clamping.
App/bridge/pipeline/measurement/policy regressions and 1188 exact dry-render cases
pass, as do matching wasm and installer checks. Host/wasm do not simulate a real
DMA wait or concurrent cores; actual scheduling is still a hardware test.

An initial broader BSP board run also attempted the UNCHANGED audio_test.c and
failed at its Linux fixed-address mmap assertion (samples == ptr(0x1C08000)).
The LCD tests had passed before that failure; audio_test.c and fm1_audio.h are
unmodified. The focused LCD runner selects lcd/lcd_hw from the upstream driver,
with CRLF-safe script loading. This broader host fixture limitation remains
unresolved and is not reported as a passing board suite.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — matched SDK/compiler and target guards, local dependency commits pinned; internal helper/snapshot changes, unchanged persisted/USB ABI. |
| Audio hot path / numerics | PASS (source/build/scoped host) — no audio-ISR waits/allocations/I/O/doubles; bounded UI polling/callbacks and 32-bit clocks, unchanged DSP and exact comparisons. Hardware service cost UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/LCD mocks) — canvas remains DMA-owned during service, callback does not touch LCD/GUI, existing voice publication and legacy waits retained. Physical multicore/DMA behavior UNVERIFIED. |
| Memory / storage / I/O | PASS (static/build/LCD mocks) — linker limits fit, retained source-buffer/pending-clear contracts and timeout counters, no storage/pin/clock changes. Real stack, SPI and fault recovery UNVERIFIED. |
| Dependencies / includes / licenses | PASS — minimal local MPL platform/BSP changes with notices retained; existing clock API passed explicitly, no new libraries or assets. |
| Compatibility / package / recovery | PASS (build/site) — product/loader/storage and pinned rollback retained, platform-source hashes checked. Physical installation/recovery UNVERIFIED. |
| Host functional validation | PASS for affected app/LCD/scoped sanitizer/wasm/installer suites; broader BSP audio fixture FAIL at the unchanged host mmap assertion described above. |
| Hardware real-time validation | UNVERIFIED — run the comparison with UI, dense MIDI/Soundmondo, USB recording, listening, faults/counters, boot cycles and soak. |

Skipped: physical flashing/recovery/fault injection, actual DMA/multicore timing,
listening/recording, repeated boots and soak. The broader BSP audio fixture is a
recorded failed host check, not a substitute for hardware audio qualification.
