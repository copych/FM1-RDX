#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/build-fm1.sh --host
mkdir -p build/app-smoke
app/build/host/rdx_host tests/app-smoke.txt build/app-smoke
mkdir -p build/editor-controls
app/build/host/rdx_host tests/editor-controls.txt build/editor-controls
