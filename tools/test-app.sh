#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build-fm1.sh --host
mkdir -p build/app-smoke
app/build/host/rdx_host tests/app-smoke.txt build/app-smoke
mkdir -p build/editor-controls
app/build/host/rdx_host tests/editor-controls.txt build/editor-controls
mkdir -p build/bank-controls
app/build/host/rdx_host tests/bank-controls.txt build/bank-controls
mkdir -p build/manual-controls
app/build/host/rdx_host tests/manual-controls.txt build/manual-controls
mkdir -p build/level-shortcut
app/build/host/rdx_host tests/level-shortcut.txt build/level-shortcut
mkdir -p build/encoder-controls
app/build/host/rdx_host tests/encoder-controls.txt build/encoder-controls
mkdir -p build/timing-controls
app/build/host/rdx_host tests/timing-controls.txt build/timing-controls
mkdir -p build/benchmark-controls
app/build/host/rdx_host tests/benchmark-controls.txt build/benchmark-controls
mkdir -p build/profile-controls
app/build/host/rdx_host tests/profile-controls.txt build/profile-controls
mkdir -p build/core-test-controls
app/build/host/rdx_host tests/core-test-controls.txt build/core-test-controls
mkdir -p build/gui-split-controls
app/build/host/rdx_host tests/gui-split-controls.txt build/gui-split-controls
mkdir -p build/control-isolation-controls
app/build/host/rdx_host tests/control-isolation-controls.txt build/control-isolation-controls
mkdir -p build/voice-cost-controls
app/build/host/rdx_host tests/voice-cost-controls.txt build/voice-cost-controls
mkdir -p build/preset-home
app/build/host/rdx_host tests/preset-home.txt build/preset-home
mkdir -p build/select-context
app/build/host/rdx_host tests/select-context.txt build/select-context
if node --version >/dev/null 2>&1; then
    node tools/prepare-soundmondo-test.mjs
else
    sh tools/node-wsl.sh tools/prepare-soundmondo-test.mjs
fi
app/build/host/rdx_host build/soundmondo-controls.txt build/soundmondo-controls
mkdir -p build/polyphony-controls
app/build/host/rdx_host tests/polyphony-controls.txt build/polyphony-controls
bash tools/test-isolation.sh
bash tools/test-voice-cost.sh
