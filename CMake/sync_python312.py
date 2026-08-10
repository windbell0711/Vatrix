# vx: sync the pure-Python stdlib + C extensions into <out>/python312/ so embedded python needs no system install
# Layouts supported:
#   - NuGet package (scripts/fetch_python.ps1): <root>/Lib (pure stdlib) + <root>/DLLs (C extensions)
#   - MSYS2 install:                          <root>/lib/python3.12 (incl. lib-dynload)
import pathlib
import shutil
import sys

out_dir = pathlib.Path(sys.argv[1])
src_root = pathlib.Path(sys.argv[2])
if not src_root.is_dir():
    sys.exit("python root not found: " + str(src_root))

if (src_root / "Lib" / "os.py").is_file():
    stdlib = src_root / "Lib"
    dynload = src_root / "DLLs"
else:
    stdlib = src_root
    dynload = src_root / "lib-dynload"

dst = out_dir / "python312"
# vx: keep C extensions (DLLs/lib-dynload): csv/typing/os etc. need _csv/_sre/...
excluded = {"__pycache__", "test", "idlelib", "turtledemo"}
copied = skipped = 0
for p in sorted(stdlib.rglob("*")):
    rel = p.relative_to(stdlib)
    if any(part in excluded for part in rel.parts):
        continue
    target = dst / rel
    if p.is_dir():
        target.mkdir(parents=True, exist_ok=True)
        continue
    if target.exists() and target.stat().st_size == p.stat().st_size and target.stat().st_mtime >= p.stat().st_mtime:
        skipped += 1
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(p, target)
    copied += 1

if dynload.is_dir():
    for p in sorted(dynload.rglob("*")):
        rel = p.relative_to(dynload)
        target = dst / "lib-dynload" / rel
        if p.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            continue
        if target.exists() and target.stat().st_size == p.stat().st_size and target.stat().st_mtime >= p.stat().st_mtime:
            skipped += 1
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, target)
        copied += 1

print(f"python312 sync: {copied} copied, {skipped} up-to-date -> {dst}")