# Building FM1-RDX

Current release: **v1.0.2**. End-user instructions are in [README](../README.md); detailed evidence is in [release-1.0.2.md](RELEASE-1.0.0-beta.md).

Clone recursively. Modified platform/BSP sources are published as codex/fm1-base-rdx and codex/felucca-bsp-rdx branches of this repository; gitlinks and source-lock.json pin exact revisions. Upstream attribution, component notices and SDK requirements are retained.

```sh
git clone --recurse-submodules https://github.com/copych/FM1-RDX.git
cd FM1-RDX
bash tools/build-fm1.sh
bash tools/build-emu.sh
bash tools/test-app.sh
node tests/emu-test.mjs
```

Builds require the matched JieLi AC79 SDK/toolchain, Python/Pillow and Emscripten. The Windows workspace uses WSL Ubuntu. Set JIELI_TOOLCHAIN and AC79_SDK for your installation; defaults refer to the existing local toolchain and sibling SDK checkout. Output is app/build/rdx.fwsc. Scripts do not flash hardware. SDK/manifest, RAM, soft-double and other target guards remain enabled. Import tools are source-maintenance operations, not routine build prerequisites.

The release uses the established checked package path with APP_VERSION set to 1.0.2. The upstream strict distribution-metadata/license release gate remains unchanged. Component licences and SDK terms apply; no blanket licence is asserted for the original RDX source. Product identity FM-1_9900 is retained for update compatibility.

CPU1 generates dry voices by default; CPU0 mixes, processes FX and submits output. The normal CPU1 ceiling is seven, target 90% of the 44.1 kHz/128-frame period, with 0.5 percentage-point recovery hysteresis. Unavailable CPU1 retains the CPU0 fallback. Host/browser builds test behavior, not FM-1 peripheral/instruction timing.

Start-Emulator.ps1 builds/serves the browser on port 8788. The local installer uses 8790. tools/write-evidence.mjs records source/artifact hashes. Pin matching values in site/preview.json and run tools/build-site.mjs, optionally passing a clean output directory. tests/site-test.mjs validates that directory. The explicit GitHub release helper uses existing Git credentials without printing or storing them.
