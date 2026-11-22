#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
for test in voice_measure voice_measure_engine; do
  g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
    -fno-sanitize-recover=all -Isrc "tests/$test.cpp" -o "build/$test"
  ASAN_OPTIONS=detect_leaks=0 "build/$test"
done
echo 'PASS: voice sweep timing, wrap, guard, completion, mismatch, restoration and normal overload recovery'
