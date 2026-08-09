# vx: sync the pure-Python stdlib into <out>/python312/ so embedded python needs no MSYS2 install
import pathlib
import shutil
import sys

out_dir = pathlib.Path(sys.argv[1])
stdlib = pathlib.Path(sys.argv[2])
if not stdlib.is_dir():
    sys.exit("stdlib not found: " + str(stdlib))

dst = out_dir / ("python" + stdlib.name[len("python"):].replace(".", ""))  # python3.12 -> python312
# vx: keep lib-dynload: csv/typing/os etc. need C extensions (_csv, _sre, ...)
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
print(f"python312 sync: {copied} copied, {skipped} up-to-date -> {dst}")
