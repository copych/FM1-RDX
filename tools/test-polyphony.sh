#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -Isrc tests/polyphony.cpp src/rdx_engine.cpp -o build/polyphony_test
ASAN_OPTIONS=detect_leaks=0 build/polyphony_test
