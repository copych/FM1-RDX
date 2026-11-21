#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O2 -g -ffp-contract=off -fsanitize=undefined,float-cast-overflow \
 -fno-sanitize-recover=all -Isrc tests/routing_equivalence.cpp src/rdx_engine.cpp -o build/routing_equivalence
build/routing_equivalence
