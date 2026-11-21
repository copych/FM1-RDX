#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all \
 -Isrc tests/lfo_continuity.cpp -o build/lfo_continuity
build/lfo_continuity
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=undefined,float-cast-overflow \
 -fno-sanitize-recover=all -Isrc tests/op1_continuity.cpp src/rdx_engine.cpp -o build/op1_continuity
build/op1_continuity
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -DPLAT_HOST -Iapp -Isrc tests/wobble_hold.cpp app/engine.cpp -o build/wobble_hold
ASAN_OPTIONS=detect_leaks=0 build/wobble_hold
