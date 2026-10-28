"""STARLIFT's four bosses (0.20), 45 x 20, built from shaded shapes and then detailed by hand.

Each shape is lit from the top left: a pixel's brightness (from the shape's curvature) picks a highlight, a mid tone, a
shade or a dark letter, so the forms read as round or faceted; an ink outline goes round everything. HOUSE letters as the
game maps them for the aliens (EMAP): s hull, n hull shade, g dome, r body take the stage's alien colours; k ink, w white,
l light, G dark green, y gold, m deep red, b steel, u and c accent.
"""
import math

W, H = 45, 20
LX, LY, LZ = -0.55, -0.65, 0.52                           # the light: from the top left, a little in front


def canvas():
    return [['.'] * W for _ in range(H)]


def shade(nx, ny, ramp):
    """A lit point on a bulge: (nx, ny) from its centre, -1..1; ramp: letters from bright to dark."""
    nz = math.sqrt(max(0.0, 1 - nx * nx - ny * ny))
    b = nx * LX + ny * LY + nz * LZ                       # facing the light: bright
    i = 0 if b > 0.94 else 1 + int((0.94 - b) / 1.2 * (len(ramp) - 1))   # only the peak takes the highlight
    return ramp[max(0, min(len(ramp) - 1, i))]


def ellipse(g, cx, cy, rx, ry, ramp, y0=-99, y1=99):
    for y in range(H):
        for x in range(W):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1 and y0 <= y <= y1:
                g[y][x] = shade(dx, dy, ramp)


def outline(g):
    out = [r[:] for r in g]
    for y in range(H):
        for x in range(W):
            if g[y][x] != '.':
                if any(not (0 <= x + a < W and 0 <= y + b < H) or g[y + b][x + a] == '.' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    out[y][x] = 'k'
    return out


def put(g, x, y, s):
    for i, ch in enumerate(s):
        if ch != ' ' and 0 <= x + i < W and 0 <= y < H:
            g[y][x + i] = ch


def rows(g):
    return [''.join(r) for r in g]


# ---- the Mothership: a glass dome on a broad lit disc, a band of windows, running lights, a glowing tractor emitter
def mothership():
    g = canvas()
    ellipse(g, 22.5, 7.5, 8.5, 6.5, 'lggggGG', y1=8)                       # the dome
    ellipse(g, 22.5, 11.0, 22.0, 5.0, 'wssssnnb')                        # the disc
    ellipse(g, 22.5, 14.5, 13.0, 3.2, 'snnbb', y0=14)                   # the hull below
    ellipse(g, 22.5, 17.5, 4.5, 2.2, 'lgG', y0=17)                      # the emitter
    g = outline(g)
    for x in range(5, 40):                                              # the rim: a dark seam under the disc's edge
        if g[12][x] not in '.k': g[12][x] = 'b'
    for i, x in enumerate(range(6, 40, 4)):                             # running lights round the rim, gold and red in turn
        g[11][x] = 'y' if i % 2 else 'r'
    for x in range(9, 37, 4):                                           # a band of lit windows, framed
        put(g, x, 10, 'ku')
    put(g, 18, 2, 'ww'); put(g, 17, 3, 'w')                            # the dome's glint
    for x in range(14, 31, 4): g[15][x] = 'k'                           # panel seams on the hull below
    put(g, 21, 18, 'lll')                                               # the emitter's glow
    return rows(g)


# ---- the Hive: a veined living dome, a row of gold eyes, mandibles, tendrils dripping beneath
def hive():
    g = canvas()
    ellipse(g, 22.5, 11.0, 19.5, 10.0, 'wrrrrrmm', y1=14)                 # the dome
    ellipse(g, 22.5, 14.5, 18.0, 2.5, 'rmmm', y0=14, y1=15)             # its rim, curling under
    for x in range(6, 40, 3):                                           # tendrils, a few swaying
        n = 3 + (x * 7 % 4)
        for k in range(n):
            xx = x + (1 if (k + x) % 4 == 3 else 0)
            if 16 + k < H: g[16 + k][xx] = 'm' if k < n - 1 else 'r'
    g = outline(g)
    for x0, y0 in ((9, 5), (16, 9), (25, 9), (32, 5), (20, 3)):         # veins
        for k in range(4):
            if g[y0 + k // 2][x0 + k] not in '.k': g[y0 + k // 2][x0 + k] = 'm'
    for x in (11, 18, 25, 32):                                          # the eyes: dark sockets, gold irises, a slit, a glint
        put(g, x - 1, 7, ' kkk ')
        put(g, x - 1, 8, 'kyyyk')
        put(g, x - 1, 9, 'kykyk')
        put(g, x - 1, 10, 'kyyyk')
        put(g, x, 11, 'kkk')
        g[8][x] = 'w'
    put(g, 17, 12, 'kwkwkwkwkwk')                                      # mandibles
    put(g, 18, 13, 'kmkmkmkmk')
    put(g, 12, 2, 'ww'); put(g, 10, 3, 'www'); put(g, 9, 4, 'ww')      # the wet glint on top
    return rows(g)


# ---- the Prism warship: a faceted crystal blade, light and dark facets, an accent spine, a red core, fins of light
def prism():
    g = canvas()
    for y in range(H):                                                  # the blade: a long low diamond, its facets in bands
        half = int(22 * (1 - abs(y - 11) / 9.5)) if 2 <= y <= 18 else -1
        for x in range(22 - half, 23 + half):
            if half < 0: continue
            f = ((x - 22) * 2 + (y - 11) * 5) // 9                      # facet index along the slant
            top = y < 11
            if x < 22: g[y][x] = ('w' if f % 2 else 'l') if top else ('s' if f % 2 else 'n')
            else: g[y][x] = ('s' if f % 2 else 'c') if top else ('n' if f % 2 else 'b')
    g = outline(g)
    for y in range(4, 18):                                              # the spine: accent light down the middle
        if g[y][22] not in '.k': g[y][22] = 'c'
    put(g, 21, 8, 'kkk'); put(g, 20, 9, 'kkrkk'); put(g, 19, 10, 'kkrwrkk'); put(g, 19, 11, 'krryrrk'); put(g, 19, 12, 'kkrrrkk'); put(g, 20, 13, 'kkrkk'); put(g, 21, 14, 'kkk')   # the core: a burning gem
    for x in (6, 38):                                                    # wing-tip lights
        put(g, x, 11, 'y')
    for x in (12, 32):                                                   # fins of light below
        put(g, x, 17, 'kck'); put(g, x, 18, 'kck'); put(g, x + 1, 19, 'c')
    return rows(g)


# ---- the Dreadnought: a skull-fronted battleship, a heavy rounded hull, burning eye sockets, a jaw of teeth, guns
def dread():
    g = canvas()
    ellipse(g, 22.5, 8.5, 18.0, 8.0, 'wssssnnb', y1=13)                  # the cranium
    ellipse(g, 22.5, 13.0, 21.0, 4.5, 'ssnnbb', y0=12, y1=16)           # the jaw-hull
    for y in range(16, 19):                                              # the keel
        for x in range(8 + (y - 16) * 2, 37 - (y - 16) * 2):
            g[y][x] = 'b' if y < 18 else 'n'
    g = outline(g)
    for cx in (15, 29):                                                  # the eye sockets: dark, a red glow, a hot gold core
        put(g, cx - 4, 5, ' kkkkkk ')
        put(g, cx - 4, 6, 'kkrrrrkk')
        put(g, cx - 4, 7, 'krrmyyrk')
        put(g, cx - 4, 8, 'krrywyrk')
        put(g, cx - 4, 9, 'kkrrrrkk')
        put(g, cx - 4, 10, ' kkkkkk ')
    put(g, 21, 10, 'kkk'); put(g, 21, 11, 'kmk')                        # the nose cavity
    put(g, 12, 13, 'kwkwkwkwkwkwkwkwkwkwk')                             # the teeth, upper and lower
    put(g, 12, 14, 'kwkwkwkwkwkwkwkwkwkwk')
    for x in (3, 39):                                                    # gun turrets either side, barrels out
        put(g, x, 11, 'kbbk'); put(g, x, 12, 'kbbk')
    put(g, 0, 11, 'kbbb'); put(g, 41, 11, 'bbbk')
    for x in range(11, 35, 6): g[17][x] = 'r'                           # gun ports along the keel, glowing
    put(g, 16, 1, 'www'); put(g, 13, 2, 'ww')                           # the cranium's glint
    for x in range(9, 37, 9):                                           # armour seams
        for y in range(3, 5):
            if g[y][x] not in '.k': g[y][x] = 'n'
    return rows(g)


MOTHERSHIP, HIVE, PRISM, DREAD = mothership(), hive(), prism(), dread()
for _d in (MOTHERSHIP, HIVE, PRISM, DREAD):
    assert len(_d) == H and all(len(r) == W for r in _d)
