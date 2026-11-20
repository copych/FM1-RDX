#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! node --version >/dev/null 2>&1; then
    export EM_NODE_JS="$PWD/tools/node-wsl.sh"
fi
emcc --version | head -1
python3 tools/prepare-emu.py
bash tools/build-fm1.sh --web
