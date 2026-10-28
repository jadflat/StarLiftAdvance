-- mGBA (nightly, --script): screenshots and frame timing of STARLIFT.gba. Addresses come from the ELF (elfsym.py).
local out = OUTDIR
local f = io.open(out .. "log.txt", "w")
local N, P, I = ADDR_N, ADDR_P, ADDR_I
local AU, CU, DU = ADDR_A, ADDR_C, ADDR_D
local last = {n = 0, a = 0, c = 0, d = 0}
local function stats(tag)
  local n = emu:read32(N)
  local sp, sb, mx, c = 0, 0, 0, 0
  for k = math.max(1, n - 30), n - 1 do
    local p = emu:read32(P + (k % 128) * 4)
    local i = emu:read32(I + (k % 128) * 4)
    sp = sp + p; sb = sb + (p - i); if p - i > mx then mx = p - i end; c = c + 1
  end
  if c > 0 then
    f:write(tag .. " flushes " .. n .. " period " .. (sp / c / 1000) .. " ms busy " .. (sb / c / 1000) .. " ms max " .. (mx / 1000) .. "\n")
  else
    f:write(tag .. " no flushes " .. n .. "\n")
  end
  local a, c, d = emu:read32(AU), emu:read32(CU), emu:read32(DU)
  local dn = n - last.n
  if dn > 0 then f:write(tag .. " per flush: audio " .. ((a - last.a) / dn / 1000) .. " ms, convert " .. ((c - last.c) / dn / 1000) .. " ms, draw " .. ((d - last.d) / dn / 1000) .. " ms" .. string.char(10)) end
  last = {n = n, a = a, c = c, d = d}
  f:flush()
end
callbacks:add("frame", function()
  local n = emu:currentFrame()
  if n > 200 and n < 1500 then
    if n % 80 < 6 then emu:setKeys(1) else emu:setKeys(0) end
  elseif n >= 1500 then
    local k = math.floor(n / 45) % 8
    local keys = {32 + 1, 16 + 1, 1, 2, 64, 128, 32 + 2, 16 + 1}   -- LEFT/RIGHT with A, A, B, UP, DOWN ...
    emu:setKeys(keys[k + 1])
  end
  if n == 200 then emu:screenshot(out .. "title.png"); stats("title") end
  if n == 1800 then emu:screenshot(out .. "s1800.png"); stats("t1800") end
  if n == 3500 then
    local t = "sram:"
    for i = 0, 15 do t = t .. string.format(" %02x", emu:read8(0x0E000000 + i)) end
    for i = 0, 15 do t = t .. string.format(" %02x", emu:read8(0x0E000100 + i)) end
    f:write(t .. string.char(10)); f:flush()
  end
  if n == 3000 then emu:screenshot(out .. "s3000.png"); stats("t3000") end
  if n == 4500 then emu:screenshot(out .. "s4500.png"); stats("t4500") end
  if n == 6000 then emu:screenshot(out .. "s6000.png"); stats("t6000"); f:write("done" .. string.char(10)); f:close() end
end)
