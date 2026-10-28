#!/usr/bin/env python3
"""Run STARLIFT.gba in a nightly mGBA with tools/smoke.lua and print its log.

    python gba/tools/runmgba.py <mGBA.exe> <outdir> [seconds]
"""
import subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from elfsym import symbols
import os
OUT = Path(os.environ.get("GBA_OUT", HERE.parent / "out"))   # the build's output folder (build.py honours GBA_OUT too)

mgba, outdir = sys.argv[1], Path(sys.argv[2])
secs = float(sys.argv[3]) if len(sys.argv) > 3 else 40
outdir.mkdir(parents=True, exist_ok=True)
sy = symbols(OUT / "STARLIFT.elf")
lua = (HERE / "smoke.lua").read_text()
lua = (lua.replace("OUTDIR", '"%s/"' % outdir.as_posix()).replace("ADDR_N", str(sy["gba_dbgN"][0]))
          .replace("ADDR_D", str(sy["gba_drawUs"][0])).replace("ADDR_A", str(sy["gba_audUs"][0])).replace("ADDR_C", str(sy["gba_convUs"][0])).replace("ADDR_P", str(sy["gba_dbgPeriod"][0])).replace("ADDR_I", str(sy["gba_dbgIdle"][0])))
script = outdir / "run.lua"
script.write_text(lua)
log = outdir / "log.txt"
if log.exists():
    log.unlink()
p = subprocess.Popen([mgba, "-C", "mute=1", "--script", str(script), str(OUT / "STARLIFT.gba")],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < secs:
    time.sleep(1)
    if log.exists() and "done" in log.read_text():
        break
p.kill()
print(log.read_text() if log.exists() else "no log")
