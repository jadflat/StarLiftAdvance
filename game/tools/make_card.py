"""Builds STARLIFT.DAT, the game's file on the SD card, and CHStarlift/CardIndex.h, which the game is compiled against.

  block 0           "STARLIFT", the build's hash (a CRC-32 of the rest), the sections' first blocks
  CARD_PIC blocks   the big pictures (tools/make_sprites.py: out/card/pics.bin)
  CARD_SONG blocks  the songs, one blob each (tools/make_music.py: out/card/songs.bin)
  CARD_TXT blocks   the how-to-play text
  CARD_PLANET       the eight worlds, PLANET_LEN bytes each

Runs both generators first. Run: python tools/make_card.py   (copy out/card/STARLIFT.DAT to the card's root)
"""
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
CARD = os.path.join(ROOT, 'out', 'card')

for g in ('make_sprites.py', 'make_music.py'):
    subprocess.run([sys.executable, os.path.join(HERE, g)], check=True)


def padded(b):
    return b + bytes(-len(b) % 512)


# the how-to-play pages: (page, x, y, colour, text); colours are the game's (Game.h: 1 white, 4 green, 11 accent, 12 silver, 14 gold, 15 red)
WH, GR, AC, SI, GO, RE = 1, 4, 11, 12, 14, 15
HELP = [
    (0, 9, 24, WH, "LEFT/RIGHT  TILT"), (0, 9, 32, SI, "A           MAIN ENGINE"), (0, 9, 40, WH, "B           CANNON + LASER"),
    (0, 9, 48, SI, "HOLD UP     CHARGE SHOT"),
    (0, 9, 56, WH, "DOWN TWICE  LANDING GEAR"), (0, 9, 64, SI, "START       PAUSE MENU"), (0, 9, 72, WH, "HOLD START  CHGAME MENU"),
    (0, 9, 86, AC, "NEWTON RULES:"), (0, 9, 94, AC, "NO BRAKES IN SPACE!"),
    (1, 8, 24, WH, "COLONISTS WAIT ON FLAT GROUND."), (1, 8, 32, GO, "LAND BESIDE THEM, NOT ON THEM!"),
    (1, 8, 40, WH, "THEY RUN ABOARD: 8 SEATS."), (1, 8, 48, WH, "FLY THEM TO THE BASE PAD AT"), (1, 8, 56, WH, "THE FAR END: IT REFUELS AND"),
    (1, 8, 64, WH, "REPAIRS THE LANDER."), (1, 8, 72, GO, "LAND: GEAR DOWN, LEVEL, SLOW."), (1, 8, 80, WH, "BUMP OR SHOOT A SAUCER AS"),
    (1, 8, 88, WH, "ITS TAKEAWAY COMES UP: THE"), (1, 8, 96, WH, "FOOD IS YOURS. BRING IT HOME!"),
    (2, 8, 22, WH, "UFOS BEAM COLONISTS UP AND"), (2, 8, 30, WH, "TAKE 5 TO THEIR COLLECTOR."), (2, 8, 38, GR, "SHOOT ONE: THEY JET DOWN."),
    (2, 8, 50, WH, "STINGERS SPIT PLASMA, MINES"), (2, 8, 58, WH, "DRIFT, TURRETS FIRE FLAK."),
    (2, 8, 70, GO, "MOTHERSHIPS: ONLY LASERS HURT"), (2, 8, 78, GO, "THEM. SHOOT ONE OPEN TO FREE"), (2, 8, 86, GO, "EVERYONE IT TOOK."),
    (2, 8, 98, RE, "HULL: EASY 8, NORMAL 5, HARD 3."),
]

# ---- the eight worlds: everything that makes each one itself (Game.h's struct Planet has the same layout, byte for byte)
C = dict(INK=0, WHITE=1, SKY0=2, SKY1=3, GREEN=4, FAR=5, MID=6, NEAR=7, GNDD=8, GND=9, GNDL=10, ACC=11, SILVER=12, STEEL=13, GOLD=14, RED=15)
TH = dict(MOON=0, RUINS=1, BASE=2, ALIEN=3)
WX = dict(METEOR=0, DUST=1, SNOW=2, RAIN=3, ASH=4, DRIZZLE=5, GLINT=6, ACID=7)
STY = dict(SMOOTH=0, JAGGED=1, MESA=2, SPIKES=3, CONES=4)
SKY = dict(EARTH=0, SUN=1, AURORA=2, GIANT=3, REDSUN=4, RINGED=5, TWINSUN=6, MOONS=7)
TL = dict(STONES=0, BRICKS=1, STRATA=2, CRYSTAL=3)
FOOD = dict(BURGER=0, NOODLES=1, ICECREAM=2, PIZZA=3, HOTDOG=4, FISH=5, SUSHI=6, PIE=7)
PL = dict(ROCK=0, SPIKES=1, BULB=2, FERN=3, FLOWER=4, TUBE=5, EYE=6, CACTUS=7, MUSH=8)
FLY = dict(NONE=0, BAT=1, JELLY=2, RAY=3)
CRAWL = dict(NONE=0, HOPPER=1, BEETLE=2, SNAIL=3)
WORLDS = [
    dict(name="THE MOON", alien=(('GREEN','SILVER','STEEL','RED'),('ACC','GOLD','RED','GREEN')), base=0, coll=0, place="SEA OF TRANQUILITY", shop="BURGERS", food='BURGER', shopc='RED', basec='SILVER',
         pal=[(2, 2, 10), (14, 14, 34), (92, 92, 104), (132, 132, 140), (74, 74, 84), (40, 40, 48), (112, 112, 120), (184, 184, 192), (40, 90, 230)],
         grav=11, theme='MOON', wx='METEOR', sky='EARTH', style=('JAGGED', 'SMOOTH', 'SMOOTH'), tile='STONES', air=0, clouds='INK', crack='INK',
         fly='NONE', crawl='NONE', plants=('ROCK', 'ROCK', 'ROCK'), wind=0, ads=("LUNA COLA", "CRATER WAX", "MOON PIES")),
    dict(name="SABULON", alien=(('GREEN','STEEL','INK','RED'),('ACC','WHITE','SILVER','GREEN')), base=2, coll=2, worms=2, place="THE DEEP DESERT", shop="NOODLES", food='NOODLES', shopc='GOLD', basec='GNDL',
         pal=[(150, 82, 40), (244, 186, 112), (186, 120, 76), (222, 160, 92), (196, 132, 70), (150, 96, 50), (214, 160, 94), (248, 214, 150), (90, 170, 255)],
         grav=24, theme='RUINS', wx='DUST', sky='SUN', style=('MESA', 'SMOOTH', 'SMOOTH'), tile='STRATA', air=1, clouds='SKY1', crack='INK',
         fly='BAT', crawl='BEETLE', plants=('ROCK', 'CACTUS', 'ROCK'), wind=3, ads=("STILLSUIT CO", "WORM TOURS", "SAND SPA")),
    dict(name="CRYO", alien=(('ACC','GOLD','RED','GREEN'),('GREEN','SILVER','STEEL','RED')), base=1, coll=1, place="POLAR OUTPOST", shop="ICE CREAM", food='ICECREAM', shopc='ACC', basec='WHITE',
         pal=[(4, 10, 40), (34, 70, 140), (150, 186, 228), (96, 132, 192), (56, 86, 146), (64, 92, 146), (172, 210, 240), (244, 250, 255), (0, 230, 255)],
         grav=19, theme='BASE', wx='SNOW', sky='AURORA', style=('SPIKES', 'JAGGED', 'SMOOTH'), tile='STONES', air=1, clouds='INK', crack='GNDD',
         fly='RAY', crawl='HOPPER', plants=('SPIKES', 'ROCK', 'TUBE'), wind=2, ads=("ICE-O-MATIC", "FROST FUEL", "SNOW TIRES")),
    dict(name="VERDA", alien=(('GOLD','SILVER','STEEL','RED'),('RED','WHITE','SILVER','GREEN')), base=2, coll=3, place="THE LOST TEMPLE", shop="PIZZA", food='PIZZA', shopc='GREEN', basec='GNDL',
         pal=[(64, 22, 96), (222, 112, 160), (130, 84, 148), (44, 112, 76), (22, 70, 46), (50, 34, 26), (44, 140, 60), (148, 224, 94), (255, 170, 60)],
         grav=33, theme='RUINS', wx='RAIN', sky='GIANT', style=('SMOOTH', 'JAGGED', 'SMOOTH'), tile='STONES', air=2, clouds='FAR', crack='INK',
         fly='JELLY', crawl='HOPPER', plants=('BULB', 'FERN', 'MUSH'), wind=0, ads=("VINE JUICE", "TEMPLE TOURS", "SPORE-TEX")),
    dict(name="VULCAN", alien=(('ACC','SILVER','STEEL','GREEN'),('GREEN','WHITE','STEEL','GREEN')), base=3, coll=0, place="THE FORGE", shop="HOT DOGS", food='HOTDOG', shopc='ACC', basec='STEEL',
         pal=[(24, 0, 2), (150, 32, 12), (84, 24, 22), (50, 16, 18), (28, 10, 14), (22, 10, 10), (64, 32, 28), (124, 62, 42), (255, 124, 0)],
         grav=41, theme='ALIEN', wx='ASH', sky='REDSUN', style=('CONES', 'JAGGED', 'JAGGED'), tile='STRATA', air=1, clouds='FAR', crack='ACC',
         fly='BAT', crawl='BEETLE', plants=('FLOWER', 'ROCK', 'SPIKES'), wind=1, ads=("LAVA LAMPS", "MAGMA MINTS", "HOT ROCKS")),
    dict(name="TITAN", alien=(('RED','GOLD','STEEL','GREEN'),('GREEN','SILVER','STEEL','RED')), base=1, coll=2, place="RING STATION", shop="FISH+CHIPS", food='FISH', shopc='STEEL', basec='GNDL',
         pal=[(112, 60, 12), (232, 172, 72), (174, 114, 54), (124, 78, 42), (84, 52, 30), (70, 46, 26), (162, 112, 62), (222, 172, 104), (120, 230, 255)],
         grav=9, theme='BASE', wx='DRIZZLE', sky='RINGED', style=('SMOOTH', 'MESA', 'SMOOTH'), tile='STRATA', air=2, clouds='SKY1', crack='INK',
         fly='JELLY', crawl='SNAIL', plants=('TUBE', 'MUSH', 'ROCK'), wind=2, ads=("RING RENTALS", "GAS-O-LINE", "METHANE FIZZ")),
    dict(name="PRISM", alien=(('GOLD','WHITE','SILVER','RED'),('RED','GREEN','STEEL','GREEN')), base=3, coll=1, place="CRYSTAL SANCTUM", shop="SUSHI", food='SUSHI', shopc='WHITE', basec='GNDL',
         pal=[(20, 0, 40), (122, 40, 144), (94, 52, 146), (144, 72, 176), (72, 30, 104), (50, 20, 72), (150, 92, 194), (232, 172, 255), (90, 255, 230)],
         grav=29, theme='RUINS', wx='GLINT', sky='TWINSUN', style=('SPIKES', 'SPIKES', 'JAGGED'), tile='CRYSTAL', air=0, clouds='INK', crack='ACC',
         fly='RAY', crawl='SNAIL', plants=('SPIKES', 'BULB', 'EYE'), wind=0, ads=("GEM SHINE", "PRISM TV", "SPARKLE-OS")),
    dict(name="ZORG", alien=(('RED','SILVER','STEEL','GREEN'),('GOLD','GREEN','STEEL','RED')), base=0, coll=3, place="ZORG PRIME", shop="GLORP PIES", food='PIE', shopc='GREEN', basec='STEEL',
         pal=[(0, 10, 18), (22, 72, 72), (34, 64, 76), (22, 46, 56), (12, 28, 38), (14, 20, 26), (52, 72, 82), (112, 152, 162), (180, 255, 60)],
         grav=37, theme='ALIEN', wx='ACID', sky='MOONS', style=('SMOOTH', 'MESA', 'SMOOTH'), tile='CRYSTAL', air=1, clouds='FAR', crack='INK',
         fly='JELLY', crawl='BEETLE', plants=('EYE', 'TUBE', 'BULB'), wind=1, ads=("EAT GLORP", "ZORG-O-RAMA", "PROBE U")),
]


def fixed(t, n):
    b = t.encode('ascii')
    assert len(b) < n, t
    return b + bytes(n - len(b))


plan = bytearray()
for w in WORLDS:
    r = bytearray()
    for c in w['pal']:
        r += bytes(c)
    r += bytes([w['grav'], TH[w['theme']], WX[w['wx']], SKY[w['sky']]] + [STY[x] for x in w['style']] +
               [TL[w['tile']], w['air'], C[w['clouds']], C[w['crack']], FOOD[w['food']], C[w['shopc']], C[w['basec']],
                FLY[w['fly']], CRAWL[w['crawl']]] + [PL[x] for x in w['plants']] + [w['wind'], w['base'], w['coll'], w.get('worms', 0)] + [C[c] for sc in w['alien'] for c in sc])   # alien: the saucers' etc. colours, a scheme a sector
    r += fixed(w['name'], 12) + fixed(w['place'], 20) + fixed(w['shop'], 12)
    for a in w['ads']:
        r += fixed(a, 14)
    PLANET_LEN = len(r)
    plan += r

# the game's words (tools/messages.txt): a table of 2-byte offsets, then the texts, none across a block
MSGS = [l.rstrip('\n').split('\t', 1) for l in open(os.path.join(HERE, 'messages.txt')) if l.strip() and not l.startswith('#')]
msg = bytearray(2 * len(MSGS))
for k, (name, t) in enumerate(MSGS):
    b = t.encode('ascii') + bytes(1)
    if (len(msg) % 512) + len(b) > 512:
        msg += bytes(512 - len(msg) % 512)
    msg[2 * k:2 * k + 2] = struct.pack('<H', len(msg))
    msg += b
txt, help_off = bytearray(2 * len(HELP)), []                # a table of offsets (2 bytes each), then the lines
for page, x, y, c, t in HELP:
    rec = bytes([page, x, y, c]) + t.encode('ascii') + bytes(1)
    if (len(txt) % 512) + len(rec) > 512:
        txt += bytes(512 - len(txt) % 512)
    help_off.append(len(txt))
    txt[2 * (len(help_off) - 1):2 * len(help_off)] = struct.pack('<H', len(txt))
    txt += rec
# ---- each world's pack: copies of the pictures it draws, the ones drawn every frame first, so a frame reads few blocks.
# Its index (22 pictures: off, w, h, 4 bytes each) is loaded into RAM when the stage is built (Game.h: struct Pack).
import json
sys.path.insert(0, HERE)
import life
import skies
import structures
where = json.load(open(os.path.join(CARD, 'pics.json')))
pics = bytearray(open(os.path.join(CARD, 'pics.bin'), 'rb').read())
FLY_N = ['', 'BAT', 'JELLY', 'RAY']
CRAWL_N = ['', 'HOPPER', 'BEETLE', 'SNAIL']
PLANT_N = ['ROCK', 'SPIKES', 'BULB', 'FERN', 'FLOWER', 'TUBE', 'EYE', 'CACTUS', 'MUSH']
SKY_N = [n if f else None for n, f, x, y in skies.SKIES]
FOOD_N = ['BURGER', 'NOODLES', 'ICECREAM', 'PIZZA', 'HOTDOG', 'FISH', 'SUSHI', 'PIE']
import itertools


def put(names):
    """Copy pictures into the picture section from the current block on (none straddling a block); their index entries."""
    global pics
    ent = {}
    for n in names:
        o, pw, ph, ln = where[n]
        if ln <= 512 and (len(pics) % 512) + ln > 512:
            pics += bytes(512 - len(pics) % 512)
        assert len(pics) < 65536
        ent[n] = struct.pack('<HBB', len(pics), pw, ph)
        pics += pics[o:o + ln]
    return ent


def blocks(names):                                                # blocks a group takes, packed largest first
    used = []
    for ln in sorted((where[n][3] for n in names), reverse=True):
        for k in range(len(used)):
            if used[k] + ln <= 512:
                used[k] += ln
                break
        else:
            used.append(ln)
    return used


# one index per stage (Pack, 23 pictures): the two stages of a world share its pictures, but each draws three of its six
# buildings, packed into as few blocks as they go (a frame then reads about seven blocks: the cache holds seven)
idx = [None] * 16
BOSSES = ['MOTHERSHIP', 'BOSS_HIVE', 'BOSS_PRISM', 'BOSS_DREAD']   # stages 4, 8, 12 and 16
for wi, w in enumerate(WORLDS):
    names = ['SKY_' + SKY_N[SKY[w['sky']]] if SKY_N[SKY[w['sky']]] else None, 'GI_' + life.GIANT_OF[wi]]
    names += ['PL_%s%d' % (pn, f) for pn in w['plants'] for f in (0, 1)]
    names += ['CR_%s%d' % (FLY_N[FLY[w['fly']]], f) if FLY[w['fly']] else None for f in (0, 1)]
    names += ['CR_%s%d' % (CRAWL_N[CRAWL[w['crawl']]], f) if CRAWL[w['crawl']] else None for f in (0, 1)]
    st = structures.THEMES[TH[w['theme']]] if TH[w['theme']] else []
    rest = ['BURGER', 'FOOD%d' % FOOD[w['food']], ['BASE', 'BASE1', 'BASE2', 'BASE3'][w['base']], ['BUCKET', 'COLL1', 'COLL2', 'COLL3'][w['coll']]]
    pics += bytes(-len(pics) % 512)                               # each pack starts on a block
    boss = [BOSSES[((sec * 8 + wi) >> 2) & 3] for sec in (0, 1)]   # a boss stage's own boss (stage & 3 == 3)
    ent = put([n for n in names if n] + sorted(set(boss)))          # (the boss: in the critters' block, which has room)
    groups = [[], []]
    if st:                                                        # the split of the six with the fewest blocks a stage
        best = min((c for c in itertools.combinations(st, 3) if st[0] in c),
                   key=lambda c: (max(len(blocks(c)), len(blocks([n for n in st if n not in c]))), len(blocks(c)) + len(blocks([n for n in st if n not in c]))))
        groups = [list(best), [n for n in st if n not in best]]
        for gr in groups:
            pics += bytes(-len(pics) % 512)
            gr.sort(key=lambda n: -where[n][3])
            order = []                                            # largest first, each block filled up with the next that fit
            left = list(gr)
            while left:
                room, blk = 512, []
                for n in list(left):
                    if where[n][3] <= room:
                        blk.append(n); room -= where[n][3]; left.remove(n)
                order += blk
                ent.update(put(blk))
                pics += bytes(-len(pics) % 512) if left else b''
    ent.update(put(rest))
    for sec in (0, 1):
        r = bytearray()
        for n in names:
            r += ent[n] if n else bytes(4)
        for k in range(6):
            r += ent[groups[sec][k % 3]] if st else bytes(4)
        for n in rest + [boss[sec]]:
            r += ent[n]
        assert len(r) == 92
        idx[sec * 8 + wi] = r
index = bytearray(b''.join(idx))
pics = padded(bytes(pics))
songs = padded(open(os.path.join(CARD, 'songs.bin'), 'rb').read())
txt = padded(bytes(txt))
plan = padded(bytes(plan))
index = padded(bytes(index))
msg = padded(bytes(msg))
pic_blk = 1
song_blk = pic_blk + len(pics) // 512
txt_blk = song_blk + len(songs) // 512
plan_blk = txt_blk + len(txt) // 512
msg_blk = plan_blk + len(plan) // 512
pack_blk = msg_blk + len(msg) // 512
body = pics + songs + txt + plan + msg + index
h = zlib.crc32(body) & 0xffffffff
head = padded(b'STARLIFT' + struct.pack('<IHH', h, pic_blk, song_blk))
open(os.path.join(CARD, 'STARLIFT.DAT'), 'wb').write(head + body)
open(os.path.join(ROOT, 'CHStarlift', 'CardIndex.h'), 'w', newline='').write(
    "// Generated by tools/make_card.py: STARLIFT.DAT's hash and where its sections start (byte offsets).\n"
    "#pragma once\n#include <stdint.h>\n\n"
    "static const uint32_t CARD_HASH = 0x%08Xu;\n"
    "static const uint32_t CARD_PIC = %d, CARD_SONG = %d, CARD_TXT = %d;\n"
    "// the how-to-play lines: page, x, y, colour, then the text (ending in 0)\n"
    "static const uint32_t CARD_PACK = %d;   // each stage's pack index (stage & 15): 92 bytes\n"
    "static const uint32_t CARD_MSG = %d;\n"
    "enum : uint8_t { %s };\n"
    "static const uint32_t CARD_PLANET = %d;\n#define PLANET_LEN %d\n"
    "#define HELP_LINES %d\n"
    "// (the line offsets are the first 2 * HELP_LINES bytes of CARD_TXT)\n%s" % (h, pic_blk * 512, song_blk * 512, txt_blk * 512, pack_blk * 512, msg_blk * 512, ', '.join(n for n, t in MSGS), plan_blk * 512, PLANET_LEN, len(HELP), ""))
print("STARLIFT.DAT: %d bytes (hash %08X)" % (len(head) + len(body), h))
