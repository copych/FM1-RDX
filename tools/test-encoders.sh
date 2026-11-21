#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -fsanitize=undefined -fno-sanitize-recover=all tests/encoder_motion_test.cpp -o build/encoder_motion_test
build/encoder_motion_test
