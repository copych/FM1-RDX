#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
  -Isrc tests/user_store.cpp -o build/user-store
ASAN_OPTIONS=detect_leaks=0 build/user-store
