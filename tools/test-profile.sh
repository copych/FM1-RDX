#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -ffp-contract=off -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all \
  -Isrc tests/profile_equivalence.cpp src/rdx_engine.cpp src/rdx_profile.cpp -o build/profile_equivalence
build/profile_equivalence
