# FM-1 benchmark photo results — complete idle set

Initially transcribed from ten photos in D:/exchange, received 2026-11-21 at approximately 23:15 local time. A subsequent explicit attachment set contains all 15 completed DSP Benchmark cases (CASE 15/15 REP 0) and Audio Timing. Filenames below refer to that complete attachment set. The nine previously supplied case values and Audio Timing values are unchanged; this is not evidence of an independent repeated benchmark run. This is user-device photographic evidence, not a locally instrumented hardware run. The UI matches the Preview 14 diagnostic; exact installed package identity is not visible in the photographs.

| Case | MIN ticks | MAX ticks | Displayed MIN ns/item | Photo filename |
| --- | ---: | ---: | ---: | --- |
| EMPTY LOOP | 35 | 36 | 5 | 2af8b239-5fa3-4b6a-a683-63b193369936.jfif |
| ADD DEP | 69 | 70 | 11 | caa23edb-c8cf-46c6-aa0d-7a8b955ae2dc.jfif |
| ADD INDEP | 69 | 70 | 11 | 74430b0c-e8cb-4450-abde-dd598e268f6b.jfif |
| MUL DEP | 69 | 70 | 11 | cb0aa277-ca31-48c9-802f-10338bc851df.jfif |
| MUL INDEP | 69 | 70 | 11 | 42a75b21-2ab5-4acb-ad79-f61522238b81.jfif |
| DIV DEP | 103 | 104 | 16 | 3dcbf461-b001-4efd-b82b-7e0aab9c7ec8.jfif |
| DIV INDEP | 103 | 247 | 16 | b59ea6df-811c-45a1-873b-bd682ffb5ed7.jfif |
| FLOAT TO INT | 69 | 212 | 11 | b0712537-e3b0-4f39-ae7a-5dd326654ae8.jfif |
| INT TO FLOAT | 90 | 285 | 14 | 0f9c677d-c879-4536-9264-f3baeb9e2df4.jfif |
| FLASH READ | 176 | 320 | 28 | 81e9e768-1548-4baf-ad97-4909a061de00.jfif |
| RAM READ | 193 | 427 | 31 | c24057bb-758e-42cb-be4d-291d9ae1dcf8.jfif |
| FLASH INTERP | 364 | 501 | 59 | 4e697cfa-89ea-46be-b02b-0168f50878ec.jfif |
| RAM INTERP | 398 | 542 | 64 | 24065063-3cb9-41fc-b115-932afb7df7bb.jfif |
| PHASE WRAP | 226 | 422 | 36 | d4e3fa07-0174-4aab-ac98-8c55233ba5a0.jfif |
| PITCH LUT | 415 | 632 | 67 | 07bcb168-0a7b-496f-b294-723da821b9c6.jfif |

The user confirmed that no notes were sounding during the benchmark. Treat this as the idle/no-sounding-notes set; normal audio interrupts remained enabled by the diagnostic. FX slot types, residual effect tails, USB activity and repeated-run consistency are not established.

All 15 cases are now transcribed; no cases remain missing.

Audio Timing photo 6611f92a-6f62-464f-bb2c-039c5067cd16.jfif: APP MAX 1870 us; SYN 1841 us; FX 8 us; predicted 5V 2441 us; CPU 64%; XRUN 0; >90% 0; elapsed 24 s; P7; FX ON; voices 4/4, MAX 8; CLK 360 MHz. FX ON indicates the chain enable state, not whether the slots contain processing effects. The 24-second counters do not establish a long soak or worst-case guarantee. Four active voices on this screen do not contradict the user's no-notes benchmark statement: the photos show different screens at different times.

## Interpretation

- The matched RAM-read kernel's minimum is 9.7% slower than flash (193 versus 176 ticks). RAM interpolation is 9.3% slower (398 versus 364). These are warm 1 KiB table comparisons under enabled interrupts, not cold-flash tests. They do not support a blanket LUT-to-RAM change, nor prove RAM intrinsically slower. Address/bank/cache/interference effects require controlled repetitions to distinguish.
- The empty kernel takes 35 ticks minimum (1.458 us). ADD and MUL each take 69 ticks, DIV 103. Subtracting the empty minimum gives approximately 5.534 ns/add or multiply and 11.068 ns/divide. With the displayed nominal 360 MHz clock, these are 1.992 and 3.984 cycles/item, consistent with approximately 2 and 4 cycles. This is a differential kernel estimate with timer/clock assumptions, not a direct cycle-counter latency measurement. The arithmetic bodies retain four actual FPU instructions per iteration in the audited target disassembly.
- DEP and INDEP minima are identical for add, multiply and divide. Independent chains provide no measurable minimum-time advantage in these particular compiled loops. This does not establish a general scheduling rule for mixed DSP code or every operand class.
- The independent division kernel has a minimum of 4.29 us for all 256 items, or 16.76 ns/item before subtracting overhead. Its maximum is 247 ticks versus 104 for dependent division; interrupt/interference timing can explain a difference, but the photos do not identify its cause.
- The pitch lookup kernel is 17.29 us total minimum (67.55 ns/item), and phase wrapping is 9.42 us total minimum (36.78 ns/item). These are substantial compound operations compared with the arithmetic-only division kernel. They remain optimization candidates, not measured fractions of renderer time: benchmark dependency chains and memory access differ from production rendering.
- EMPTY, ADD/MUL and DIV DEP vary by just one tick. DIV INDEP and subsequent conversion/table cases have much larger spreads. IRQs were enabled; interrupts and memory effects can contribute. Their individual causes cannot be assigned from the photos.
- The four-voice synthesis peak still dominates the photographed application time. These photos identify useful comparisons but do not yet establish why the renderer costs 1841 us, a safe higher voice limit, or an expected speedup from a particular rewrite.

The basic FPU arithmetic comparison is complete for this idle set. The next useful performance investigation is time attribution inside the complete renderer (operator work, envelopes/modulation and surrounding bookkeeping), with probe overhead controlled. The compound-kernel numbers cannot be summed into a trustworthy renderer profile. Repeated idle/loaded runs remain useful for interference and consistency, but a blanket fixed-point rewrite or LUT-to-RAM move is not justified by this evidence. No firmware, clock or budgeting change was made from these results.
