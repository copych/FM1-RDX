#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow \
  -fno-sanitize-recover=all -Isrc tests/dry_pipeline.cpp -o build/dry-pipeline
ASAN_OPTIONS=detect_leaks=0 build/dry-pipeline
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Isrc tests/pipeline_budget.cpp -o build/pipeline-budget
ASAN_OPTIONS=detect_leaks=0 build/pipeline-budget
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  tests/ui_profile.c -o build/ui-profile
ASAN_OPTIONS=detect_leaks=0 build/ui-profile
bash tools/test-lcd-wait.sh
g++ -std=c++17 -O2 -ffp-contract=off -fsanitize=undefined,float-cast-overflow \
  -fno-sanitize-recover=all -Isrc tests/dry_equivalence.cpp src/rdx_engine.cpp -o build/dry-equivalence
build/dry-equivalence
