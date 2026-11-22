# Felucca / Melodee FM6 source review — 2026-11-22

Inspected actual sources pinned to Felucca
ca1db43a78455ae747766a2738f8580380d40bc1 and Melodee
165d177a425944f3d30b0b669255a6ed0050c3a5. Downloaded read-only reference files
and source manifests are in ignored build/source-review/{felucca,melodee}.
No external implementation code was copied into the RDX firmware.

## Verified implementation differences

| Area | Felucca | Current Melodee | RDX Preview 22/23 |
| --- | --- | --- | --- |
| FM signal math | Q15 sine and Q24 operator output, integer phase. | MODERN uses native FP32 signal arithmetic; phase remains a wrapping integer counter, with DX7 MARK I/OPL alternatives. | FP32 operators/envelopes, float phase and interpolation. |
| Control work | FM6 control tick 32 samples, gain ramp inside operator loop. | Dexed-compatible FM6 control block 64 samples, run in 32-sample control chunks. | AEG/PEG/modulation/pitch evaluated every sample. |
| Rendering layout | Operator over a short block, separate input/no-input and add/write loops. | Prepared operator plan; specialized no-input/input, feedback, engine and flat-gain kernels. | Sample-major routed voice loop; cached route selection still examined each sample. |
| Multicore | Parallel part renders into per-core sums; cost-based part assignment and join. | Costed audio job batches, including per-voice control/inactive FM6 work; both cores process DSP, with private PCM. | CPU0 DSP; CPU1 application controls/UI/LCD. |

Primary sources: [Felucca FM6 core](https://github.com/hugelton/Felucca/blob/ca1db43a78455ae747766a2738f8580380d40bc1/firmware/src/fm6_core.c),
[Felucca mixer/core split](https://github.com/hugelton/Felucca/blob/ca1db43a78455ae747766a2738f8580380d40bc1/firmware/src/fx.c),
[Melodee FM6 core](https://github.com/keremimo/melodee/blob/165d177a425944f3d30b0b669255a6ed0050c3a5/firmware/src/fm6_core.c),
[Melodee audio batching](https://github.com/keremimo/melodee/blob/165d177a425944f3d30b0b669255a6ed0050c3a5/firmware/src/audio_batch.h).

The current Melodee source corrects an overly broad earlier assumption that both
FM6 implementations are integer signal engines. Its modern engine demonstrates
that native FP32 is a viable approach on this chip. It is still a mixed numeric
design: its integer phase, log-domain state and control cadence differ from RDX.

Felucca fm6_op_run selects one of four loop bodies before iterating samples.
Melodee fm6_run_into dispatches a prepared operator plan and selects separate
feedback/no-feedback and engine-specific kernels; MELODEE_FM6_FAST also chooses
a flat-gain modern kernel. This removes routing and mode decisions from the
inner operator loop and keeps phase/gain/frequency local during that loop.

Felucca's mix_split partitions parts by measured part costs. Melodee's
audio_batch.h queues bounded jobs with a cost table and CPU0 service allowance,
and returns voice-state ownership at worker join. These are meaningful uses of
the second DSP core, not simply moving menus there. Their join-based architecture
is different from our no-audio-wait boundary and cannot be transplanted unchanged
under our current real-time constraints.

Voice allocator capacities and README polyphony claims are not controlled
benchmarks of every patch/operator/effect/modulation combination. No local FM1
build/benchmark of either external project was run. We cannot infer an exact
RDX voice count or compare operator counts as equivalent workloads.

## Transferable next steps

1. Cache parameter-only coefficients. Preview 23 does this without changing
   signal values or cadence; see CACHED-MODULATION.md.
2. Prepare routing once per block/patch and use small operator-block kernels.
   Retain per-sample AEG/pitch/gain arrays rather than adopting a coarser control
   rate. Measure scratch memory traffic, register spills and exact audio output:
   our diagnostic block staging already demonstrated that scratch overhead can
   erase some savings.
3. Specialize unmodulated/feedback-zero paths while preserving hidden oscillator,
   envelope and feedback state, especially across live parameter changes.
4. Consider sharing actual DSP across cores only as a separately designed change
   with completion, failure recovery and latency contracts. UI isolation alone
   cannot supply the extra DSP throughput these mixers use.

Not adopted: their DX7 control cadence, logarithmic envelope equations, integer
phase quantization, silence thresholds/skipping, gain ramps or feedback topology.
Those choices are part of their intended synth behavior and are not numerical
drop-in replacements for the Reface-inspired engine. Any such change would need
an explicit sound-accuracy decision and device/listening tests.
