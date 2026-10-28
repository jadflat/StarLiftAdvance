"""STARLIFT's sprites, drawn as text grids in the CHGame HOUSE palette (the pixel-art-gen skill's way) and written as
  CHStarlift/Sprites.h   run-length rows (a byte: run length - 1 in the high nibble, the HOUSE index low, 15 = clear)
  out/sprites/*.json     the pixel-art-gen skill's JSON for each sprite, rendered to out/sprites/*.png by its script
  out/sprites/sheet.png  a contact sheet at 4x

The lander is drawn once, upright, and turned to 15, 30 ... 90 degrees here (a rotation sampled 4x4 a pixel, each pixel
taking the colour most of its samples hit, then outlined again); the game mirrors these for the other side.

Run: python tools/make_sprites.py   (then check: cd <CHGame>/tools && python -m artkit.craft out/sprites/*.png)
The game maps a HOUSE index onto its own palette when it blits (blit() in CHStarlift.ino).
"""
import json
import math
import os
import subprocess
import sys

HOUSE = ['#000000', '#FFFFFF', '#146b34', '#2fb34a', '#7be26a', '#BBBBCC', '#EE1122', '#770022',
         '#FFCC22', '#774411', '#2266EE', '#112255', '#FFBB88', '#66EEFF']
T = 15
# grid letters -> HOUSE index
KEY = {'.': T, 'k': 0, 'w': 1, 'G': 2, 'g': 3, 'l': 4, 's': 5, 'r': 6, 'm': 7, 'y': 8, 'b': 9, 'u': 10, 'n': 11, 'o': 12, 'c': 13}
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OUT = os.path.join(ROOT, 'out', 'sprites')
# the grid-to-PNG renderer of the pixel-art-gen skill; PIXEL_ART_RENDER overrides its place
RENDER = os.environ.get('PIXEL_ART_RENDER') or os.path.join(os.path.expanduser('~'), '.claude', 'skills', 'pixel-art-gen', 'scripts', 'render_pixel_art.py')


def grid(rows):
    w = max(len(r) for r in rows)
    return [[KEY[ch] for ch in r.ljust(w, '.')] for r in rows]


def mirror(half):
    """Rows given as the left half plus the centre column; the right half is the left reversed."""
    return grid([r + r[-2::-1] for r in half])


def outline(p):
    h, w = len(p), len(p[0])
    q = [row[:] for row in p]
    for y in range(h):
        for x in range(w):
            if p[y][x] == T and any(0 <= x + dx < w and 0 <= y + dy < h and p[y + dy][x + dx] not in (T, 0)
                                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                q[y][x] = 0
    return q


def rotate(p, deg, size):
    """Turn a grid clockwise by deg about its centre into a size x size grid: 4x4 samples a pixel, majority colour."""
    h, w = len(p), len(p[0])
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    out = [[T] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            votes = {}
            for sy in range(4):
                for sx in range(4):
                    dx = x + (sx + 0.5) / 4 - size / 2
                    dy = y + (sy + 0.5) / 4 - size / 2
                    u = ca * dx + sa * dy + w / 2          # inverse rotation into the source
                    v = -sa * dx + ca * dy + h / 2
                    iu, iv = math.floor(u), math.floor(v)
                    c = p[iv][iu] if 0 <= iu < w and 0 <= iv < h else T
                    votes[c] = votes.get(c, 0) + 1
            filled = 16 - votes.get(T, 0)
            if filled >= 7:
                inner = {c: n for c, n in votes.items() if c not in (T, 0)}
                if inner and sum(inner.values()) >= 5:
                    out[y][x] = max(inner, key=inner.get)
                else:
                    out[y][x] = 0
    return outline([[c if c != 0 else T for c in row] for row in out])


SPR = {}


def cen(rows, w=None):
    """Centre each row in a common width (odd rows in an odd width stay symmetric)."""
    w = w or max(len(r) for r in rows)
    out = []
    for r in rows:
        pad = w - len(r)
        out.append('.' * (pad // 2) + r + '.' * (pad - pad // 2))
    return grid(out)


# ---- the lander: an upright module, lit from the top left. A silver cabin with a dark window and a glint, a red band,
# a gold-foil descent stage with crinkles, a nozzle bell and splayed legs with pads.
LANDER = grid([
    '........k........',
    '........k........',
    '.....kkkkkkk.....',
    '....kwwsssssk....',
    '...kwsskkkksnk...',
    '...kwskwnkksnk...',
    '...kssskkkksnk...',
    '...ksssssssnnk...',
    '..kkrrrrrrrrrkk..',
    '.kwyyyyyyyyyyrnk.',
    '.kyyynyyyyynyrnk.',
    '.kyyyyyynyyyyrnk.',
    '.kyynyyyyyyyrrnk.',
    '..knnnnnnnnnnnk..',
    '..kn..ksnnk..nk..',
    '.kn..ksnnnnk..nk.',
    'kn...kkkkkkk...nk',
    'kkk...........kkk',
])
LANDER_UP = LANDER[:14] + grid([                       # the gear stowed: the legs folded up against the descent stage
    '..kkk.ksnnk.kkk..',
    '.....ksnnnnk.....',
    '.....kkkkkkk.....',
    '.................',
])
SIZE = 23
for i in range(7):
    SPR['LAND%d' % i] = rotate(LANDER, i * 15, SIZE)
for i in range(7):
    SPR['LANDUP%d' % i] = rotate(LANDER_UP, i * 15, SIZE)

# ---- a colonist in a white suit with a gold visor (facing right; the game mirrors)
BODY = ['.kwwwssk.', 'kswwwssnk', 'ksksssknk', '.kssssnk.']
LEGS = ['.kssknnk.', '.ksk.knk.', '.kkk.kkk.']
HEAD = ['...kkk...', '..kwwsk..', '.kwwyyyk.', '.kwsyyrk.', '..kssnk..']
SPR['MAN_STAND'] = grid(HEAD + BODY + LEGS)
SPR['MAN_WAVE1'] = grid(['...kkk.kk', '..kwwskwk', '.kwwyyysk', '.kwsyyrsk', '..kssnsk.',
                         '.kwwwssk.', 'kswwwsnk.', 'ksksssnk.', '.kssssnk.'] + LEGS)
SPR['MAN_WAVE2'] = grid(['...kkk...', '..kwwsk..', '.kwwyyykk', '.kwsyyrsk', '..kssnsk.',
                         '.kwwwssk.', 'kswwwsnk.', 'ksksssnk.', '.kssssnk.'] + LEGS)
SPR['MAN_RUN1'] = grid(HEAD + BODY + ['kssk.knk.', 'ksk...knk', 'kk.....kk'])
SPR['MAN_RUN2'] = grid(HEAD + BODY + ['..ksnnk..', '..ksnk...', '..kkkk...'])
SPR['MAN_LOOKUP'] = grid(['...kkk...', '..kwyyk..', '.kwsyyrk.', '.kwwsssk.', '..kssnk..'] + BODY + LEGS)
SPR['MAN_TALK'] = grid(HEAD + ['.kwwwssk.', 'kswwwsskk', 'ksksssnsk', '.kssssnk.'] + LEGS)
SPR['MAN_CHEER'] = grid(['kk.kkk.kk', 'wkkwwskwk', 'swkwyyykw', '.skwyyrks', '..kssnks.', '.kwwwssk.', '.kwwwsnk.', '..ksssnk.', '.kssssnk.'] + LEGS)
for i in range(8):                                    # tumbling up a tractor beam: turned in 45 degree steps
    SPR['MAN_SPIN%d' % i] = rotate(SPR['MAN_STAND'], i * 45, 15)

# ---- a flying saucer: a glowing dome (the planet's accent colour) on a shaded silver disc, a ring of running lights
RIM = 'yrwg'                                          # the running lights' colours, in turn round the rim
for f in range(4):                                     # four frames: the lights chase round, so the saucer seems to spin
    ring = ''.join(('wsss'[((i - f) % 4)] if (i - f) % 4 else RIM[(i // 4 + f) % 4]) for i in range(21))
    ring = ''.join('s' if c in 'ws' and c != 'w' else c for c in ring)
    SPR['SAUCER%d' % f] = cen([
        'kkkkkkk',
        'kgwwggggk',
        'kgwggggggnk',
        'kkkkkkkkkkkkkkk',
        'kwwssssssssssssssnk',
        'kwsssssssssssssssssnk',
        'k' + ring + 'k',
        'knnnnnnnnnnnnnnnnnnnk',
        'kknnnnnnnnnnnnnkk',
        'kkkkkkkkkkk',
    ], 23)

# ---- a snatcher: a red brain-jelly with one gold eye and trailing tentacles (two frames)
SN_TOP = [
    'kkkkk',
    'kkrwwrrkk',
    'krwwrrrrrnk',
    'krwrrkkkrrnk',
    'krrrkyywkrrnk',
    'krrrkyyykrrnk',
    'krrrrkkkrrnnk',
    'kkrrrrrrnnkk',
]
SPR['SNATCH0'] = cen(SN_TOP + ['krkrkrknk', 'krkrkrknk', 'kr.krk.nk', 'k..k.k..k'], 15)
SPR['SNATCH1'] = cen(SN_TOP + ['krkrkrknk', 'krk.krk.nk', 'kr..kr..kr', '.k...k...k'], 15)

# ---- a space mine: a spiked red ball (two frames: the core flashes)
for f, core in enumerate(('y', 'w')):
    SPR['MINE%d' % f] = grid([
        'k...k...k',
        '.kkkrkkk.',
        '.krwwrrk.',
        '.kwwrrrnk',
        'krrr' + core + 'rrnk',
        '.krrrrnnk',
        '.kknrnnk.',
        '.k.krk.k.',
        'k...k...k',
    ])

# ---- a ground turret: a shaded steel dome on a striped base (the barrel is drawn by the game)
SPR['TURRET'] = grid([
    '....kkkkk....',
    '...kwssssk...',
    '..kwsssssnk..',
    '..ksssssnnk..',
    '.kkkkkkkkkkk.',
    '.knyknyknynk.',
    'knnnnnnnnnnnk',
    'kkkkkkkkkkkkk',
])


def base():
    """The base: a shaded hangar dome with ribs and a lit door, a control tower with a beacon, a radar dish."""
    W, H = 44, 30
    p = [[T] * W for _ in range(H)]
    cx, cy, rx, ry = 18, 29, 16, 15
    bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    for y in range(H):
        for x in range(W):
            nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            d = nx * nx + ny * ny
            if d <= 1 and y < cy:
                nz = max(0.0, 1 - d) ** 0.5
                lum = (-0.55 * nx - 0.6 * ny + 0.6 * nz)          # light from the top left
                v = lum * 16 + bayer[y & 3][x & 3] / 2
                p[y][x] = 1 if v > 13 else 5 if v > 6 else 11
    for x in range(cx - rx, cx + rx + 1, 6):                       # ribs
        for y in range(H):
            if p[y][x] in (1, 5, 11) and (x - cx) != 0:
                p[y][x] = 11 if p[y][x] != 11 else 0
    for y in range(20, 29):                                       # the lit door
        for x in range(13, 24):
            p[y][x] = 0 if x in (13, 23) or y == 20 else 8 if y > 21 else 6
    for y in range(4, 29):                                        # the control tower
        for x in range(36, 42):
            p[y][x] = 1 if x == 37 else 5 if x < 40 else 11
    for y in range(7, 10):
        for x in range(36, 42):
            p[y][x] = 13 if 37 <= x <= 40 and y == 8 else 0
    for y in (14, 20):
        for x in range(37, 41):
            p[y][x] = 8
    for y in range(0, 4):
        p[y][38] = 0
    p[0][38] = 6
    for (x, y) in ((28, 6), (29, 5), (30, 4), (31, 4), (32, 5), (29, 7), (30, 7), (31, 6), (32, 6), (33, 6)):    # radar dish
        p[y][x] = 5
    for y in range(7, 12):
        p[y][31] = 11
    for x in range(W):
        p[29][x] = 0
    return outline(p)


SPR['BASE'] = base()


def canvas(w, h):
    return [[T] * w for _ in range(h)]


def rect(p, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            if 0 <= yy < len(p) and 0 <= xx < len(p[0]):
                p[yy][xx] = c


def burger_bar():
    """A 1950s drive-through takeaway: a boomerang roof, a sign board and a pole (the game lights the letters and puts the food on top)."""
    W_, H_ = 58, 38
    p = canvas(W_, H_)
    rect(p, 0, 2, 46, 11, 1); rect(p, 1, 3, 44, 9, 6)                 # the sign board: white rim, red face
    rect(p, 10, 13, 2, 6, 11); rect(p, 34, 13, 2, 6, 11)              # its posts
    for x in range(48):                                                # the roof, rising to the right
        y = 19 - x * 5 // 47
        rect(p, x, y, 1, 1, 1); rect(p, x, y + 1, 1, 2, 6); rect(p, x, y + 3, 1, 1, 0)
    for x in range(2, 45):                                             # the walls under it
        top = 19 - x * 5 // 47 + 4
        rect(p, x, top, 1, H_ - 1 - top, 5)
    rect(p, 2, 16, 1, 21, 1); rect(p, 44, 15, 1, 22, 11)
    rect(p, 2, 23, 43, 1, 6)                                           # a red stripe
    rect(p, 5, 25, 18, 9, 0); rect(p, 6, 26, 16, 7, 8)                 # the big window, lit gold inside
    for k in range(4):
        p[26 + k][8 + k] = 1; p[26 + k][9 + k] = 1                     # a glare on the glass
    rect(p, 12, 26, 1, 7, 9)
    rect(p, 30, 22, 11, 2, 1)                                          # the drive-through awning, striped
    for x in range(30, 41, 2):
        p[22][x] = 6; p[23][x + 1] = 6
    rect(p, 31, 25, 9, 7, 0); rect(p, 32, 26, 7, 5, 8)                 # the drive-through window
    rect(p, 24, 28, 5, 9, 11); rect(p, 24, 28, 5, 1, 0); p[32][27] = 8  # the door
    rect(p, 0, H_ - 1, 46, 1, 0)
    rect(p, 48, 9, 2, H_ - 9, 11); rect(p, 48, 9, 1, H_ - 9, 5)       # the pole
    return outline(p)


def bucket():
    """The aliens' collecting bucket on its legs; the tubes under it run into the ground (the game draws the rest)."""
    W_, H_ = 40, 34
    p = canvas(W_, H_)
    for i, x in enumerate((8, 31)):                                    # legs, splayed
        for y in range(18, H_):
            xx = x + ((y - 18) // 3) * (-1 if i == 0 else 1)
            rect(p, xx, y, 2, 1, 11)
    for y in range(4, 21):                                             # the bucket: a tapering drum, lit from the left
        hw = 15 - (y - 4) * 5 // 16
        x0, x1 = 20 - hw, 20 + hw
        for x in range(x0, x1):
            c = 1 if x < x0 + 2 else 11 if x >= x1 - 3 else 5
            if y in (8, 16):
                c = 6 if c == 5 else 7 if c == 11 else 6
            p[y][x] = c
    rect(p, 3, 2, 34, 3, 5); rect(p, 3, 2, 34, 1, 1); rect(p, 4, 3, 32, 1, 3)   # the rim, and the green slime inside
    for (x, y) in ((17, 11), (18, 10), (19, 10), (20, 10), (21, 10), (22, 11), (17, 13), (22, 13), (18, 14), (19, 14), (20, 14), (21, 14)):
        p[y][x] = 3                                                    # an alien eye on its side
    rect(p, 18, 11, 4, 3, 4); rect(p, 19, 12, 2, 1, 0)
    for x in (15, 19, 23):                                             # the tubes down into the ground
        rect(p, x, 21, 2, H_ - 21, 11)
        for y in range(23, H_, 4):
            rect(p, x, y, 2, 1, 2)
    return outline(p)


CARD = {}                                             # the pictures read from STARLIFT.DAT
# sprites drawn from the card too (each group fits a block, so a frame that draws one of them reads the block once)
# the pictures drawn nearly every frame share three blocks (the card cache holds six): saucers, stingers and the turret;
# the upright lander; the common colonist poses and the mines. The rarer ones follow.
HOT = (['SAUCER%d' % i for i in range(4)] + ['SNATCH0', 'SNATCH1', 'TURRET'] + ['LAND0', 'LAND1', 'LAND2'] +
       ['MAN_STAND', 'MAN_WAVE1', 'MAN_RUN1', 'MAN_RUN2', 'MAN_CHEER', 'MINE0', 'MINE1'])
for n in HOT + ['MAN_LOOKUP', 'MAN_TALK', 'MAN_WAVE2'] + ['LANDUP%d' % i for i in range(7)] + ['MAN_SPIN%d' % i for i in range(8)] + ['LAND3', 'LAND4', 'LAND5', 'LAND6']:
    CARD[n] = SPR.pop(n)
CARD['BASE'] = SPR.pop('BASE')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import structures                                     # the background buildings (tools/structures.py), one theme after another
import plants as plantdesigns                         # the alien plants (tools/plants.py), two frames each
for n in plantdesigns.ORDER:
    fa, fb = plantdesigns.frames(plantdesigns.P[n])
    CARD['PL_%s0' % n] = grid(fa); CARD['PL_%s1' % n] = grid(fb if n != 'ROCK' else fa)
import skies                                          # the planets and suns in the skies (tools/skies.py)
for n, f, x, y in skies.SKIES:
    if f:
        CARD['SKY_' + n] = f()
import life                                           # critters, the sky giants and the logos (tools/life.py)
for n in life.FLY_ORDER + life.CRAWL_ORDER:
    CARD['CR_%s0' % n] = life.g(life.CRITTERS[n][0]); CARD['CR_%s1' % n] = life.g(life.CRITTERS[n][1])
for k, f in enumerate(life.FOODS):
    CARD['FOOD%d' % k] = life.g(f)
CARD['DESCENT'] = life.g(life.DESCENT)
CARD['GANTRY'] = life.g(life.GANTRY)
CARD['PUMP'] = life.g(life.PUMP)
import bosses                                         # the four bosses (tools/bosses.py, redrawn in 0.20)
CARD['MOTHERSHIP'] = life.g(bosses.MOTHERSHIP)
for _b, _d in (('BOSS_HIVE', bosses.HIVE), ('BOSS_PRISM', bosses.PRISM), ('BOSS_DREAD', bosses.DREAD)):
    CARD[_b] = life.g(_d)
CARD['SCOOTER'] = life.g(life.SCOOTER)
CARD['COW'] = life.g(life.COW)
CARD['COWEAT'] = life.g(life.COWEAT)
CARD['COWUP'] = life.g(life.COWUP)
for n in life.GIANT_ORDER:
    CARD['GI_' + n] = life.g(life.GIANTS[n])
for n, gr in life.LOGOS.items():
    CARD['LOGO_' + n] = gr
import bases as basedesigns                           # the other player bases and alien collectors (tools/bases.py)
BASE_DOOR, COLL_TOP, COLL_TUBE = [18], [31], [(15, 19, 23)]   # the dome's door; the bucket's mouth (from the ground) and tubes
for k, f in enumerate(basedesigns.BASES):
    g, door = f()
    CARD['BASE%d' % (k + 1)] = outline(g)
    BASE_DOOR.append(door)
for k, f in enumerate(basedesigns.COLLECTORS):
    g, top, tubes = f()
    CARD['COLL%d' % (k + 1)] = outline(g)
    COLL_TOP.append(top); COLL_TUBE.append(tubes)
for th in (1, 2, 3):
    for n in structures.THEMES[th]:
        CARD[n] = structures.PICS[n][0]
CARD['BURGER'] = burger_bar()
CARD['BUCKET'] = bucket()
def pack(p):
    out = []
    for row in p:
        x = 0
        while x < len(row):
            v, n = row[x], 1
            while x + n < len(row) and row[x + n] == v and n < 16:
                n += 1
            out.append(((n - 1) << 4) | v)
            x += n
    return out


def render(name, p):
    """The pixel-art-gen skill's JSON for a sprite, rendered to a PNG by its script."""
    h, w = len(p), len(p[0])
    js = {"width": w, "height": h, "background": "transparent", "grid_lines": False, "pixel_size": 1,
          "pixels": [{"x": x, "y": y, "color": HOUSE[p[y][x]]} for y in range(h) for x in range(w) if p[y][x] != T]}
    jp = os.path.join(OUT, name + '.json')
    with open(jp, 'w', newline='') as f:
        json.dump(js, f)
    subprocess.run([sys.executable, RENDER, jp, '-o', os.path.join(OUT, name + '.png'), '-p', '1'], check=True, stdout=subprocess.DEVNULL)


def main():
    os.makedirs(OUT, exist_ok=True)
    lines = ["// Generated by tools/make_sprites.py: the game's sprites as packed HOUSE indices (see the generator).",
             "#pragma once", "#include <stdint.h>", "",
             "struct Spr { const uint8_t *d; uint8_t w, h; };   // run-length rows: (length - 1) << 4 | HOUSE index, 15 = clear",
             "// constants of 8 bytes or less are 'small data', which this core copies into RAM: a section of their own keeps them in flash",
             "#if defined(__riscv) && !defined(CHSIM)",
             "#define ROM(n) __attribute__((section(\".rodata.spr.\" #n)))",
             "#else",
             "#define ROM(n)",
             "#endif", ""]
    total = 0
    SPR_ALL = dict(SPR)
    for name, p in SPR.items():
        data = pack(p)
        total += len(data)
        h, w = len(p), len(p[0])
        lines.append("static const uint8_t D_%s[%d] = {%s};" % (name, len(data), ",".join(str(v) for v in data)))
        lines.append("static const Spr S_%s ROM(S_%s) = { D_%s, %d, %d };" % (name, name, name, w, h))
        render(name, p)
    lines.append("")
    lines.append("struct CSpr { uint16_t off; uint8_t w, h; };   // a picture in STARLIFT.DAT's picture section (CardIndex.h: CARD_PIC)")
    sec = bytearray()
    where = {}
    for name, p in CARD.items():
        data = pack(p)
        assert len(data) <= (1536 if name.startswith('LOGO') else 1024), name   # at most two blocks (the title pictures three: drawn alone)
        if len(data) <= 512 and (len(sec) % 512) + len(data) > 512:
            sec += bytes(512 - len(sec) % 512)
        lines.append("static const CSpr C_%s ROM(C_%s) = { %d, %d, %d };   // %d bytes" % (name, name, len(sec), len(p[0]), len(p), len(data)))
        where[name] = (len(sec), len(p[0]), len(p), len(data))
        sec += bytes(data)
        SPR_ALL[name] = p
        render(name, p)
    card = os.path.join(ROOT, 'out', 'card')
    os.makedirs(card, exist_ok=True)
    open(os.path.join(card, 'pics.bin'), 'wb').write(bytes(sec))
    json.dump(where, open(os.path.join(card, 'pics.json'), 'w'))      # make_card.py builds each world's pack from these
    lines.append("static const CSpr *const STRUCT[3][6] = { %s };   // the background buildings of the ruins, the bases, the aliens" % ", ".join(
        "{ %s }" % ", ".join("&C_" + n for n in structures.THEMES[th]) for th in (1, 2, 3)))
    lines.append("static const int8_t STRUCT_LIGHT[3][6][3] = { %s };   // a light on each: x, y, kind (1 beacon, 2 rising glow, 3 arc, 4 eyes)" % ", ".join(
        "{ %s }" % ", ".join("{ %s }" % ", ".join(map(str, (structures.PICS[n][1] or [(0, 0, 0)])[0])) for n in structures.THEMES[th]) for th in (1, 2, 3)))
    lines.append("static const CSpr *const BASEPIC[4] = { &C_BASE, &C_BASE1, &C_BASE2, &C_BASE3 };")
    lines.append("static const uint8_t BASE_DOOR[4] = { %s };   // the door's x in each" % ", ".join(map(str, BASE_DOOR)))
    lines.append("static const CSpr *const COLLPIC[4] = { &C_BUCKET, &C_COLL1, &C_COLL2, &C_COLL3 };")
    lines.append("static const uint8_t COLL_TOP[4] = { %s };    // where colonists drop in, above the ground" % ", ".join(map(str, COLL_TOP)))
    lines.append("static const uint8_t COLL_TUBE[4][3] = { %s };" % ", ".join("{ %s }" % ", ".join(map(str, t)) for t in COLL_TUBE))
    lines.append("static const CSpr *const PLANTPIC[9][2] = { %s };" % ", ".join("{ &C_PL_%s0, &C_PL_%s1 }" % (n, n) for n in plantdesigns.ORDER))
    lines.append("static const CSpr *const SKYPIC[8] = { %s };   // by SkyObj; the aurora is drawn by code" % ", ".join("&C_SKY_" + n if f else "nullptr" for n, f, x, y in skies.SKIES))
    lines.append("static const uint8_t SKY_AT[8][2] = { %s };   // where each one's centre sits on screen" % ", ".join("{ %d, %d }" % (x, y) for n, f, x, y in skies.SKIES))
    lines.append("static const CSpr *const CRITPIC[2][3][2] = { %s };   // fliers, crawlers; two frames each" % ", ".join(
        "{ %s }" % ", ".join("{ &C_CR_%s0, &C_CR_%s1 }" % (n, n) for n in order) for order in (life.FLY_ORDER, life.CRAWL_ORDER)))
    lines.append("static const CSpr *const GIANTPIC[8] = { %s };   // each world's sky giant" % ", ".join("&C_GI_" + n for n in life.GIANT_OF))
    lines.append("static const CSpr *const FOODPIC[8] = { %s };" % ", ".join("&C_FOOD%d" % k for k in range(8)))
    lines.append("static const CSpr *const SPIN[8] = { %s };" % ", ".join("&C_MAN_SPIN%d" % i for i in range(8)))
    lines.append("static const CSpr *const LANDER_UP[7] = { %s };" % ", ".join("&C_LANDUP%d" % i for i in range(7)))
    lines.append("static const CSpr *const SAUCER[4] = { %s };" % ", ".join("&C_SAUCER%d" % i for i in range(4)))
    lines.append("static const CSpr *const LANDER[3] = { %s };   // up to 30 degrees" % ", ".join("&C_LAND%d" % i for i in range(3)))
    lines.append("static const CSpr *const LANDER_STEEP[4] = { &C_LAND3, &C_LAND4, &C_LAND5, &C_LAND6 };   // 45 to 90 degrees, from the card")
    open(os.path.join(ROOT, 'CHStarlift', 'Sprites.h'), 'w', newline='').write("\n".join(lines) + "\n")
    print("Sprites.h: %d sprites, %d bytes; card pictures %d bytes" % (len(SPR), total, len(sec)))
    from PIL import Image
    sc, pad, x, y, rowh, wmax = 6, 8, 8, 8, 0, 960
    pos = []
    for n, p in SPR_ALL.items():
        w, h = len(p[0]) * sc, len(p) * sc
        if x + w + pad > wmax:
            x, y, rowh = pad, y + rowh + pad, 0
        pos.append((n, x, y))
        x += w + pad
        rowh = max(rowh, h)
    sheet = Image.new('RGBA', (wmax, y + rowh + pad), (70, 40, 110, 255))
    for n, x, y in pos:
        im = Image.open(os.path.join(OUT, n + '.png')).convert('RGBA')
        im = im.resize((im.width * sc, im.height * sc), 0)
        sheet.paste(im, (x, y), im)
    sheet.save(os.path.join(OUT, 'sheet.png'))


main()
