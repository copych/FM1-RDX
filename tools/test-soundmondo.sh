#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -DPLAT_HOST -Iapp -Isrc tests/soundmondo.cpp app/engine.cpp -o build/soundmondo_test
ASAN_OPTIONS=detect_leaks=0 build/soundmondo_test
