#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -Isrc tests/effects_test.cpp src/rdx_engine.cpp src/rdx_effects.cpp -o build/effects_test
ASAN_OPTIONS=detect_leaks=0 build/effects_test
