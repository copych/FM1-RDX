#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/prepare-reference.py
g++ -std=c++17 -O2 -ffp-contract=off -w -DRDX_REFERENCE=1 -Ibuild/reference -Isrc tests/envelope_trace.cpp -o build/envelope-reference
g++ -std=c++17 -O2 -ffp-contract=off -w -Isrc tests/envelope_trace.cpp -o build/envelope-port
build/envelope-reference build/envelope-reference.csv
build/envelope-port build/envelope-port.csv
python3 tools/compare-envelopes.py "$@"
