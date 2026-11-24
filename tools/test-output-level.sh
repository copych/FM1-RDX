#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all tests/output_level.cpp -o build/output-level-test
ASAN_OPTIONS=detect_leaks=0 build/output-level-test
