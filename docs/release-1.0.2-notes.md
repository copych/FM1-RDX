# FM1-RDX 1.0.2

A clearer Reface-style display and cleaner effects, with more ways to keep musical ideas.

- Reversible whole-loop quantization: straight and triplet grids from 1/4 to 1/32, plus OFF for the original timing.
- Musical drafts now remember quantization alongside the phrase, voice and tempo. Existing drafts load with quantization OFF.
- Tighter Looper, Store and Draft layouts, contextual button hints, and readable OP 1–4 headings on operator pages.
- Fixed the name-edit cursor leaving underlines behind and the looper header redrawing unnecessarily.
- DC removal before effects and output protection, with newly measured effect gain calibration. Factory presets retain their original settings; no per-preset normalization.
- Output peak protection, while retaining up to seven adaptive voices, USB MIDI, Soundmondo, stereo USB recording, user voice slots and the matching browser emulator.

[Play in your browser](https://copych.github.io/FM1-RDX/emu/) · [Install on FM-1](https://copych.github.io/FM1-RDX/install/)

Stop playback before saving or loading. Flash writes briefly mute sound. Close the installer tab before opening your MIDI or recording app. Drafts saved by 1.0.2 require 1.0.2 or later; existing older drafts remain loadable.

Download the .fwsc file for the firmware package, or use the web installer. The web ZIP contains the matching browser synth and installer for local hosting. SHA256SUMS.txt and build-evidence.json identify the files. The installer retains the stock-firmware recovery option and does not replace the bootloader.

The firmware is tested regularly on the author's FM-1. Build, host and emulator results for this release, along with the hardware checks still to be measured for the latest changes, are recorded in docs/release-1.0.2.md.
