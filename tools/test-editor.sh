#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
 -fno-sanitize-recover=all -DPLAT_HOST -Isrc -Iapp tests/editor_test.cpp app/engine.cpp app/editor.cpp -o build/editor_test
ASAN_OPTIONS=detect_leaks=0 build/editor_test
