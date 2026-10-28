"""STARLIFT's sky objects: planets, suns and moons, pictures for the card (one per world's sky; the aurora is drawn by code).

HOUSE letters the game maps for the sky: 'c' the world's accent, 'y' gold, 'w' white, 's' silver, 'g' green, 'r' red,
'k' ink, 'n' steel; and three of the world's own: 'b' far hills, 'o' light ground, 'm' the upper sky (a night side
that fades into it). The night side is dithered.
"""
import math

T = 15
K, W, G, S, R, Y, N, C = 0, 1, 3, 5, 6, 8, 11, 13
SKYC, FARC, LGT = 7, 9, 12         # 'm' the sky, 'b' far hills, 'o' light ground


def cv(w, h):
    return [[T] * w for _ in range(h)]


def ball(p, cx, cy, r, colour, night=None, lit=None):
    """A sphere lit from the left: colour(x, y) for the day side; the right edge dithered into the night colour."""
    for y in range(-r, r + 1):
        for x in range(-r, r + 1):
            if x * x + y * y > r * r + r * 0.6:
                continue
            c = colour(x, y)
            hw = math.sqrt(max(0, r * r - y * y))
            e = x - hw * 0.35
            if night is not None and (e > 2 or (e > 0 and (x + y) % 2)):
                c = night
            elif lit is not None and x < -hw * 0.6 and y < 0:
                c = lit
            p[cy + y][cx + x] = c


def earth():
    p = cv(21, 21)
    land = {(-5, -3), (-4, -3), (-4, -2), (-3, -2), (-5, -2), (-3, -1), (-2, -1), (-3, 0), (-2, 0), (-2, 1), (-1, 2), (-1, 3), (2, -5), (3, -5),
            (3, -4), (4, -4), (2, -4), (1, 4), (2, 4), (2, 5), (3, 5), (-6, 3), (-7, 2), (-6, 2)}
    cloud = {(-7, -4), (-6, -5), (-5, -6), (-4, -6), (0, -2), (1, -2), (2, -1), (3, 0), (-1, 6), (0, 6), (-6, -1), (-7, 0), (4, 2), (5, 2)}
    ball(p, 10, 10, 9, lambda x, y: W if (x, y) in cloud else G if (x, y) in land else C, night=K)
    return p


def sun():
    p = cv(15, 15)
    ball(p, 7, 7, 7, lambda x, y: W if x * x + y * y < 10 else Y if x * x + y * y < 26 else LGT)
    return p


def giant():
    p = cv(49, 49)
    def col(x, y):
        if 4 < y < 9 and -9 < x < 2:
            return R                                   # the great storm
        return C if ((y + 30) // 5) % 2 else Y
    ball(p, 24, 24, 24, col, night=FARC)
    return p


def redsun():
    p = cv(73, 38)
    for y in range(38):
        for x in range(73):
            d = (x - 36) ** 2 + (y - 37) ** 2
            if d <= 36 * 36:
                p[y][x] = Y if d < 12 * 12 else C if d < 22 * 22 or (d < 26 * 26 and (x + y) % 2) else R
    return p


def ringed():
    p = cv(67, 33)
    cx, cy, r = 33, 16, 15
    def ring(front):
        for a in range(360):
            t = math.radians(a)
            x, y = cx + int(round(math.cos(t) * 32)), cy + int(round(math.sin(t) * 6))
            if (math.sin(t) >= 0) == front and 0 <= x < 67:
                p[y][x] = Y
                if front and y + 1 < 33:
                    p[y + 1][x] = FARC
    ring(False)
    for y in range(-r, r + 1):
        for x in range(-r, r + 1):
            if x * x + y * y <= r * r:
                c = LGT if ((y + 20) // 4) % 2 else W
                if x > r - 4 and (x + y) % 2 or x > r - 2:
                    c = FARC
                p[cy + y][cx + x] = c
    ring(True)
    return p


def twinsun():
    p = cv(34, 26)
    ball(p, 6, 6, 6, lambda x, y: W if x * x + y * y < 12 else C)
    ball(p, 26, 19, 4, lambda x, y: W if x * x + y * y < 4 else Y)
    return p


def moons():
    p = cv(74, 16)
    ball(p, 7, 8, 7, lambda x, y: S, night=None)
    for y in range(-6, 7):                                 # a crescent: the sky bites the right side
        for x in range(-6, 7):
            if (x - 3) ** 2 + (y + 2) ** 2 <= 36 and p[8 + y][7 + x] != T:
                p[8 + y][7 + x] = T
    ball(p, 67, 5, 4, lambda x, y: W if x < -1 and y < 0 else C)
    return p


# in the order of SkyObj (Game.h): EARTH, SUN, AURORA (none: the game draws it), GIANT, REDSUN, RINGED, TWINSUN, MOONS
SKIES = [('EARTH', earth, 100, 27), ('SUN', sun, 30, 34), ('AURORA', None, 0, 0), ('GIANT', giant, 92, 48), ('REDSUN', redsun, 64, 60),
         ('RINGED', ringed, 40, 33), ('TWINSUN', twinsun, 36, 37), ('MOONS', moons, 64, 30)]   # name, painter, centre x and y on screen
