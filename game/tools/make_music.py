"""STARLIFT's music: spacey songs in the manner of 1950s science-fiction films, as step sequences for the software FM
synth (CHStarlift/Synth.cpp). A theremin (a near-sine voice that glides between notes and wavers), vibraphones, bells
and a soft bass, mostly over light drums.

  0 title     MOONGLOW   G major, 84 bpm              a gentle tune over G, Em, C, D (STARRISE, the old title, is kept) (THEREMIN, C lydian augmented, is kept for later)
  1 the moon  TRANQUIL   F lydian, 80 bpm             slow bells and a drone, almost no drums
  2 ruins     SANDS      E double harmonic, 100 bpm   an exotic theremin line over a half-time groove
  3 bases     OUTPOST    D dorian, 120 bpm            a busy sequencer and a pulse, four on the floor
  4 jungle    JUNGLE     G mixolydian, 104 bpm        exotica: vibes, a bouncing bass, bongo-like toms
  5 alien     INVASION   B phrygian, 112 bpm          a B-movie march, the theremin wailing in tritones
  6 titan     COSMOS     A mixolydian, 88 bpm         wide bells and a theremin, a slow pulse

A song is 16 bars of sixteen steps; three streams of one byte a step:
  lead, bass: 0 = rest (the note is released), 1 = hold, 2..127 = a MIDI note (a new note);
  drums: a bit mask, 1 kick, 2 snare, 4 closed hat, 8 open hat.
The third voice plays the lead delayed by a few steps and shifted by an interval (an echo, or a harmony).
Each song names its own FM instruments; a lead with glide slides between notes, with vib it wavers.

Writes CHStarlift/Music.h and out/card/songs.bin (tools/make_card.py runs this). Run: python tools/make_music.py
"""
import math
import os

RATE = 8000                        # the synth's sample rate: Synth.cpp's SYN_RATE must match
N = None
H = 'H'                            # hold the note (a tie): the theremin's long glides

SCALES = {
    'dorian': [0, 2, 3, 5, 7, 9, 10],
    'phrygian': [0, 1, 3, 5, 7, 8, 10],
    'doubleharm': [0, 1, 4, 5, 7, 8, 11],
    'minor': [0, 2, 3, 5, 7, 8, 10],
    'lydian': [0, 2, 4, 6, 7, 9, 11],
    'lydaug': [0, 2, 4, 6, 8, 9, 11],              # lydian augmented: the eerie raised fifth
    'mixolydian': [0, 2, 4, 5, 7, 9, 10],
    'whole': [0, 2, 4, 6, 8, 10],
    'major': [0, 2, 4, 5, 7, 9, 11],
}


def deg(scale, d):
    s = SCALES[scale]
    return s[d % len(s)] + 12 * (d // len(s))


def lead_stream(root, scale, pats, order, shifts):
    out = []
    for bar, name in enumerate(order):
        for d in pats[name]:
            out.append(0 if d is None else 1 if d == H else root + shifts[bar % 8] + deg(scale, d))
    return out


BASS = {
    'pulse': [0, N, N, N, N, N, N, N, 0, N, N, N, 7, N, N, N],
    'long': [0, H, H, H, H, H, H, H, 7, H, H, H, H, H, H, H],
    'drone': [0] + [H] * 15,
    'eighths': [0, N] * 8,
    'seq': [0, N, 12, 0, N, 0, 12, N, 0, N, 12, 0, 7, N, 12, N],
    'bounce': [0, N, N, 7, N, N, 12, N, 0, N, N, 7, N, 12, 7, N],
    'march': [0, N, N, N, 7, N, N, N, 0, N, N, N, 7, N, 6, N],
    'dub': [0, N, N, 0, N, N, 7, N, 0, N, 3, N, 7, N, 5, N],
}


def bass_stream(root, style, shifts, bars=16):
    out = []
    for bar in range(bars):
        for o in BASS[style]:
            out.append(0 if o is None else 1 if o == H else root - 12 + shifts[bar % 8] + o)
    return out


def drums(style, bars=16):
    out = []
    for bar in range(bars):
        d = [0] * 16
        if style == 'air':                                   # a hat now and then
            if bar % 2 == 0:
                d[8] |= 4
            if bar % 4 == 3:
                d[14] |= 4
        elif style == 'soft':                                # a soft pulse and a hat
            if bar % 2 == 0:
                d[0] |= 1
            d[8] |= 4
            if bar % 2:
                d[12] |= 4
        elif style == 'down':                                # half time: kick on 1 and the 'and' of 2, snare on 3
            d[0] |= 1; d[7] |= 1; d[8] |= 2
            if bar % 2:
                d[10] |= 1
            for s in (4, 12, 15):
                d[s] |= 4
        elif style == 'four':                                # four on the floor, open hats off the beat
            for s in (0, 4, 8, 12):
                d[s] |= 1
            for s in (2, 6, 10, 14):
                d[s] |= 8
            if bar >= 4:
                d[4] |= 2; d[12] |= 2
        elif style == 'exotica':                             # toms (snares) in a bongo pattern
            d[0] |= 1; d[8] |= 1
            for s in (3, 6, 11, 14):
                d[s] |= 2
            for s in (2, 10):
                d[s] |= 4
        elif style == 'march':                               # a military snare over a kick on the beat
            for s in (0, 8):
                d[s] |= 1
            for s in (4, 12):
                d[s] |= 2
            for s in (14, 15):
                d[s] |= 2 if bar % 2 else 4
        if bar % 8 == 7 and style in ('four', 'march', 'down'):
            d[12] |= 1; d[13] |= 4; d[14] |= 2; d[15] |= 2
        out.append(d)
    return [x for b in out for x in b]


SONGS = []


def add(name, bpm, sweep, inst, lead, bass, delay, dr, shift=12):
    assert len(lead) == len(bass) == len(dr) == 256, name
    SONGS.append(dict(name=name, ms=int(round(60000 / bpm / 4)), sweep=sweep, inst=inst, lead=lead, bass=bass,
                      delay=delay, drums=dr, shift=shift))


# Instruments: (ratio, idx0, idxDecay, idxFloor, amp, volDecay, volFloor, drive, vib, glide, release)
THEREMIN = (1, 12, 1, 5, 88, 0, 80, 16, 12, 5, 3)          # near-sine, a deep waver, a slow glide
THEREMIN_HI = (1, 8, 1, 4, 70, 0, 62, 16, 14, 4, 3)
VIBES = (4, 80, 5, 0, 74, 3, 0, 16, 2, 0, 10)               # a bright strike that fades to a pure tone
BELL = (3, 96, 2, 12, 64, 1, 18, 16, 0, 0, 5)
BELL_ECHO = (3, 70, 2, 10, 42, 1, 12, 16, 0, 0, 5)
SOFT_BASS = (1, 30, 2, 6, 100, 2, 56, 16, 0, 0, 12)
DRONE = (1, 18, 0, 18, 70, 0, 66, 16, 3, 0, 2)
SEQ = (2, 64, 7, 8, 80, 6, 10, 16, 0, 0, 16)
SEQ_ECHO = (2, 50, 7, 6, 50, 6, 8, 16, 0, 0, 16)
PLUCK_BASS = (1, 56, 6, 10, 104, 4, 36, 18, 0, 0, 16)

# ---- 0 title: THEREMIN. The theremin glides through a slow, eerie tune; the vibes echo it an octave up
P = {'A': [4, H, H, H, H, H, 3, H, 5, H, H, H, 4, H, H, H],
     'B': [7, H, H, H, 6, H, 5, H, 4, H, H, H, H, H, N, N],
     'C': [9, H, H, H, 8, H, H, H, 7, H, 6, H, 4, H, H, H],
     'D': [2, H, 3, H, 4, H, H, H, 0, H, H, H, H, H, N, N]}
order = list('ABAC' + 'ABCD' + 'CCAB' + 'ABCD')
sh = [0, 0, 0, 0, -2, -2, 2, 2]
add('THEREMIN', 72, 6, [THEREMIN, SOFT_BASS, VIBES], lead_stream(60, 'lydaug', P, order, sh),
    bass_stream(60, 'long', sh), 4, drums('air'), shift=12)

# ---- 1 the moon: TRANQUIL. Long bells over a drone, a few soft hats
P = {'A': [7, N, N, N, 9, N, N, N, 11, N, N, N, 9, N, N, N],
     'B': [4, N, N, N, 6, N, 7, N, 9, N, N, N, N, N, N, N],
     'C': [11, N, 9, N, 7, N, 6, N, 4, N, N, N, 2, N, N, N]}
order = list('ABAC' + 'ABAC' + 'CBAB' + 'ABAC')
add('TRANQUIL', 80, 10, [BELL, DRONE, BELL_ECHO], lead_stream(65, 'lydian', P, order, [0, 0, 0, 0, -2, -2, 0, 0]),
    bass_stream(65, 'drone', [0, 0, 0, 0, -2, -2, 0, 0]), 6, drums('air'))

# ---- 2 ruins: SANDS. An exotic theremin over a half-time groove
P = {'A': [0, H, 1, H, 4, H, H, H, 5, H, 4, H, 1, H, 0, H],
     'B': [4, H, 5, H, 7, H, H, H, 8, H, 7, H, 5, H, 4, H],
     'C': [7, H, H, H, 8, 7, 5, H, 4, H, 5, H, 1, H, H, H],
     'D': [8, H, 7, H, 5, H, 4, H, 1, H, H, H, 0, H, H, H]}
order = list('AABA' + 'AABC' + 'BBCD' + 'AACD')
sh = [0, 0, 0, 0, 5, 5, 0, 0]
add('SANDS', 100, 12, [THEREMIN, PLUCK_BASS, BELL_ECHO], lead_stream(64, 'doubleharm', P, order, sh),
    bass_stream(64, 'dub', sh), 3, drums('down'), shift=12)

# ---- 3 bases: OUTPOST. A sequencer line echoed a fifth up, a pulsing bass, four on the floor
P = {'A': [0, 2, 4, 2, 7, 4, 2, 4, 0, 2, 4, 2, 9, 7, 4, 2],
     'B': [3, 5, 7, 5, 10, 7, 5, 7, 3, 5, 7, 5, 9, 7, 5, 4],
     'C': [7, N, 9, N, 10, N, 9, 7, 5, N, 4, N, 2, N, 0, N]}
order = list('AAAB' + 'AAAB' + 'CCAB' + 'AACB')
sh = [0, 0, 0, 0, -2, -2, 3, 3]
add('OUTPOST', 120, 20, [SEQ, PLUCK_BASS, SEQ_ECHO], lead_stream(62, 'dorian', P, order, sh),
    bass_stream(62, 'seq', sh), 3, drums('four'), shift=7)

# ---- 4 jungle: JUNGLE. Exotica: vibes over a bouncing bass and bongo-like toms
P = {'A': [4, N, 2, N, 4, N, 7, N, 6, N, 4, N, 2, N, N, N],
     'B': [7, N, 9, N, 7, N, 4, N, 2, N, 4, N, 0, N, N, N],
     'C': [9, N, 9, N, 10, N, 9, N, 7, N, 6, N, 7, N, N, N]}
order = list('ABAB' + 'ABAC' + 'CBCB' + 'ABAC')
sh = [0, 0, 5, 5, 0, 0, -2, -2]
add('JUNGLE', 104, 8, [VIBES, SOFT_BASS, THEREMIN_HI], lead_stream(67, 'mixolydian', P, order, sh),
    bass_stream(67, 'bounce', sh), 5, drums('exotica'), shift=12)

# ---- 5 alien: INVASION. A march; the theremin wails in tritones, the vibes answer
P = {'A': [0, H, H, H, 1, H, H, H, 4, H, H, H, 3, H, 1, H],
     'B': [4, H, 5, H, 4, H, 1, H, 0, H, H, H, H, H, N, N],
     'C': [7, H, H, H, 6, H, 5, H, 4, H, 3, H, 1, H, 0, H],
     'D': [8, H, 7, H, 8, H, 9, H, 7, H, H, H, 4, H, H, H]}
order = list('AABA' + 'AABC' + 'DDCB' + 'AACB')
sh = [0, 0, 0, 0, 1, 1, 0, 6]
add('INVASION', 112, 16, [THEREMIN, PLUCK_BASS, VIBES], lead_stream(71, 'phrygian', P, order, sh),
    bass_stream(59, 'march', sh), 2, drums('march'), shift=6)

# ---- 6 titan: COSMOS. Wide bells and the theremin, a slow pulse
P = {'A': [7, H, H, H, H, H, 6, H, 4, H, H, H, H, H, N, N],
     'B': [9, H, H, H, 10, H, 9, H, 7, H, H, H, 4, H, H, H],
     'C': [11, H, H, H, 9, H, 7, H, 6, H, 7, H, 9, H, H, H]}
order = list('ABAC' + 'ABAC' + 'BCBC' + 'ABAC')
sh = [0, 0, -2, -2, 5, 5, 0, 0]
add('COSMOS', 88, 10, [THEREMIN, SOFT_BASS, BELL_ECHO], lead_stream(57, 'mixolydian', P, order, sh),
    bass_stream(57, 'pulse', sh), 4, drums('soft'), shift=19)


# ---- the title: STARRISE. A pleasant tune in C major over C, Am, F, G in classic FM colours: an electric piano lead,
# a punchy FM bass, FM bells echoing an octave up, the index sweeping open and shut; light drums, loud enough for a room
LEAD_EPIANO = (1, 100, 3, 26, 114, 1, 72, 16, 3, 0, 6)     # an FM electric piano: a bright, bell-edged strike that mellows
BASS_FM = (1, 84, 6, 14, 110, 3, 52, 16, 0, 0, 12)        # a punchy FM bass
BELL_FM = (3, 96, 3, 10, 58, 2, 10, 16, 0, 0, 8)          # FM bells (3:1) an octave up
P = {'A': [4, H, H, 2, 0, H, 2, H, 4, H, 7, H, 4, H, H, H],
     'B': [5, H, 4, H, 2, H, 4, H, 2, H, H, H, N, N, N, N],
     'C': [7, H, H, 9, 7, H, 4, H, 5, H, 4, H, 2, H, H, H],
     'D': [4, H, 2, H, 0, H, H, H, H, H, H, H, N, N, N, N]}
order = list('ABAC' + 'ABAD' + 'CCAB' + 'ACAD')
sh = [0, 0, -3, -3, 5, 5, 7, 7]
add('STARRISE', 100, 18, [LEAD_EPIANO, BASS_FM, BELL_FM], lead_stream(72, 'major', P, order, [0] * 8),
    bass_stream(60, 'pulse', sh), 4, drums('soft'), shift=12)

# ---- the title since 0.8: MOONGLOW. Gentler than STARRISE (which grated): G major, 84 bpm, a mellow low-index FM lead
# in long notes over I, vi, IV, V, a held bass, a soft bell echo an octave up and hardly any drums
LEAD_MELLOW = (1, 40, 2, 8, 100, 1, 64, 16, 4, 0, 8)      # a warm, round tone: little FM brightness, a slight waver
P = {'A': [4, H, H, H, H, H, 2, H, 0, H, H, H, 2, H, 4, H],
     'B': [5, H, H, H, 4, H, H, H, 2, H, H, H, H, H, N, N],
     'C': [3, H, H, H, 5, H, 7, H, 9, H, H, H, 7, H, 5, H],
     'D': [6, H, H, H, 4, H, H, H, 1, H, H, H, H, H, N, N],
     'E': [7, H, H, H, H, H, 6, H, 4, H, H, H, 2, H, 4, H],
     'F': [5, H, 4, H, 2, H, H, H, 0, H, H, H, H, H, N, N],
     'G': [3, H, 2, H, 3, H, 5, H, 4, H, H, H, H, H, 2, H],
     'K': [6, H, H, H, 4, H, 1, H, 4, H, H, H, H, H, N, N]}
order = list('ABCD' + 'EFGK' + 'EBCD' + 'AFGK')
sh = [0, -3, 5, 7, 0, -3, 5, 7]
add('MOONGLOW', 84, 6, [LEAD_MELLOW, SOFT_BASS, BELL_ECHO], lead_stream(67, 'major', P, order, [0] * 8),
    bass_stream(55, 'long', sh), 6, drums('air'), shift=12)

# ---- the bosses (0.19): SHOWDOWN. E phrygian, 132 bpm: the theremin wails over a driving bass, four on the floor
P = {'A': [0, H, 1, H, 0, H, N, 3, H, 1, H, 0, H, H, N, N],
     'B': [4, H, 5, H, 4, H, 3, H, 1, H, H, H, 0, H, N, N],
     'C': [7, H, H, H, 8, H, 7, H, 5, H, 4, H, 3, H, 1, H],
     'D': [8, H, 7, H, 8, H, 9, H, 7, H, H, H, H, H, N, N]}
order = list('AABA' + 'AABC' + 'CDCD' + 'AABD')
sh = [0, 0, 0, 0, 1, 1, 0, 0]
add('SHOWDOWN', 132, 20, [THEREMIN, PLUCK_BASS, SEQ_ECHO], lead_stream(64, 'phrygian', P, order, sh),
    bass_stream(52, 'eighths', sh), 3, drums('four'), shift=7)

# 0 the title; 1..8 the planets in order (Moon, Ares, Cryo, Verda, Vulcan, Titan, Prism, Zorg); 9 the bosses
byname = {sg['name']: sg for sg in SONGS}
ORDER = ('MOONGLOW', 'TRANQUIL', 'SANDS', 'OUTPOST', 'JUNGLE', 'INVASION', 'COSMOS', 'SANDS', 'INVASION', 'SHOWDOWN')
SONGS = [byname[n] for n in ORDER]

# Each song is one self-contained blob in STARLIFT.DAT (tools/make_card.py puts it there); the game loads the one it
# needs into RAM when the music changes. Blob: 3 instruments (11 B each), stepMs, sweep, delay, shift, the number of
# bars, the lead, bass and drum streams (16 bar numbers each), then the song's own bars (16 steps each).
blobs, offs = [], []
done = {}
for sg in SONGS:
    nm = sg['name']
    if nm not in done:
        bars, ix = [], {}
        streams = []
        for part in ('lead', 'bass', 'drums'):
            st = []
            for k in range(16):
                bar = tuple(sg[part][k * 16:k * 16 + 16])
                if bar not in ix:
                    ix[bar] = len(bars)
                    bars.append(bar)
                st.append(ix[bar])
            streams.append(st)
        b = bytearray()
        for ins in sg['inst']:
            b += bytes(ins)
        b += bytes([sg['ms'], sg['sweep'], sg['delay'], sg['shift'] & 255, len(bars)])
        for st in streams:
            b += bytes(st)
        for bar in bars:
            b += bytes(bar)
        done[nm] = (sum(len(x) for x in blobs), len(b))
        blobs.append(b)
    offs.append(done[nm])
CARD = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'out', 'card')
os.makedirs(CARD, exist_ok=True)
open(os.path.join(CARD, 'songs.bin'), 'wb').write(b''.join(blobs))

out = ["// Generated by tools/make_music.py: the synth's tables, and where each song is in STARLIFT.DAT's song section.",
       "#pragma once", "#include <stdint.h>", "", "#define SYN_RATE %d" % RATE, "#define SONG_COUNT %d" % len(SONGS),
       "#define SONG_MAX %d                 // the largest song blob: the RAM buffer it loads into" % max(len(b) for b in blobs), ""]
sine = [int(round(127 * math.sin(i * 2 * math.pi / 256))) for i in range(65)]
out.append("static const int8_t QSINE[65] = { %s };   // a quarter of a sine wave: SIN() builds the rest" % ",".join(map(str, sine)))
out.append("static inline int SIN(uint8_t i) { const uint8_t q = i & 63; const int v = QSINE[(i & 64) ? 64 - q : q]; return (i & 128) ? -v : v; }")
inc16 = [int(round(440 * 2 ** ((60 + k - 69) / 12) * 65536 / RATE * 16)) for k in range(12)]
assert max(inc16) < 65536
out.append("static const uint16_t NOTE_INC16[12] = { " + ", ".join(str(v) for v in inc16) + " };")
out.append("// an FM voice: the modulator's ratio to the carrier, its index at the start, how fast it falls and where it stops,")
out.append("// the volume at the start, how fast it falls while held and where it stops; drive (16 = clean), vibrato depth,")
out.append("// glide speed (0 = none) and how fast a released note dies")
out.append("struct Inst { uint8_t ratio, idx0, idxDecay, idxFloor, amp, volDecay, volFloor, drive, vib, glide, release; };")
out.append("enum { SB_STEP = 33, SB_SWEEP, SB_DELAY, SB_SHIFT, SB_NBARS, SB_LEAD, SB_BASS = SB_LEAD + 16, SB_DRUMS = SB_BASS + 16, SB_BARS = SB_DRUMS + 16 };   // a song blob's layout")
out.append("static const uint16_t SONG_OFF[SONG_COUNT] = { %s };" % ", ".join(str(o) for o, n in offs))
out.append("static const uint16_t SONG_LEN[SONG_COUNT] = { %s };" % ", ".join(str(n) for o, n in offs))
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CHStarlift', 'Music.h'), 'w', newline='').write(chr(10).join(out) + chr(10))
print("wrote Music.h and out/card/songs.bin: %d songs, %d bytes" % (len(SONGS), sum(len(b) for b in blobs)))
