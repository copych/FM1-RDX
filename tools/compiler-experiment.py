"""Build isolated compiler candidates; restore production configuration on every exit.

Uses the existing WSL/Linux toolchain via build-fm1.sh; no flashing or publishing.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess

root = Path(__file__).resolve().parent.parent
os.chdir(root)
out = root / "build/compiler-experiment"
out.mkdir(parents=True, exist_ok=True)
config = root / "app/fm1.toml"
original = config.read_bytes()
variants = [("o2", "-O2", []), ("o3", "-O3", ["-DRDX_TEST_O3"]),
            ("o2-u4", "-O2", ["-DRDX_TEST_UNROLL"]),
            ("o3-u4", "-O3", ["-DRDX_TEST_O3", "-DRDX_TEST_UNROLL"])]
results = []

def run(argv, log):
    with log.open("w") as f:
        return subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT).returncode

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

try:
    for name, opt, defs in variants:
        dest = out / name
        dest.mkdir(exist_ok=True)
        flags = defs + ([opt] if opt == "-O3" else [])
        text = original.decode()
        text = re.sub(r'^(o2_cflags\s*=\s*\[)(.*?)(\])',
                      lambda m: m[1] + m[2] + "".join(", " + json.dumps(v) for v in flags) + m[3],
                      text, count=1, flags=re.M)
        config.write_text(text)
        (dest / "fm1.toml").write_text(text)
        record = {"name": name, "optimization": opt, "defines": defs,
                  "scope": "engine.cpp and editor.cpp (platform unity flags unchanged)"}
        results.append(record)
        print(f"{name}: target build", flush=True)
        rc = run(["bash", "tools/build-fm1.sh"], dest / "target.log")
        record["target"] = rc == 0
        if rc:
            print(f"{name}: target rejected; see target.log", flush=True)
            continue
        for file in ["rdx.fwsc", "rdx.bin", "rdx.elf", "rdx.dis", "o2/engine.o"]:
            shutil.copy2(root / "app/build" / file, dest / Path(file).name)
        record["firmwareSha256"] = sha(dest / "rdx.fwsc")
        record["imageBytes"] = (dest / "rdx.bin").stat().st_size
        run(["/root/.jieli/toolchain/pi32v2/bin/nm", "-S", "--size-sort", "app/build/o2/engine.o"], dest / "symbols.txt")
        run(["/root/.jieli/toolchain/pi32v2/bin/nm", "-u", "app/build/o2/engine.o"], dest / "helpers.txt")
        print(f"{name}: host controls and audio equivalence", flush=True)
        record["host"] = run(["bash", "tools/test-app.sh"], dest / "host.log") == 0
        native = ["g++", "-std=c++17", opt, "-ffp-contract=off", *defs, "-Isrc"]
        rc = run([*native, "tests/sound_render.cpp", "src/rdx_engine.cpp", "src/rdx_effects.cpp",
                  "-o", str(dest / "render")], dest / "audio-build.log")
        if rc == 0:
            rc = run([str(dest / "render"), "dry", str(dest / "audio.f32")], dest / "audio.log")
        record["audio"] = rc == 0
        if record["audio"]:
            record["audioSha256"] = sha(dest / "audio.f32")
            record["audioMatchesO2"] = record["audioSha256"] == results[0].get("audioSha256")
        rc = run([*native, "-fsanitize=undefined,float-cast-overflow", "-fno-sanitize-recover=all",
                  "tests/profile_equivalence.cpp", "src/rdx_engine.cpp", "src/rdx_profile.cpp",
                  "-o", str(dest / "equivalence")], dest / "equivalence-build.log")
        if rc == 0:
            rc = run([str(dest / "equivalence")], dest / "equivalence.log")
        record["equivalence"] = rc == 0
        print(f"{name}: matching browser build and worklet tests", flush=True)
        rc = run(["bash", "tools/build-emu.sh"], dest / "web-build.log")
        if rc == 0:
            record["wasmSha256"] = sha(root / "app/build/emu/app.wasm")
            shutil.copytree(root / "app/build/emu", dest / "emu", dirs_exist_ok=True)
            # WSL can find an unusable native node binary on PATH; use the
            # project's existing explicit Linux loader wrapper.
            node = ["sh", "tools/node-wsl.sh"]
            rc = run([*node, "tests/emu-test.mjs", str(dest / "emu")], dest / "web-test.log")
        record["browser"] = rc == 0
        print(f"{name}: {json.dumps(record)}", flush=True)
finally:
    config.write_bytes(original)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Production fm1.toml restored; candidate artifacts are separate.", flush=True)
