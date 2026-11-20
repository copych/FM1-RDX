#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-/mnt/c/Users/admin/Documents/ChatGPT/FM1-X0X-mod/.toolchains/AC79_SDK}"
python3 platform/fm1-base/tools/build.py app "$@"
