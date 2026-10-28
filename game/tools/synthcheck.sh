#!/bin/sh
# Render each song on the PC (8 kHz, 20 s, an effect at 3 s) and print its level and clipping; WAVs go to out/music_preview.
S=out/synthtest
zig c++ -O2 -std=c++17 -I$S/inc -ICHStarlift $S/main.cpp CHStarlift/Synth.cpp -o $S/t.exe || exit 1
for n in 0 1 2 3 4 5 6; do $S/t.exe $n 20 $S/s$n.raw; done
python - <<'P'
import numpy as np, wave, os
os.makedirs('out/music_preview', exist_ok=True)
for n in range(7):
    a = np.fromfile(f'out/synthtest/s{n}.raw', dtype=np.uint8)
    w = wave.open(f'out/music_preview/song{n}.wav', 'wb'); w.setnchannels(1); w.setsampwidth(1); w.setframerate(8000); w.writeframes(a.tobytes()); w.close()
    b = a.astype(int)[8000:] - 128
    sil = (np.abs(b.reshape(-1, 800)).max(axis=1) < 4).mean()
    print(n, 'rms %.1f' % np.sqrt((b**2).mean()), 'clip%% %.2f' % (100*(abs(b) >= 120).mean()), 'silent tenths %.0f%%' % (100*sil))
P
