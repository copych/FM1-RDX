#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  tests/control_bridge.c -o build/control-bridge-test
ASAN_OPTIONS=detect_leaks=0 build/control-bridge-test
echo 'PASS: control ownership, MIDI ordering/filtering, staging/queue overflow, SysEx publication and guarded fallback'
