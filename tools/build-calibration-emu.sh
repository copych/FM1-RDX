#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cp app/fm1.toml build/calibration-fm1.toml
trap 'cp build/calibration-fm1.toml app/fm1.toml' EXIT
python3 - <<'PY'
from pathlib import Path
p=Path('app/fm1.toml')
s=p.read_text().replace('"-DRDX_FM1"','"-DRDX_FM1", "-DRDX_CALIBRATION_CAPTURE"')
p.write_text(s)
PY
bash tools/build-emu.sh
mkdir -p build/calibration-emu
cp -r app/build/emu/. build/calibration-emu/
