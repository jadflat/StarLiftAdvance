#!/usr/bin/env python3
"""Log the order in which a few functions run (mGBA nightly breakpoints): python trace.py <mGBA.exe> <outdir> [seconds]"""
import subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from elfsym import symbols
import os
OUT = Path(os.environ.get("GBA_OUT", HERE.parent / "out"))   # the build's output folder (build.py honours GBA_OUT too)

mgba, outdir = sys.argv[1], Path(sys.argv[2])
secs = float(sys.argv[3]) if len(sys.argv) > 3 else 10
outdir.mkdir(parents=True, exist_ok=True)
sy = symbols(OUT / "STARLIFT.elf")
names = {"start": sy["start"][0], "gba_main": sy["gba_main"][0], "setup": sy["_Z5setupv"][0], "loop": sy["_Z4loopv"][0],
         "present": sy["_ZL7presentv"][0], "gfx_wait": sy["_Z8gfx_waitv"][0], "sim_advance": sy["_Z11sim_advancej"][0],
         "pollButtons": sy["_ZN6CHGame11pollButtonsEv"][0], "gba_vblank": sy["_Z10gba_vblankv"][0],
         "exit": sy["exit"][0]}
bp = "".join('emu:setBreakpoint(function() ev("%s") end, %d)\n' % (k, v) for k, v in names.items())
lua = ('local f = io.open("%s/seq.txt", "w")\nlocal n = 0\nlocal function ev(name)\n  n = n + 1\n'
       '  if n <= 80 then f:write(n .. " " .. name .. " f" .. emu:currentFrame() .. " lr=" .. string.format("%%08x", emu:readRegister("lr")) .. string.char(10)); f:flush() end\nend\n%s'
       % (outdir.as_posix(), bp))
(outdir / "seq.lua").write_text(lua, newline="\n")
out = outdir / "seq.txt"
if out.exists():
    out.unlink()
p = subprocess.Popen([mgba, "-C", "mute=1", "--script", str(outdir / "seq.lua"), str(OUT / "STARLIFT.gba")],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(secs)
p.kill()
print(out.read_text() if out.exists() else "no output")
