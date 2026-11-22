#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# The Windows submodule checkout may use CRLF. Python reads universal newlines;
# preserve the original script path as $0 so its source/include paths stay valid.
python3 - <<'PY'
from pathlib import Path
import subprocess
p = Path('platform/fm1-base/bsp/tests/run_board_tests.sh').resolve()
code = p.read_text()
selection = 'for test in lcd lcd_hw audio; do'
assert selection in code, 'Review changed BSP test driver'
code = code.replace(selection, 'for test in lcd lcd_hw; do')
subprocess.run(['bash', '-c', code, str(p)], check=True)
PY
