"""Compare target RAM instruction bytes, without inferring execution speed."""
from pathlib import Path
import hashlib
import json
import re
import struct

root = Path(__file__).resolve().parent.parent / "build/compiler-experiment"
reports = []
for folder in sorted(root.iterdir()):
    if not (folder / "engine.o").exists():
        continue
    data = (folder / "engine.o").read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected ELF32 little endian")
    start = struct.unpack_from("<I", data, 32)[0]
    stride, count, names_index = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, start+i*stride) for i in range(count)]
    names = sections[names_index]
    strings = data[names[4]:names[4]+names[5]]
    def name(s):
        return strings[s[0]:strings.index(0,s[0])].decode()
    ram_sections = [s for s in sections if name(s) == ".app_ram"]
    body = b"".join(data[s[4]:s[4]+s[5]] for s in ram_sections)
    symtab = next(s for s in sections if s[1] == 2)
    symnames = sections[symtab[6]]
    symstrings = data[symnames[4]:symnames[4]+symnames[5]]
    symbols = [struct.unpack_from("<IIIBBH",data,p) for p in range(symtab[4],symtab[4]+symtab[5],symtab[9])]
    def symname(s):
        return symstrings[s[0]:symstrings.index(0,s[0])].decode()
    symbol = next(s for s in symbols if symname(s) == "_ZN9RdxEngine11renderBlockEPfS0_")
    ram_index = symbol[5]
    section = sections[ram_index]
    at, length = symbol[1]-section[3], symbol[2]
    render = data[section[4]+at:section[4]+at+length]
    if len(render) != length:
        raise ValueError("Truncated renderer")
    report = {"name": folder.name,"appRamBytes":len(body),"renderBytes":length,
              "appRamInstructionSha256":hashlib.sha256(body).hexdigest(),
              "renderInstructionSha256":hashlib.sha256(render).hexdigest()}
    # Resolve relocation symbol names, not ELF-local symbol-table indices.
    relocations=[]
    for s in sections:
        if s[1] in (4,9) and s[7] == ram_index:
            for p in range(s[4],s[4]+s[5],s[9]):
                off,info=struct.unpack_from("<II",data,p)
                addend=struct.unpack_from("<i",data,p+8)[0] if s[1]==4 else None
                relocations.append((off,info&255,symname(symbols[info>>8]),addend))
    report["renderRelocationsSha256"]=hashlib.sha256(json.dumps(relocations).encode()).hexdigest()
    reports.append(report)
(root / "code-comparison.json").write_text(json.dumps(reports,indent=2)+"\n")
print(json.dumps(reports,indent=2))
