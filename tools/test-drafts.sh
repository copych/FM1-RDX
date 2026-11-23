#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all tests/drafts.cpp -o build/draft-test
ASAN_OPTIONS=detect_leaks=0 build/draft-test
