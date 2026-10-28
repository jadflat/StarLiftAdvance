#!/usr/bin/env python3
"""Play STARLIFT.gba in a nightly mGBA (smoke.lua's button presses) and save every sound buffer the ROM mixes, as the
FIFOs would play them: out/<dir>/sound.wav (32017 Hz, 16-bit mono, the 10-bit output level), then print its levels,
the frame seams and the mixing time.

    python gba/tools/sounddump.py <mGBA.exe> <outdir> [frames]     (GBA_OUT picks the build, as for runmgba.py)

This checks what the mixer writes (the level, the knee, the interpolation across frames). What the FIFOs and DMA then
do with it is up to the hardware (or mGBA): listen to the game for that.
"""
import os, subprocess, sys, time, wave
from pathlib import Path
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from elfsym import symbols
OUT = Path(os.environ.get("GBA_OUT", HERE.parent / "out"))
mgba, outdir = sys.argv[1], Path(sys.argv[2])
frames = int(sys.argv[3]) if len(sys.argv) > 3 else 3600
outdir.mkdir(parents=True, exist_ok=True)
sy = symbols(OUT / "STARLIFT.elf")
Q, PAD = 536, 32
lua = r'''
local out = "%s"
local A, B, CUR, AU = %d, %d, %d, %d
local f = io.open(out .. "bufs.bin", "wb")
local last, n0, a0 = -1, 0, 0
callbacks:add("frame", function()
  local n = emu:currentFrame()
  if n > 200 and n < 1500 then
    if n %% 80 < 6 then emu:setKeys(1) else emu:setKeys(0) end
  elseif n >= 1500 then
    local k = math.floor(n / 45) %% 8
    local keys = {32 + 1, 16 + 1, 1, 2, 64, 128, 32 + 2, 16 + 1}
    emu:setKeys(keys[k + 1])
  end
  local au = emu:read32(AU)
  if au ~= last and n > 60 then
    local c = emu:read8(CUR)
    f:write(emu:readRange(A + c * %d, %d)); f:write(emu:readRange(B + c * %d, %d))
    if n0 == 0 then n0, a0 = n, au end
  end
  last = au
  if n == %d then
    local g = io.open(out .. "mix.txt", "w"); g:write(string.format("%%d %%d\n", n - n0, au - a0)); g:close()
    f:close(); g = io.open(out .. "done.txt", "w"); g:write("done"); g:close()
  end
end)
''' % (outdir.as_posix() + "/", sy["gba_audA"][0], sy["gba_audB"][0], sy["gba_audCur"][0], sy["gba_audUs"][0],
       Q + PAD, Q, Q + PAD, Q, frames)
script = outdir / "dump.lua"
script.write_text(lua, newline="\n")
for f in ("done.txt", "bufs.bin"):
    if (outdir / f).exists():
        (outdir / f).unlink()
p = subprocess.Popen([mgba, "-C", "mute=1", "--script", str(script), str(OUT / "STARLIFT.gba")], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < frames / 60 * 3 + 60 and not (outdir / "done.txt").exists():
    time.sleep(1)
p.kill()
raw = np.frombuffer((outdir / "bufs.bin").read_bytes(), dtype=np.int8).reshape(-1, 2, Q)
y = raw[:, 0, :].astype(int) * 2 + (raw[:, 1, :].astype(np.uint8) & 1)    # half steps of FIFO A
s = y.reshape(-1)
w = wave.open(str(outdir / "sound.wav"), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(32017)
w.writeframes((s * 2 * 64).astype("<i2").tobytes()); w.close()
nf, us = map(int, (outdir / "mix.txt").read_text().split())
d = np.abs(np.diff(y, axis=1)).mean()
seam = np.abs(y[1:, 0] - y[:-1, -1]).mean()
lvl = lambda v: 20 * np.log10(max(v, 1e-9) / 256)
print(f"{len(y)} buffers ({len(s) / 32017:.1f} s), peak {lvl(np.abs(s).max()):.1f} dB, rms {lvl(np.sqrt((s.astype(float) ** 2).mean())):.1f} dB "
      f"(of the 10-bit range), at the limit {100 * (np.abs(s) >= 254).mean():.3f}%")
print(f"step between samples: inside a frame {d:.2f}, across a frame seam {seam:.2f} half steps")
print(f"mixing: {us / max(nf, 1) / 1000:.3f} ms a frame ({100 * us / max(nf, 1) / 16743:.1f}% of the CPU) over {nf} frames")
