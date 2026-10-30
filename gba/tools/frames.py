#!/usr/bin/env python3
"""Play a fixed run by the game's own frame count (main.cpp's test hook) and time every frame; optionally dump frames.

    python gba/tools/frames.py <mGBA.exe> <outdir> [frames] [snap,snap,...] [--fresh]

Every build plays exactly the same frames, however fast it draws (the buttons are a pattern of chgame.frameCount), so
timings and pictures can be compared frame for frame between builds. Writes <outdir>/log.txt (each checkpoint's mean,
busy and worst; the whole run's worst, a histogram, every frame over 33.3 ms) and <outdir>/snap_<frame>.bin (the 4-bit
framebuffer, 8 KB) for each frame asked for. Uses GBA_OUT like the other tools. --fresh deletes GBA_OUT's
STARLIFT.sav first (mGBA keeps one beside each ROM: a high score or a game to continue changes what is played).
--prof also writes <outdir>/samples.txt: main.cpp's 1 kHz PC samples, each after the "F <frame> <busy us>" line of the
frame it ended in (an approximation: samples are collected once an emulator frame).
"""
import os, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from elfsym import symbols
OUT = Path(os.environ.get("GBA_OUT", HERE.parent / "out"))

fresh, prof = "--fresh" in sys.argv, "--prof" in sys.argv
args = [a for a in sys.argv[1:] if a not in ("--fresh", "--prof")]
mgba, outdir = args[0], Path(args[1])
frames = int(args[2]) if len(args) > 2 else 3000
snaps = [int(s) for s in args[3].split(",")] if len(args) > 3 and args[3] else []
if fresh and (OUT / "STARLIFT.sav").exists():
    (OUT / "STARLIFT.sav").unlink()
outdir.mkdir(parents=True, exist_ok=True)
sy = symbols(OUT / "STARLIFT.elf")
a = lambda n: sy[n][0] if n in sy else 0
NL = "string.char(10)"
lua = f'''local out = "{outdir.as_posix()}/"
local f = io.open(out .. "log.txt", "w")
local N, P, I = {a("gba_dbgN")}, {a("gba_dbgPeriod")}, {a("gba_dbgIdle")}
local SCRIPT, SNAPAT, SNAPDONE, SNAP = {a("test_script")}, {a("test_snapAt")}, {a("test_snapDone")}, {a("gba_snap")}
local snaps = {{{",".join(map(str, snaps))}}}
local si, seen, worst, worstAt, over, tot, hist, overList = 1, 1, 0, 0, 0, 0, {{}}, ""
local cs, cb, cm, cn, finished, started = 0, 0, 0, 0, false, false
local hooked = false
local CONV = {a("gba_convUs")}
local shotAt, shotName = nil, 0
local PSUM, PFR, BST = {a("gba_profSum")}, {a("gba_profFrames")}, {a("gba_blitStat")}
local PROF, PCN, PCR, pseen = {1 if prof else 0}, {a("gba_pcN")}, {a("gba_pcRing")}, 0
local pf = PROF == 1 and io.open(out .. "samples.txt", "w") or nil
local CHK = {{ {{"scenery", {a("gba_scChecks")}, {a("gba_scBad")}, {a("gba_scBadFirst")}}}, {{"blit", {a("gba_blChecks")}, {a("gba_blBad")}, {a("gba_blBadFirst")}}}, {{"ground layer", {a("gba_gcChecks")}, {a("gba_gcBad")}, {a("gba_gcBadFirst")}}}, {{"convert", {a("gba_cvChecks")}, {a("gba_cvBad")}, {a("gba_cvBadFirst")}}} }}   -- GBA_SCENECHECK builds
callbacks:add("frame", function()
  local fr = emu:currentFrame()
  if finished then return end
  if not started then                       -- the first callback: the test marker into SRAM, and start over (main.cpp)
    started = true
    local m = "CHGBATST"
    for i = 1, 8 do emu:write8(0x0E007FF7 + i, string.byte(m, i)) end
    emu:reset()
    return
  end
  if not hooked then                        -- once the game has shown its first frame (start-up done)
    if emu:read32(N) < 1 then return end
    hooked = true
    if snaps[si] then emu:write32(SNAPAT, snaps[si]); emu:write32(SNAPDONE, 0) else emu:write32(SNAPAT, 0xFFFFFFFF) end
    f:write("test hook " .. (emu:read32(SCRIPT) == 1 and "on" or "OFF: not a fixed run") .. " (reset at emulator frame " .. fr .. ", game frame now " .. emu:read32(N) .. ")" .. {NL})
  end
  if shotAt and fr >= shotAt then emu:screenshot(out .. "shot_" .. shotName .. ".png"); f:write("shot " .. shotName .. string.format(" DISPCNT %x map2[0..3] %x %x %x %x", emu:read16(0x04000000), emu:read16(0x06007000 + 2*32*10), emu:read16(0x06007002 + 2*32*10), emu:read16(0x06007004 + 2*32*10), emu:read16(0x06007006 + 2*32*10)) .. {NL}); shotAt = nil end
  if emu:read32(SNAPDONE) == 1 then
    shotAt, shotName = fr + 2, snaps[si]                -- the screen as shown (all layers), once that frame is up
    local g = io.open(out .. "snap_" .. snaps[si] .. ".bin", "wb")
    for i = 0, 8191 do g:write(string.char(emu:read8(SNAP + i))) end
    g:close()
    si = si + 1
    if snaps[si] then emu:write32(SNAPAT, snaps[si]) else emu:write32(SNAPAT, 0xFFFFFFFF) end
    emu:write32(SNAPDONE, 0)
  end
  local n = emu:read32(N)
  if pf then
    local pn = emu:read32(PCN)
    for i = pseen, pn - 1 do pf:write(string.format("%x", emu:read32(PCR + (i % 4096) * 4)) .. {NL}) end
    pseen = pn
  end
  for k = seen, n - 1 do
    if pf then pf:write("F " .. k .. " " .. (emu:read32(P + (k % 128) * 4) - emu:read32(I + (k % 128) * 4)) .. " " .. fr .. {NL}) end
    local p = emu:read32(P + (k % 128) * 4)
    local b = p - emu:read32(I + (k % 128) * 4)
    if k > 2 and k <= {frames} then
      tot = tot + 1
      if b > worst then worst = b; worstAt = k end
      if b > 33333 then over = over + 1; overList = overList .. " " .. k .. ":" .. math.floor(b / 100) / 10 end
      local h = math.floor(b / 2000); hist[h] = (hist[h] or 0) + 1
      cs = cs + p; cb = cb + b; cn = cn + 1; if b > cm then cm = b end
      if k % 250 == 0 then
        f:write("frames " .. (k - 249) .. "-" .. k .. ": period " .. math.floor(cs / cn / 10) / 100 .. " ms, busy " .. math.floor(cb / cn / 10) / 100 .. " ms, max " .. math.floor(cm / 10) / 100 .. " ms" .. {NL})
        cs, cb, cm, cn = 0, 0, 0, 0
      end
    end
  end
  if n > seen then seen = n end
  if n > {frames} then
    f:write("BCPOOL resets " .. emu:read32({a("gba_bcResets")}) .. " ents " .. emu:read32({a("gba_bcN")}) .. " used " .. emu:read32({a("gba_bcUsed")}) .. {NL})
    f:write("WHOLE RUN: frames " .. tot .. ", worst " .. (worst / 1000) .. " ms (frame " .. worstAt .. "), over 33.3 ms: " .. over .. {NL})
    local t = "busy histogram (2 ms bins):"
    for h = 0, 200 do if hist[h] then t = t .. " " .. (h * 2) .. "-" .. (h * 2 + 2) .. ":" .. hist[h] end end
    for _, c in ipairs(CHK) do
      if c[2] > 0 then f:write("CHECK " .. c[1] .. ": " .. emu:read32(c[2]) .. " draws compared, " .. emu:read32(c[3]) .. " differ" .. string.format(" (first: %x)", emu:read32(c[4])) .. {NL}) end
    end
    if PSUM > 0 and emu:read32(PFR) > 0 then                -- GBA_PROF builds: the game's dbg::prof sections, mean us a frame
      local pt = "PROF (us a frame, " .. emu:read32(PFR) .. " frames):"
      for i = 0, 11 do pt = pt .. " " .. i .. ":" .. (emu:read32(PSUM + 4 * i) // emu:read32(PFR)) end
      f:write(pt .. {NL})
      if BST > 0 then
        local bt = "BLITS (w x h: us a frame, calls a frame):"
        for i = 0, 63 do local k = emu:read32(BST + 12 * i); if k > 0 then bt = bt .. string.format(" %dx%d:%d/%.1f", k >> 8, k & 255, emu:read32(BST + 12 * i + 4) // emu:read32(PFR), emu:read32(BST + 12 * i + 8) / emu:read32(PFR)) end end
        f:write(bt .. {NL})
      end
    end
    if CONV > 0 then f:write("CONV (present's convert + sky, us a frame): " .. (emu:read32(CONV) // emu:read32(N)) .. {NL}) end
    f:write(t .. {NL} .. "over 33.3 ms (frame:ms):" .. overList .. {NL} .. "done" .. {NL})
    f:close()
    if pf then pf:write("end" .. {NL}); pf:close() end
    finished = true
  end
end)
'''
(outdir / "frames.lua").write_text(lua, newline="\n")
log = outdir / "log.txt"
if log.exists():
    log.unlink()
p = subprocess.Popen([mgba, "-C", "mute=1"] + (["-C", "audioSync=0", "-C", "videoSync=0", "-C", "fpsTarget=6000"] if os.environ.get("GBA_UNTHROTTLED") else []) + ["--script", str(outdir / "frames.lua"), str(OUT / "STARLIFT.gba")],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
while time.time() - t0 < float(os.environ.get("GBA_TIMEOUT", "600")):
    time.sleep(1)
    if log.exists() and "done" in log.read_text():
        break
p.kill()
print(log.read_text() if log.exists() else "no log")
