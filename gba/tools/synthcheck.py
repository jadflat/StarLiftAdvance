#!/usr/bin/env python3
"""Check patch 10-synth-blocks on the PC: blocks::render() with the synth's knee and clamp must give synth::sample()'s
output bit for bit. Plays all songs (24 s each, the engine, every effect) both ways in frames of 134 samples.

    python gba/tools/synthcheck.py        (needs out/card/songs.bin: python tools/make_card.py)

Run it after any change to the game's Synth.cpp or Music.h: the patch copies sample()'s mixing, so it must follow.
"""
import os, subprocess, sys
from pathlib import Path

GBA = Path(__file__).resolve().parent.parent
ROOT = GBA.parent / "game"            # the game, this project's copy (build.py's ROOT)
GAME = ROOT / "CHStarlift"
B = GBA / "build" / "synthcheck"
B.mkdir(parents=True, exist_ok=True)
songs = next((s for s in (ROOT / "out/card/songs.bin", Path(os.environ.get("GBA_BUILD", GBA / "build")) / "head/out/card/songs.bin")
              if s.exists()), None)      # (or the one build.py made in its copy of the game)
if not songs:
    sys.exit("songs.bin is missing: run python tools/make_card.py in game/, or build first")

(B / "CHGame.h").write_text(
    "#pragma once\n#include <stdint.h>\n"
    "namespace audio { struct Step { uint8_t hz, endHz, ms; }; struct Effect { const Step *steps; uint8_t n, flags; };"
    " enum : uint8_t { SOFT = 0x10, GLIDE = 0x20 }; }\n", newline="\n")
ino = (GAME / "CHStarlift.ino").read_text(encoding="utf-8")
(B / "sounds.inc").write_text(ino[ino.index("enum Sfx : uint8_t {"):ino.index("static NOINL void sfx(uint8_t id)")], newline="\n")
(B / "synth.cpp").write_text((GAME / "Synth.cpp").read_text(encoding="utf-8") + '#include "synth_blocks.inc"\n', newline="\n")
(B / "main.cpp").write_text(r'''
#include "Synth.h"
#include "Music.h"
#include <stdio.h>
#include <string.h>
#define AUDIO_STEPS(name) static const audio::Step name[]
#define AUDIO_STEP(hz, end, ms) { (uint8_t)(((hz) + 10) / 20), (uint8_t)(((end) + 10) / 20), (uint8_t)(((ms) + 1) / 2) }
#define AUDIO_REST(ms) { 0, 0, (uint8_t)(((ms) + 1) / 2) }
#define AUDIO_EFFECT(steps, flags) { steps, (uint8_t)(sizeof(steps) / sizeof((steps)[0])), (uint8_t)(flags) }
#include "sounds.inc"
namespace synth { uint8_t sample(); }
namespace blocks { void render(int16_t *out, int n); }
int main(int argc, char **argv) {
    const bool blk = argv[1][0] == '1';
    static uint8_t blob[4096];
    FILE *b = fopen(argv[2], "rb"); fread(blob, 1, sizeof blob, b); fclose(b);
    FILE *f = fopen(argv[3], "wb");
    for (int song = 0; song < SONG_COUNT; song++) {
        synth::music(song);
        for (int fr = 0; fr < 60 * 24; fr++) {
            if (synth::wanted() >= 0) { const int w = synth::wanted(); memcpy(synth::buffer(), blob + SONG_OFF[w], SONG_LEN[w]); synth::loaded(); }
            if (fr == 360) synth::engine(200);
            if (fr == 600) synth::engine(0);
            if (fr >= 700 && fr % 23 == 0) { const SfxAt &a = SOUNDS[(fr / 23) % S_COUNT]; const audio::Effect e = { SFX_STEPS + a.at, a.n, a.flags }; synth::sfx(e); }   // as the game's sfx()
            if (fr == 1000) synth::engine(120);
            uint8_t o[134];
            if (blk) {
                int16_t x[134];
                blocks::render(x, 134);
                for (int k = 0; k < 134; k++) {
                    int mix = x[k];
                    if (mix > 96) mix = 96 + ((mix - 96) >> 2); else if (mix < -96) mix = -96 + ((mix + 96) >> 2);
                    o[k] = (uint8_t)(128 + (mix > 127 ? 127 : mix < -128 ? -128 : mix));
                }
            } else for (int k = 0; k < 134; k++) o[k] = synth::sample();
            fwrite(o, 1, 134, f);
        }
    }
    fclose(f);
}
''', newline="\n")
exe = B / "synthcheck.exe"
subprocess.run(["zig", "c++", "-O2", "-std=c++17", f"-I{B}", f"-I{GBA / 'patches'}", f"-I{GAME}", str(B / "main.cpp"), str(B / "synth.cpp"),
                "-o", str(exe)], check=True)
for m in "01":
    subprocess.run([str(exe), m, str(songs), str(B / f"out{m}.raw")], check=True)
a, c = (B / "out0.raw").read_bytes(), (B / "out1.raw").read_bytes()
silent = a.count(128) / len(a)
if a == c and silent < 0.5:
    print(f"synthcheck: blocks::render() = synth::sample() over {len(a)} samples ({len(a) / 8004:.0f} s)")
else:
    first = next((i for i in range(len(a)) if a[i] != c[i]), None)
    sys.exit(f"synthcheck FAILED: first difference at sample {first}, silent {silent:.0%}")
