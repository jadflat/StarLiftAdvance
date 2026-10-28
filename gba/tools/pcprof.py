#!/usr/bin/env python3
"""Sampling profile of STARLIFT.gba: the ROM records the interrupted PC every millisecond (main.cpp), mGBA plays a scripted
run, and this resolves the samples to functions.

    python gba/tools/pcprof.py <mGBA.exe> <outdir> [from_frame] [to_frame]
"""
import bisect, subprocess, sys, time
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from elfsym import symbols
import os
OUT = Path(os.environ.get("GBA_OUT", HERE.parent / "out"))   # the build's output folder (build.py honours GBA_OUT too)

mgba, outdir = sys.argv[1], Path(sys.argv[2])
f0 = int(sys.argv[3]) if len(sys.argv) > 3 else 1500
f1 = int(sys.argv[4]) if len(sys.argv) > 4 else 2100
outdir.mkdir(parents=True, exist_ok=True)
sy = symbols(OUT / "STARLIFT.elf")
lua = '''local f = io.open("%s/pcs.txt", "w")
local N, R = %d, %d
local n0 = 0
callbacks:add("frame", function()
  local fr = emu:currentFrame()
  if fr > 200 and fr < 1500 then
    if fr %% 80 < 6 then emu:setKeys(1) else emu:setKeys(0) end
  else emu:setKeys(0) end
  if fr == %d then n0 = emu:read32(N) end
  if fr == %d then
    local n1 = emu:read32(N)
    for i = n0, n1 - 1 do f:write(string.format("%%x", emu:read32(R + (i %% 4096) * 4)) .. string.char(10)) end
    f:write("end" .. string.char(10)); f:close()
  end
end)
''' % (outdir.as_posix(), sy["gba_pcN"][0], sy["gba_pcRing"][0], f0, f1)
(outdir / "pcprof.lua").write_text(lua, newline="\n")
out = outdir / "pcs.txt"
if out.exists():
    out.unlink()
p = subprocess.Popen([mgba, "-C", "mute=1", "--script", str(outdir / "pcprof.lua"), str(OUT / "STARLIFT.gba")],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < 200:
    time.sleep(2)
    if out.exists() and out.read_text().rstrip().endswith("end"):
        break
p.kill()
items = sorted((v[0], k) for k, v in sy.items() if v[0] >= 0x03000000 and not k.startswith("$"))
addrs = [a for a, _ in items]
pcs = [int(x, 16) for x in out.read_text().split() if x != "end"]
byfn, byreg = Counter(), Counter()
for pc in pcs:
    r = "ROM" if pc >= 0x08000000 else "IWRAM" if pc >= 0x03000000 else "EWRAM" if pc >= 0x02000000 else "BIOS"
    byreg[r] += 1
    i = bisect.bisect_right(addrs, pc) - 1
    byfn[items[i][1] if i >= 0 and r != "BIOS" else r] += 1
n = len(pcs)
print("samples", n, "(1 kHz; the audio mixer in the VBlank interrupt is not sampled)")
print("regions:", {k: "%.1f%%" % (100 * v / n) for k, v in byreg.most_common()})
for k, v in byfn.most_common(30):
    print("%5.1f%%  %s" % (100 * v / n, k))
