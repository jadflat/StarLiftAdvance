// StarLift Advance (included by scenery_rows.inc): the ground on the GBA's background layer 1, behind the picture.
//
// The ground is fixed in the world, so it is drawn once into tiles, 8 columns at a time as they come into view, and
// the hardware scrolls it (BG1, 256 pixels around, a ring of 32 tile columns). The picture (BG0) is cleared to colour 0
// (see-through) below the ground line each frame, so the layer shows there and everything drawn later (pads, colonists,
// the lander) stays in front of it. Hills never reach below the ground line (each layer stops at the tops in front),
// so nothing else changes: the pixels on screen are the ones drawGroundCols() drew into the picture, plus some detail
// the picture never had time for: pebbles in the soil (gbgPebble).
//
// Tiles (4 bits a pixel) after the second picture buffer: char block 3 (0x0600C000), tile 88 blank, 89-92 the deepest
// band's texture (it repeats every 16 px: 2x2 tiles), then GBG_SLOTS tiles for each ring column for the rows where the
// surface and its bands are. The map is screen block 30.
void gba_groundScroll(int x);                 // gba/src/gfx_gba.cpp: BG1's scroll for the frame being drawn
enum { GBG_BASE = 0x0600C000, GBG_BLANK = 88, GBG_PAT = 89, GBG_SLOT0 = 93, GBG_SLOTS = 9, GBG_MAP = 0x0600F000 };
static int16_t gbgCol[32];                    // the world tile column each ring column holds (-1: none)
static uint8_t gbgLut[5][4];                  // each band's colour for each tile value (as drawGroundCols)
uint32_t gba_gbgOverflow;                     // (tests) tile columns that needed more than GBG_SLOTS tiles
uint32_t gba_gbgPeb[64];                      // the pebbles (see gbgPebble), EWRAM: bit y & 31 of word wx & 63
static inline volatile uint32_t *gbgTile(int idx) { return (volatile uint32_t *)(GBG_BASE + idx * 32); }
static inline int gbgBand(int d) { return d < 1 ? 0 : d < 3 ? 1 : d < 12 ? 2 : d < 28 ? 3 : 4; }

// a new stage (or the first frame): the colours, the shared tiles, an empty ring
__attribute__((noinline)) static void gbgReset() {
    const uint8_t crack = PLR.crack != C_INK ? PLR.crack : C_GNDD;
    const uint8_t L[5][4] = { { C_GNDL, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GND, C_GND, C_GNDL },
                              { crack, C_GND, C_GND, C_GND }, { crack, C_GNDD, C_GND, C_GNDD } };
    memcpy(gbgLut, L, sizeof gbgLut);
    for (int x = 0; x < 64; x++) {                                           // the pebbles (see gbgPebble)
        uint32_t m = 0;
        for (int y = 0; y < 32; y++) if (hash((uint32_t)(x * 131 + y * 7919) ^ (uint32_t)stage * 977u) % 61u == 0) m |= 1u << y;
        gba_gbgPeb[x] = m;
    }
    for (int r = 0; r < 8; r++) gbgTile(GBG_BLANK)[r] = 0;
    for (int k = 0; k < 4; k++)                                              // the deep texture at x & 8, y & 8
        for (int r = 0; r < 8; r++) {
            uint32_t w = 0;
            for (int i = 0; i < 8; i++) w |= (uint32_t)gbgLut[4][tile[(k & 1) * 8 + i][((k >> 1) * 8 + r) & 15]] << (i * 4);
            gbgTile(GBG_PAT + k)[r] = w;
        }
    volatile uint16_t *map = (volatile uint16_t *)GBG_MAP;
    for (int i = 0; i < 32 * 32; i++) map[i] = GBG_BLANK;
    for (int i = 0; i < 32; i++) gbgCol[i] = -1;
}

// StarLift Advance detail: a pebble here? About one in 60 soil pixels, from a 64 x 32 table made once a stage (the
// pattern repeats every 64 px across; a hash a pixel made each new column cost 1-2 ms)
static inline bool gbgPebble(int wx, int y) { return (gba_gbgPeb[wx & 63] >> (y & 31)) & 1; }

// world tile column X (world x 8X .. 8X + 7) into ring column X & 31
SLG_IWRAM static void gbgColumn(int X) {                                  // (in IWRAM: a whole screen of them when the camera jumps)
    const int slot = X & 31, wx0 = X * 8;
    int y0[8], ymin = SH, ymax = 0;
    for (int i = 0; i < 8; i++) {
        y0[i] = clampi(groundAt(wx0 + i), TOP, SH - 1);                       // as layerTops() makes topA[3]
        if (y0[i] < ymin) ymin = y0[i];
        if (y0[i] > ymax) ymax = y0[i];
    }
    volatile uint16_t *map = (volatile uint16_t *)GBG_MAP + slot;
    int used = 0;
    for (int ty = 0; ty < SH / 8; ty++) {
        const int yt = ty * 8;
        uint16_t idx;
        if (yt + 7 < ymin) idx = GBG_BLANK;                                     // all above the surface
        else if (yt >= ymax + 28 || used == GBG_SLOTS) {                         // all in the deepest band
            if (yt < ymax + 28) gba_gbgOverflow++;
            idx = (uint16_t)(GBG_PAT + (X & 1) + 2 * (ty & 1));
        } else {
            idx = (uint16_t)(GBG_SLOT0 + slot * GBG_SLOTS + used++);
            volatile uint32_t *t = gbgTile(idx);
            for (int r = 0; r < 8; r++) {
                const int y = yt + r;
                uint32_t w = 0;
                for (int i = 0; i < 8; i++) {
                    const int d = y - y0[i];
                    if (d < 0) continue;
                    uint32_t c = gbgLut[gbgBand(d)][tile[(wx0 + i) & 15][y & 15]];
                    if (d >= 4 && d < 26) {                                   // detail: pebbles in the soil, lit on top
                        if (gbgPebble(wx0 + i, y)) c = C_GNDL;
                        else if (gbgPebble(wx0 + i, y - 1)) c = C_GNDD;     // (and its shadow under it)
                    }
                    w |= c << (i * 4);
                }
                t[r] = w;
            }
        }
        map[ty * 32] = idx;
    }
    gbgCol[slot] = (int16_t)X;
}

// the picture below the ground line, cleared (see-through) a strip at a time; rows from yz down are all ground
SLG_IWRAM static void gbgClearStrip(uint32_t *p, int y, int yz) {
    uint32_t C = 0;
    SlgEv *e = slgEv + y;
    for (; y <= yz; y++, e++, p += GFX_FB_STRIDE / 4) { C |= e->a; e->a = 0; *p &= ~C; }
    for (; y < SH; y++, p += GFX_FB_STRIDE / 4) *p = 0;
}

__attribute__((noinline)) static void bgGroundDraw() {
    if (slgStageNew) { slgStageNew = false; gbgReset(); }                     // (set by buildTile, patch 01)
    for (int X = cam >> 3, X1 = (cam + SW - 1) >> 3; X <= X1; X++) if (gbgCol[X & 31] != X) gbgColumn(X);
    gba_groundScroll(cam);
#ifdef GBA_GROUNDCHECK
    void gbgCheck(); gbgCheck();
#endif
    uint32_t *fb = (uint32_t *)gfx_fb;
    for (int s = 0; s < SW / 8; s++) {
        const uint8_t *g = topA[3] + s * 8;
        int ya = SH, yz = 0;
        uint32_t m = 15;
        for (int i = 0; i < 8; i++, m <<= 4) {
            const int y0 = g[i];
            slgEv[y0].a |= m;
            if (y0 < ya) ya = y0;
            if (y0 > yz) yz = y0;
        }
        gbgClearStrip(fb + ya * (GFX_FB_STRIDE / 4) + s, ya, yz);
    }
}

#ifdef GBA_GROUNDCHECK
// A test build: each frame the ground is also drawn the picture's way (new_drawGroundCols, patch 01's strips) and every
// pixel below the ground line is compared with what BG1 shows there (its map and tiles read back from VRAM).
extern "C" { uint32_t gba_gcChecks, gba_gcBad, gba_gcBadFirst; uint8_t gba_gcSave[GFX_FB_BYTES] __attribute__((aligned(4))); }
void gbgCheck() {
    memcpy(gba_gcSave, gfx_fb, GFX_FB_BYTES);
    new_drawGroundCols();
    gba_gcChecks++;
    const volatile uint16_t *map = (const volatile uint16_t *)GBG_MAP;
    for (int x = 0; x < SW; x++)
        for (int y = topA[3][x]; y < SH; y++) {
            const int wx = cam + x, idx = map[(y >> 3) * 32 + ((wx >> 3) & 31)];
            const uint32_t row = gbgTile(idx)[y & 7];
            const int bg = (row >> ((wx & 7) * 4)) & 15, fbp = (gfx_fb[y * GFX_FB_STRIDE + (x >> 1)] >> ((x & 1) * 4)) & 15;
            if (bg != fbp) { if (!gba_gcBad++) gba_gcBadFirst = (uint32_t)(x << 16 | y << 8 | idx); x = SW; break; }
        }
    memcpy(gfx_fb, gba_gcSave, GFX_FB_BYTES);
}
#endif
