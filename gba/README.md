# STARLIFT for the Game Boy Advance (StarLift Advance)

    python gba/build.py            # the committed game (git HEAD) -> gba/out/STARLIFT.gba
    python gba/build.py --live     # the working tree as it is (it must compile; CardIndex.h must match dist/STARLIFT.DAT)
    python gba/build.py --rev=0.11 # any commit

zig is the only compiler needed. By default the build exports the committed game (`git archive`, read-only) into
`gba/build/head`, runs `tools/make_card.py` there so the card and `CardIndex.h` match, and compiles that. Edits in progress in
the working tree are never touched and never break the port. The game (`CHStarlift.ino`, `Card.cpp`, `Synth.cpp`) and the
vendored CHGame library compile unchanged. `gba/src` is the GBA back end:

| File | Does |
|---|---|
| `crt0.S`, `gba.ld` | header, stacks, copies hot code and data to IWRAM; picks the hot functions by section name (see below) |
| `gfx_gba.cpp` | replaces CHGfx.cpp: the 4-bit framebuffer is already GBA 4bpp tile data, so a flush is a word copy into a tile set; two sets, flipped at VBlank with the palette. `GBA_SCALE` (top of the file): 125 (default) shows 160x160, 1.25x nearest neighbour, centred, black borders, as four 64x64 4bpp affine sprites (affine sprites take 4-bit tiles, affine backgrounds don't) over a colour-0 BG0, clipped by window 0; 100 is 1:1 on BG0. Same copy cost either way |
| `gba_audio.cpp` | the game's synth (patch `10-synth-blocks`: `blocks::render()`, the same samples as `synth::sample()` a block at a time, before its 8-bit knee) mixed in the VBlank interrupt at 8004 Hz, a gentle knee, 4x linear interpolation to 32017 Hz (timer 0), FIFO A 100% + FIFO B 50% as one 9-bit mono channel (DMA 1, 2) |
| `fat_rom.cpp` | `fat::open/read`: STARLIFT.DAT is appended to the ROM |
| `save_gba.cpp` | `save::` in 32 KB battery SRAM (runs from IWRAM: the cartridge bus can't reach SRAM while fetching ROM) |
| `div.S` | integer division (the ARM7TDMI has none) |
| `main.cpp`, `libc.cpp` | clocks, buttons, interrupts (and a 1 kHz PC sampler), memcpy |

Controls: the GBA's A, B, D-pad, START and SELECT are CHGame's. START held 3 s soft-resets.

## Build notes (each one cost a crash)

- Compile Thumb with `-target thumb-linux-musleabi`: `-mthumb` before or after `-target arm-...` was ignored, and ARM code
  turned `swi 2` (Halt) into a soft reset.
- `-mlong-calls`: lld makes ARMv5 `ldr pc` thunks for ARM/Thumb calls, which don't switch state on the ARM7TDMI.
- Hot code lives in IWRAM: the game's RAMFUNCs, `Synth.cpp`'s sample loop (`blocks::render`, `envTick`, `nextStep`; the calls from the game stay in ROM), `sketch.cpp`'s scenery loops (compiled as ARM, -O3) and CHGfx's
  `gfx_hline`/`fillRect`. Sections are named in `gba.ld`; IWRAM is 28 KB of 32 (about 23 KB used).
- The game code in the header must not match a commercial game: mGBA's override for `ASLE` disabled SRAM. It is `CHSL`.

## Measured (mGBA nightly, no BIOS, scripted play)

| | |
|---|---|
| Frame, typical play | 35–40 ms (25–28 fps; the board does 30) |
| Worst seen | 47 ms (21 fps) |
| Sound mixing | 11.0% of the CPU, 1.83 ms a VBlank (`tools/sounddump.py`, 3540 frames; 17% before patch 10) |
| Screen copy | 0.8–0.9 ms a frame, 1x and 1.25x alike |
| 6000 frames of play | title, stage cards, flying, a crash, game over, a high score saved to SRAM, back to the title: no crash |

The game counts frames, so below 30 fps it runs a little slow. Tested in mGBA and on the MiSTer GBA core (sound included); not yet tried on a real GBA.

The ROM header's 156-byte boot logo belongs to Nintendo and is not in this repository. `build.py` reads it from
`gba/logo.bin` (untracked; bytes 0x04-0x9F of any GBA ROM) or the file named by `GBA_LOGO`. Without it the header logo
is left empty: emulators run the ROM, and for real hardware run devkitPro's `gbafix` on it. The fixed byte and
complement check are written either way.

## Tools (`gba/tools`, need a nightly mGBA for `--script`)

- `runmgba.py <mGBA.exe> <outdir>`: plays `smoke.lua` (button presses), writes screenshots and frame timings.
- `pcprof.py <mGBA.exe> <outdir> [from] [to]`: a sampling profile by function.
- `trace.py`: the order a few functions run in (for crashes). `elfsym.py`: symbol addresses.
- `sounddump.py <mGBA.exe> <outdir> [frames]`: saves every sound buffer the ROM mixes as `sound.wav`, with its levels, frame seams and mixing time.
- `synthcheck.py` (no mGBA): patch 10's `blocks::render()` against the game's `synth::sample()`, bit for bit. Run it after any change to `Synth.cpp` or `Music.h`.

Credits: game by Jadflat; built on CHGame, Copyright 2026 Kevin Bates, Apache-2.0.
