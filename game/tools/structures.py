"""STARLIFT's background structures: ruins, base buildings and alien towers, drawn here as pictures for the card.

Their colours follow the parallax layer they stand on: 'l' lit, 'b' body, 'd' dark (the game maps these HOUSE greens onto
the layer's colours), plus fixed accents: 'y' gold, 'r' red, 'c' the world's accent, 'w' white, 'k' ink.
Each picture comes with up to two light points (x, y, kind) the game animates: 1 a blinking beacon, 2 a glow that climbs
the picture, 3 an energy arc to the right.
"""
import math

T = 15
K, W_, D, B, L, S, R, Y, C = 0, 1, 2, 3, 4, 5, 6, 8, 13


def cv(w, h):
    return [[T] * w for _ in range(h)]


def put(p, x, y, c):
    if 0 <= y < len(p) and 0 <= x < len(p[0]):
        p[y][x] = c


def rect(p, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            put(p, xx, yy, c)


def block(p, x, y, w, h):
    """A block lit from the left: a lit left edge and top, a dark right edge."""
    rect(p, x, y, w, h, B)
    rect(p, x, y, 1, h, L)
    rect(p, x, y, w, 1, L)
    if w > 2:
        rect(p, x + w - 1, y + 1, 1, h - 1, D)


def column(p, x, base, h, broken=0):
    block(p, x - 1, base - 2, 7, 2)                         # the plinth
    top = base - h
    if broken:
        cut = [0, 3, 1, 5, 2]
        for k in range(5):
            rect(p, x + k, top + cut[k], 1, h - cut[k] - 2, L if k == 0 else D if k == 4 else B)
    else:
        block(p, x, top, 5, h - 2)
        block(p, x - 1, top - 3, 7, 3)                      # the capital
        put(p, x, top - 2, L)
    for y in range(top + (6 if broken else 1), base - 3):   # fluting
        put(p, x + 2, y, D)


def outline_dark(p):
    """A thin ink edge where the picture meets the air below or beside it (none on top: the sky shows through)."""
    h, w = len(p), len(p[0])
    q = [r[:] for r in p]
    for y in range(h):
        for x in range(w):
            if p[y][x] == T:
                for dx, dy in ((1, 0), (-1, 0), (0, 1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h and p[yy][xx] not in (T, K) and p[yy][xx] == D:
                        q[y][x] = K
                        break
    return q


PICS = {}      # name -> (grid, lights)


# ---- ruins
def ruins():
    p = cv(9, 38); column(p, 2, 37, 32); PICS['R_COLUMN'] = (p, [])
    p = cv(22, 32); column(p, 2, 31, 22, 1); column(p, 14, 31, 28, 0); rect(p, 7, 29, 6, 2, D); put(p, 8, 28, B); put(p, 10, 28, L)
    PICS['R_PAIR'] = (p, [])
    p = cv(30, 42)                                           # an arch with a keystone and carvings
    column(p, 2, 41, 26); column(p, 22, 41, 26)
    for k in range(25):
        a = math.pi * k / 24
        y = 14 - int(round(math.sin(a) * 9))
        x = 3 + k
        rect(p, x, y, 1, 4, L if k < 6 else D if k > 18 else B)
        put(p, x, y + 4, D)
    block(p, 13, 3, 5, 4); put(p, 15, 4, Y)
    rect(p, 0, 14, 30, 1, B); rect(p, 0, 15, 30, 1, D)
    PICS['R_ARCH'] = (p, [])
    p = cv(42, 32)                                           # a stepped temple: tiers, a stair, a dark shrine door
    for k in range(4):
        block(p, k * 4, 31 - 6 - k * 6, 42 - k * 8, 6)
        for x in range(k * 4 + 2, 42 - k * 4 - 2, 4):
            put(p, x, 31 - 3 - k * 6, D)
    rect(p, 18, 8, 6, 24, D)
    for y in range(8, 31, 2):
        rect(p, 18, y, 6, 1, B)
    block(p, 16, 0, 10, 8); rect(p, 19, 3, 4, 5, K); put(p, 20, 2, Y); put(p, 21, 2, Y)
    PICS['R_TEMPLE'] = (p, [(21, 1, 1)])
    p = cv(18, 26)                                           # a great stone head, half sunk
    rect(p, 2, 4, 14, 22, B); rect(p, 2, 4, 1, 22, L); rect(p, 2, 4, 14, 1, L); rect(p, 15, 5, 1, 21, D)
    rect(p, 1, 8, 16, 3, D); rect(p, 1, 8, 16, 1, L)            # the brow
    rect(p, 4, 11, 4, 2, K); rect(p, 10, 11, 4, 2, K); put(p, 5, 11, Y); put(p, 11, 11, Y)
    rect(p, 8, 11, 2, 7, L); rect(p, 9, 13, 2, 5, D)         # the nose
    rect(p, 5, 20, 8, 1, D); rect(p, 6, 21, 6, 1, K)
    rect(p, 3, 0, 12, 4, B); rect(p, 3, 0, 12, 1, L); rect(p, 0, 24, 18, 2, D)
    PICS['R_HEAD'] = (p, [(5, 11, 4)])
    p = cv(8, 36)                                            # an obelisk with carvings
    for k in range(34):
        w = 6 - k * 3 // 34
        x0 = 1 + (6 - w) // 2
        rect(p, x0, 35 - k, w, 1, B); put(p, x0, 35 - k, L); put(p, x0 + w - 1, 35 - k, D)
    for y in range(6, 32, 4):
        put(p, 4, y, D); put(p, 3, y + 1, D)
    put(p, 3, 0, L); put(p, 4, 0, L); put(p, 3, 1, Y)
    PICS['R_OBELISK'] = (p, [])


# ---- base buildings
def bases():
    p = cv(28, 16)                                           # a habitat dome with windows and an airlock
    for dy in range(14):
        hw = int(math.sqrt(max(0, 13 * 13 - dy * dy)) * 1.0)
        y = 15 - dy
        rect(p, 14 - hw, y, 2 * hw, 1, B)
        rect(p, 14 - hw, y, max(1, hw // 2), 1, L)
        rect(p, 14 + hw - hw // 3, y, hw // 3, 1, D)
    for x in range(4, 25, 4):
        put(p, x, 11, Y)
    rect(p, 22, 11, 6, 5, D); rect(p, 23, 12, 3, 3, K); put(p, 24, 13, C)
    for x in range(6, 22, 5):
        for y in range(4, 15):
            if p[y][x] in (B, L, D):
                p[y][x] = D
    PICS['B_DOME'] = (p, [(14, 2, 1)])
    p = cv(14, 44)                                           # a lattice mast with a dish
    for y in range(8, 44):
        put(p, 4, y, D); put(p, 9, y, D)
        if y % 5 == 0:
            rect(p, 4, y, 6, 1, D)
        if y % 5 == 2:
            put(p, 5 + (y // 5) % 4, y, D)
    for k in range(7):
        put(p, 2 + k, 6 - abs(k - 3) // 2, L); put(p, 2 + k, 7 - abs(k - 3) // 2, B)
    put(p, 6, 3, D); put(p, 6, 4, D)
    PICS['B_MAST'] = (p, [(6, 2, 1)])
    p = cv(30, 18)                                           # a module on stilts, a row of lit windows, a pipe
    rect(p, 3, 10, 1, 8, D); rect(p, 20, 10, 1, 8, D)
    block(p, 0, 2, 24, 9); rect(p, 0, 10, 24, 1, D)
    for k in range(5):
        rect(p, 3 + k * 4, 5, 2, 2, Y if k % 2 else C)
    rect(p, 24, 6, 6, 2, D); rect(p, 24, 6, 6, 1, B)
    rect(p, 1, 0, 6, 2, B); put(p, 3, 0, L)
    PICS['B_MODULE'] = (p, [(4, 0, 1)])
    p = cv(26, 18)                                           # fuel tanks: two spheres on a frame
    for cx in (6, 18):
        for y in range(11):
            for x in range(11):
                d = (x - 5) ** 2 + (y - 5) ** 2
                if d <= 26:
                    c = L if (x - 3) ** 2 + (y - 3) ** 2 < 6 else D if x + y > 13 else B
                    put(p, cx - 5 + x, y, c)
        rect(p, cx - 4, 11, 1, 7, D); rect(p, cx + 4, 11, 1, 7, D)
    rect(p, 0, 13, 26, 1, D); rect(p, 9, 6, 8, 1, D)
    put(p, 6, 5, R); put(p, 18, 5, R)
    PICS['B_TANKS'] = (p, [])
    p = cv(26, 26)                                           # a big radar dish on a turret
    for k in range(22):
        y = 2 + int((k - 11) ** 2 / 9)
        rect(p, 2 + k, y, 1, 3, L if k < 8 else B)
        put(p, 2 + k, y + 3, D)
    rect(p, 12, 4, 1, 10, D); put(p, 12, 3, Y)
    block(p, 9, 16, 9, 6); rect(p, 7, 22, 13, 4, D); rect(p, 7, 22, 13, 1, B)
    PICS['B_RADAR'] = (p, [(12, 3, 1)])
    p = cv(28, 16)                                           # a greenhouse: glass panes over green
    for dy in range(13):
        hw = int(math.sqrt(max(0, 12 * 12 - dy * dy)) * 1.15)
        y = 15 - dy
        rect(p, 14 - hw, y, 2 * hw, 1, C if (dy + 1) % 4 else D)
    for x in range(2, 27, 4):
        for y in range(2, 16):
            if p[y][x] == C:
                p[y][x] = D
    for x in range(3, 26, 3):
        put(p, x, 14, 3 if x % 2 else 4); put(p, x, 13, 4 if x % 6 == 0 else T if p[13][x] == T else p[13][x])
    rect(p, 0, 15, 28, 1, D)
    PICS['B_GREEN'] = (p, [])


# ---- alien technology
def alien():
    p = cv(22, 46)                                           # a spire with a disc and lights
    for k in range(38):
        w = 2 + (max(0, 8 - k) // 2)
        rect(p, 11 - w // 2, 45 - k, w, 1, D if k < 8 else B)
        put(p, 11 - w // 2, 45 - k, L)
    rect(p, 2, 5, 18, 3, B); rect(p, 2, 5, 18, 1, L); rect(p, 2, 7, 18, 1, D)
    rect(p, 6, 2, 10, 3, L); rect(p, 8, 0, 6, 2, B)
    for k in range(4):
        put(p, 4 + k * 5, 6, C)
    PICS['A_SPIRE'] = (p, [(11, 0, 1)])
    p = cv(30, 32)                                           # two pylons (the game strikes an arc between them)
    for x in (2, 24):
        block(p, x, 4, 4, 28)
        rect(p, x - 1, 2, 6, 2, L); put(p, x + 1, 1, C); put(p, x + 2, 1, C)
        for y in range(8, 30, 5):
            rect(p, x, y, 4, 1, D)
    rect(p, 0, 30, 30, 2, D)
    PICS['A_PYLONS'] = (p, [(6, 6, 3)])
    p = cv(10, 40)                                           # a glowing obelisk (rings of light climb it)
    for k in range(38):
        w = 8 - k * 4 // 38
        x0 = 1 + (8 - w) // 2
        rect(p, x0, 39 - k, w, 1, B); put(p, x0, 39 - k, L); put(p, x0 + w - 1, 39 - k, D)
    for y in range(5, 36, 6):
        put(p, 4, y, C); put(p, 5, y, C)
    put(p, 4, 0, C); put(p, 5, 0, W_)
    PICS['A_OBELISK'] = (p, [(5, 36, 2)])
    p = cv(14, 34)                                           # an orb on a stalk
    for y in range(12, 34):
        put(p, 6 + (1 if (y // 4) % 2 else 0), y, D); put(p, 7, y, B)
    for y in range(12):
        for x in range(14):
            d = (x - 6.5) ** 2 + (y - 6) ** 2
            if d <= 30:
                put(p, x, y, W_ if (x - 4) ** 2 + (y - 3) ** 2 < 4 else C if d < 18 else D)
    rect(p, 3, 31, 9, 3, D); rect(p, 4, 31, 7, 1, B)
    PICS['A_ORB'] = (p, [])
    p = cv(26, 36)                                           # a tripod tower
    for (x0, x1) in ((3, 10), (22, 15), (13, 13)):
        for y in range(16, 36):
            put(p, x0 + (x1 - x0) * (y - 16) // 20 if y < 36 else x1, 35 - (y - 16), D)
    for y in range(4, 16):
        hw = 9 - abs(y - 10)
        rect(p, 13 - hw, y, 2 * hw, 1, B); put(p, 13 - hw, y, L); put(p, 12 + hw, y, D)
    rect(p, 9, 9, 8, 2, K); put(p, 11, 9, R); put(p, 14, 9, R)
    rect(p, 12, 0, 2, 4, D)
    PICS['A_TRIPOD'] = (p, [(12, 0, 1)])
    p = cv(28, 22)                                           # a hive of pods
    for (cx, cy, r) in ((6, 15, 6), (15, 12, 8), (23, 16, 5)):
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                d = x * x + y * y * 1.3
                if d <= r * r:
                    put(p, cx + x, cy + y, L if x < -r // 2 and y < 0 else D if x > r // 2 else B)
        put(p, cx, cy, C); put(p, cx + 1, cy, C)
    rect(p, 0, 21, 28, 1, D)
    PICS['A_HIVE'] = (p, [(15, 12, 4)])


ruins()
bases()
alien()
for n in list(PICS):
    g, l = PICS[n]
    PICS[n] = (outline_dark(g), l)
THEMES = {1: ['R_COLUMN', 'R_PAIR', 'R_ARCH', 'R_TEMPLE', 'R_HEAD', 'R_OBELISK'],
          2: ['B_DOME', 'B_MAST', 'B_MODULE', 'B_TANKS', 'B_RADAR', 'B_GREEN'],
          3: ['A_SPIRE', 'A_PYLONS', 'A_OBELISK', 'A_ORB', 'A_TRIPOD', 'A_HIVE']}
