"""STARLIFT's bases: four designs for the player's base and four for the aliens' collector, pictures for the card.

HOUSE letters as in make_sprites.py: 'k' ink, 'w' white, 's' silver (the game recolours it with the world's base colour),
'n' steel, 'y' gold, 'r' red, 'c' the world's accent, 'g'/'l'/'G' greens (the aliens' slime).
For each player base: the x of its door. For each collector: the y (from the ground) where colonists drop in, and the x of
its three tubes into the ground.
"""
import math

T = 15
K, W, GD, G, GL, S, R, Y, N, C = 0, 1, 2, 3, 4, 5, 6, 8, 11, 13


def cv(w, h):
    return [[T] * w for _ in range(h)]


def put(p, x, y, c):
    if 0 <= y < len(p) and 0 <= x < len(p[0]):
        p[y][x] = c


def rect(p, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            put(p, xx, yy, c)


def panel(p, x, y, w, h):
    """A metal panel lit from the left."""
    rect(p, x, y, w, h, S)
    rect(p, x, y, 1, h, W)
    rect(p, x, y, w, 1, W)
    rect(p, x + w - 1, y + 1, 1, h - 1, N)
    rect(p, x + 1, y + h - 1, w - 1, 1, N)


def station():
    """Modules stacked like a space station: a long hab, a control deck, an antenna and a dish."""
    p = cv(44, 32)
    panel(p, 1, 20, 42, 11)
    for x in range(4, 40, 5):
        rect(p, x, 23, 3, 3, Y)
    rect(p, 1, 27, 42, 1, R)
    rect(p, 17, 24, 8, 7, K); rect(p, 18, 25, 6, 6, Y); rect(p, 21, 25, 1, 6, N)        # the door
    panel(p, 9, 11, 26, 9)
    for x in range(12, 32, 4):
        rect(p, x, 14, 2, 2, C)
    rect(p, 30, 2, 1, 9, N); put(p, 30, 1, R)                                         # the antenna
    for k in range(9):
        put(p, 11 + k, 8 - abs(k - 4) // 2, W if k < 4 else S); put(p, 11 + k, 9 - abs(k - 4) // 2, N)
    rect(p, 15, 9, 1, 2, N)
    rect(p, 0, 31, 44, 1, K)
    return p, 21


def outpost():
    """A stone outpost: stepped blocks, an arched door, a mast with a banner."""
    p = cv(44, 32)
    for k, (x, w) in enumerate(((0, 44), (4, 36), (8, 28))):
        y = 31 - 8 * (k + 1)
        panel(p, x, y, w, 8)
        for xx in range(x + 3, x + w - 2, 6):
            rect(p, xx, y + 3, 1, 4, N)
    for y in range(20, 31):                                                             # an arched door
        hw = 4 if y > 22 else 3 if y > 20 else 2
        rect(p, 22 - hw, y, 2 * hw, 1, K)
    rect(p, 19, 24, 6, 7, Y); rect(p, 21, 24, 2, 7, Y)
    rect(p, 36, 0, 1, 15, N); rect(p, 37, 1, 6, 4, R); rect(p, 37, 1, 6, 1, W)             # the mast and its banner
    for x in range(12, 33, 6):
        put(p, x, 11, Y)
    rect(p, 0, 31, 44, 1, K)
    return p, 22


def tower():
    """A round tower with a saucer roof and a ring of windows, and an annex at its foot."""
    p = cv(44, 34)
    for y in range(8, 33):
        rect(p, 16, y, 12, 1, S); put(p, 16, y, W); put(p, 17, y, W); put(p, 27, y, N); put(p, 26, y, N)
    for y in range(12, 30, 6):
        for x in range(18, 26, 3):
            rect(p, x, y, 2, 2, Y if (x + y) % 2 else C)
    for y in range(4):                                                                  # the saucer roof
        hw = 15 - abs(y - 2) * 3
        rect(p, 22 - hw, 3 + y, 2 * hw, 1, W if y < 2 else S)
    rect(p, 6, 6, 32, 1, N)
    for x in range(8, 37, 4):
        put(p, x, 5, Y)
    rect(p, 20, 0, 4, 3, S); put(p, 21, 0, R)
    panel(p, 2, 24, 14, 8); rect(p, 5, 26, 3, 3, Y); rect(p, 10, 26, 3, 3, C)            # the annex
    rect(p, 19, 26, 6, 7, K); rect(p, 20, 27, 4, 6, Y)                                   # the door
    rect(p, 0, 33, 44, 1, K)
    return p, 22


def jar():
    """A glass jar of green slime on a stand, a funnel lid; tubes run from its foot into the ground."""
    p = cv(40, 34)
    for y in range(4, 24):
        hw = 12 if 6 < y < 22 else 10
        for x in range(20 - hw, 20 + hw):
            edge = x in (20 - hw, 20 + hw - 1)
            put(p, x, y, C if edge else (G if y > 13 else T))
        put(p, 20 - hw + 2, y, W if 7 < y < 12 else p[y][20 - hw + 2])
    for x in range(10, 30):
        put(p, x, 13, GL)
    rect(p, 6, 2, 28, 2, N); rect(p, 6, 2, 28, 1, S); rect(p, 15, 0, 10, 2, N)           # the lid and funnel
    rect(p, 17, 0, 6, 1, K)
    rect(p, 10, 24, 20, 2, N); rect(p, 11, 26, 2, 8, N); rect(p, 27, 26, 2, 8, N)
    for x in (16, 20, 24):
        rect(p, x - 1, 26, 2, 8, N)
        for y in range(28, 34, 3):
            rect(p, x - 1, y, 2, 1, GD)
    for (x, y) in ((14, 16), (22, 19), (25, 15), (18, 21)):
        put(p, x, y, GL)
    return p, 34, (15, 19, 23)


def cage():
    """A barred pen on a platform under a hopper; the floor glows."""
    p = cv(40, 34)
    rect(p, 6, 0, 28, 3, N); rect(p, 6, 0, 28, 1, S)
    for y in range(3, 7):
        rect(p, 12 + (y - 3) * 2, y, 16 - (y - 3) * 4, 1, N)
    rect(p, 4, 7, 32, 2, S); rect(p, 4, 7, 32, 1, W)
    for x in range(5, 36, 3):
        rect(p, x, 9, 1, 15, N)
    rect(p, 5, 21, 31, 3, G); rect(p, 5, 21, 31, 1, GL)
    rect(p, 2, 24, 36, 3, S); rect(p, 2, 24, 36, 1, W); rect(p, 2, 26, 36, 1, N)
    for x in range(6, 36, 6):
        put(p, x, 25, R)
    for x in (15, 19, 23):
        rect(p, x, 27, 2, 7, N)
    rect(p, 4, 27, 2, 7, N); rect(p, 34, 27, 2, 7, N)
    return p, 33, (15, 19, 23)


def mouth():
    """A great alien plant-mouth: a green bulb, a ring of teeth round a dark throat, eyes on stalks, roots into the ground."""
    p = cv(40, 34)
    for y in range(6, 28):
        hw = int(math.sqrt(max(0, 1 - ((y - 17) / 11.5) ** 2)) * 17)
        for x in range(20 - hw, 20 + hw):
            c = GL if x < 20 - hw + 4 and y < 17 else GD if x > 20 + hw - 4 else G
            put(p, x, y, c)
    for y in range(6, 12):                                                              # the throat
        hw = 9 - (y - 6)
        rect(p, 20 - hw, y, 2 * hw, 1, K)
    for x in range(11, 30, 3):
        put(p, x, 6, W); put(p, x + 1, 7, W)
    for (x, top) in ((8, 0), (32, 2)):                                                  # eyes on stalks
        rect(p, x, top + 3, 1, 5, GD)
        rect(p, x - 1, top, 3, 3, W); put(p, x, top + 1, K)
    for k, x in enumerate((12, 19, 27)):
        for y in range(27, 34):
            put(p, x + ((y + k) // 3) % 2, y, GD)
    rect(p, 15, 18, 10, 2, GD)
    return p, 28, (13, 20, 28)


BASES = [station, outpost, tower]
COLLECTORS = [jar, cage, mouth]
