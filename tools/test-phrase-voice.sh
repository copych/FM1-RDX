#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
g++ -std=c++17 -O1 -g -ffp-contract=off -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -Isrc tests/phrase_voice.cpp src/rdx_engine.cpp -o build/phrase-voice-test
ASAN_OPTIONS=detect_leaks=0 build/phrase-voice-test
