# LCD section timing — local Preview 2026.11.22-28

Based on 73c3dc8 / Preview 27. Local installer:
http://127.0.0.1:8790/install/; rollback:
http://127.0.0.1:8790/install-preview27/. No publishing or agent flashing.

User-reported Preview 27 baseline: WORK 1434 us, TURN 2147 us; QMAX 723 us
with paired WORK 1283 us, AT LCD / 0V; APP 379 us, FX 298 us, voices 4/4,
WHY 4 and LATE 0. These reports do not pair the overall WORK/TURN maxima or
prove which operation occupied the queued interval. The zero-voice queue
maximum motivates measuring GUI/LCD work independently of synthesis.

## Test on FM-1

After installing and closing the installer, hold SEL and tap REC seven times.
Release SEL and turn SELECT until SYN CPU1. Turn K1 once to enter **GUI / LCD
PROFILE**. SAVE resets timing maxima/counters (not the adaptive voice model).
Hold the same chord for 30 seconds and report WAIT, CLEAR, TEXT, SUBMIT, EDITOR,
QMAX/AT, WORK/TURN, voices, LATE and XRUN. Repeat after moving encoders/changing
screens. K1 returns to the paired queue view; HOME leaves diagnostics without
disabling the pipeline. SELECT still toggles CPU0/CPU1 generation in either
subview. CPU1 stack and control faults remain available on the isolation page.

Each new number is an independent maximum for ONE operation, in microseconds:

- WAIT: lcd_sync draining the previously submitted pixel DMA, including its
  existing hardware wait/deselect path.
- CLEAR: cv_begin after that drain, primarily canvas setup/clearing; its existing
  internal lcd_sync now normally finds no pending DMA.
- TEXT: one glyph through the original cv_text rasterizer, not a whole row.
- SUBMIT: cv_blit after the previous pixel DMA has drained. This includes LCD
  window/command handshakes and submission of new pixel DMA; it does not wait
  for that new pixel transfer to finish.
- EDITOR: rdx_editor_draw producing the 128x64 bitmap. Scaling/blitting of that
  bitmap occurs later and is not included here.

Do not sum maxima as if they describe one block. QMAX and the adjacent WORK on
the queue view remain a paired job measurement. New AT tags WAIT, CLEAR and
SUBMIT identify checkpoints; a tag says where service happened, not necessarily
where all preceding delay accumulated. Individual UI intervals exclude audio
service performed at their boundaries. On CPU0 they can include interrupt
preemption; CPU0-vs-CPU1 wall times are not direct compute-speed measurements.

## Implementation / contexts — VERIFIED-SOURCE

Only the serialized control/UI job updates the five peak/call-count pairs.
CPU0 snapshots them before dispatch and resets them after control-job completion,
using the existing acquired/interrupt-excluded main-loop boundary. CPU1 draws
from that immutable snapshot. The array size has a compile-time assertion.

The application explicitly drains previous LCD DMA before canvas begin and blit,
then yields at the boundaries of WAIT/CLEAR/SUBMIT. Those calls already drained
the previous DMA internally; their internal synchronization is retained. This
does not change SPI clocks, pins, LCD commands or source-buffer ownership, and
does not introduce a DMA wait in audio. It adds a scheduling opportunity between
draining DMA and clearing or submitting, plus diagnostic timer/counter overhead.
The existing bounded LCD waits and canvas publication barriers remain intact.
Screen fills use the same instrumented eight-row canvas/blit path.

UI timing uses unsigned 24 MHz subtraction with timer-wrap behavior, bounded
indices, saturating call counters and explicit resets. Host/browser UI durations
are zero; host tests still count operations. UI counters are separate from
voice state and never touched by the audio renderer. The dry pipeline, original
sample arithmetic, FX, admission model, command queues, BSP retirement, flash
gates and one-block latency are unchanged from Preview 27. Checkpoint overhead
and changed scheduling still require hardware timing/listening validation.

## Build / tests

Existing FM-1 / AC79 SDK and clang 4.0.1 pi32v2r3/fprev1; fm1-base
7932dff6f1474cc8c3420e0df75eb75879e47319 and BSP
f869dfa8dc3c081f040407e370d303349e4444d5. Engine/editor O2, platform/app Os,
-ffp-contract=off; 44.1 kHz / 128 frames, 2902.49 us period. Product FM-1_9900,
loader, storage layout, clocks, peripherals and 8192-byte CPU1 stack unchanged.

Target build 2D2D529B: image 197288 B; RAM code 22140/32768 B;
data+bss 68556/98304 B; pool 333572/344064 B. Builder checks retain soft-double,
unsafe wide-multiply, register-access, SDK/manifest and memory guards. Generated
code is reviewed for UI timer boundaries; DSP RAM code size is unchanged.
Full source/artifact hashes: build/evidence.json; pinned installer:
site/preview.json. Platform build ID alone does not hash all DSP includes.

Validation: bash tools/build-fm1.sh; bash tools/build-emu.sh;
bash tools/test-app.sh (includes sanitized bridge, voice-measure, pipeline,
policy, UI-timer and 1188 exact dry-render comparisons); node tests/emu-test.mjs;
node tests/site-test.mjs. Native tests enter/leave the LCD subview and assert all
four LCD operations were recorded. UI-timer tests cover independent maxima,
snapshot, reset, wrap, invalid indices and counter saturation. Matching wasm
checks finite audio through the LCD subview and return to CPU0. Host timing is
not evidence of device DMA or multicore behavior.

## Compliance gate

| Category | Status and evidence |
| --- | --- |
| Target / build / ABI | PASS — existing compiler/SDK and build guards; bounded internal snapshot additions only. |
| Audio hot path / numerics | PASS (source/build/host) — no new audio calls, DSP arithmetic unchanged; UI-only 32-bit bounded instrumentation. Actual checkpoint overhead UNVERIFIED. |
| Execution ownership / synchronization | PASS (source/host) — serialized UI owner, completed-job snapshot/reset, retained barriers, no audio wait; actual device behavior UNVERIFIED for this preview. |
| Memory / storage / I/O | PASS (static/build) — timing/snapshot bounds and linker budgets fit, retained DMA ownership and LCD waits, no storage/peripheral changes. Target stack/latency/contention UNVERIFIED. |
| Dependencies / includes / licenses | PASS — local UI helper and existing APIs only; no imported code/assets or changed notices. |
| Compatibility / package / recovery | PASS (build/site) — same checked package/product/loader, pinned Preview 27 rollback; actual flashing/recovery UNVERIFIED. |
| Host functional validation | PASS — full native app and sanitizer suites, LCD subview/counter assertions, matching wasm and installer checks. |
| Hardware real-time validation | UNVERIFIED — measure the procedure above with UI, dense MIDI/Soundmondo and USB recording, plus listen for glitches, check faults, boot cycles and soak. |

Skipped: physical flashing/recovery/fault injection, actual LCD/DMA timings,
hardware listening/recording, repeated boots and soak. No reduced queue delay,
higher polyphony or real-time qualification is claimed before device testing.
