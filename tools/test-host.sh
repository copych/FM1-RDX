#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
"${CXX:-g++}" -std=c++17 -O2 -g -Wall -Wextra -Wdouble-promotion \
  -ffp-contract=off -fsanitize=undefined,bounds,float-cast-overflow \
  -fno-sanitize-recover=all -Isrc src/rdx_engine.cpp tests/core_test.cpp -o build/core_test
build/core_test
