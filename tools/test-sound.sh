#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
python3 tools/prepare-reference.py
g++ -std=c++17 -O1 -ffp-contract=off -w -DRDX_REFERENCE=1 -Ibuild/reference -Isrc tests/sound_render.cpp -o build/sound-reference
g++ -std=c++17 -O1 -ffp-contract=off -w -Isrc tests/sound_render.cpp src/rdx_engine.cpp src/rdx_effects.cpp -o build/sound-port
for mode in dry wet; do
    build/sound-reference "$mode" "build/$mode-reference.f32"
    build/sound-port "$mode" "build/$mode-port.f32"
done
python3 tools/compare-sound.py
