#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -DPLAT_HOST -Iapp -Isrc tests/wobble_hold.cpp app/engine.cpp -o build/wobble_hold
ASAN_OPTIONS=detect_leaks=0 build/wobble_hold
