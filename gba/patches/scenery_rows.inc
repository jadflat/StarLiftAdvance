// GBA, patch 01-scenery-rows (included into sketch.cpp, after the game's old_drawLayerCols / old_drawGroundCols).
//
// The same picture as the game's column loops, drawn in strips of 8 columns (SW / 8 of them): a strip's row is one 32-bit word of
// gfx_fb (8 pixels, nibble i = column 8s+i), so a row of a strip costs one read and one write instead of eight
// read-modify-writes of a byte, and none at all where the 8 columns are in the same band (a plain store).
//
// Each column's spans start at a few rows (a layer: its top, the end of its rim, the end of its body; the ground: the
// top of each band). Those rows are posted as nibble masks into slgEv[row] first; walking the strip down then ORs
// them into running masks (a column, once in a band, never leaves it before the next one starts), so each row's
// masks are a few ANDs. The texture repeats every 16 px both ways, so its 8-pixel words are worked out once a call
// per band, strip parity (even strips start at the layer's texture column, odd ones 8 later) and row & 15.
//
// One saving that changes nothing on screen: a layer's column stops at the highest top of the layers in front of it
// (the game fills it down to the next layer's top). The rows below are filled again by those nearer layers, which
// together cover everything from their highest top to the bottom, before the frame is shown.

#define SLG_IWRAM __attribute__((section(".iwram.text.slg"), noinline, target("arm")))   // (ARM, also when the rest of the file is Thumb)
struct SlgEv { uint32_t a, b, c, d; };
// per row: masks of the columns whose next span starts there (zero between uses). 256-aligned, so an entry's address
// gives its row & 15 (the texture row) without a row counter: the loops below run out of registers otherwise
static SlgEv slgEv[SH + 1] __attribute__((aligned(256)));
static uint32_t slgPat[5][2][16];        // per band, strip parity, row & 15: 8 pixels of the band's colours
static uint32_t slgTR[16][2];            // the texture by row: tile values of columns 0-7, 8-15 (one a nibble)
#define SLG_ROW15(e) (((uint32_t)(e) >> 4) & 15)

// the bands' texture words for the texture column lc (lut[b][t]: band b's colour for tile value t, 0..3)
SLG_IWRAM static void slgPatterns(int lc, const uint8_t (*lut)[4], int nb) {
    if (slgTileNew) {                                                        // buildTile() made a new texture
        slgTileNew = false;
#pragma clang loop unroll(disable)
        for (int r = 0; r < 16; r++) {
            uint32_t lo = 0, hi = 0;
#pragma clang loop unroll(disable)
            for (int i = 7; i >= 0; i--) { lo = lo << 4 | tile[i][r]; hi = hi << 4 | tile[i + 8][r]; }
            slgTR[r][0] = lo; slgTR[r][1] = hi;
        }
    }
    uint32_t c[5][4];
#pragma clang loop unroll(disable)
    for (int b = 0; b < nb; b++) for (int t = 0; t < 4; t++) c[b][t] = lut[b][t] * 0x11111111u;
#pragma clang loop unroll(disable)
    for (int q = 0; q < 32; q++) {
        const int r = q >> 1, par = q & 1, off = (lc + par * 8) & 15, o = (off & 7) * 4;
        const uint32_t u = slgTR[r][off >> 3], v = slgTR[r][(off >> 3) ^ 1];
        const uint32_t w = o ? (u >> o) | (v << (32 - o)) : u;              // nibble i: tile[(off + i) & 15][r]
        const uint32_t A = (w & 0x11111111u) * 15u, B = ((w >> 1) & 0x11111111u) * 15u;   // value bit 0, bit 1
        const uint32_t n0 = ~(A | B), n1 = A & ~B, n2 = B & ~A, n3 = A & B;
#pragma clang loop unroll(disable)
        for (int b = 0; b < nb; b++) slgPat[b][par][r] = (c[b][0] & n0) | (c[b][1] & n1) | (c[b][2] & n2) | (c[b][3] & n3);
    }
}

// a layer strip from row y to ye (posted: a = top, b = rim end, c = end; rc: each column's rim colour). Rows yf..yf1-1
// are body in all 8 columns (yf = ye: none): the texture is stored there without reading or masking.
SLG_IWRAM static void slgStripLayer(uint32_t *p, int y, int ye, int yf, int yf1, uint32_t rc, const uint32_t *pat) {
    uint32_t A = 0, B = 0, E = 0;
    SlgEv *e = slgEv + y;
    SlgEv *const end = slgEv + ye;
    for (;;) {
        for (SlgEv *const stop = slgEv + yf; e < stop; e++, p += GFX_FB_STRIDE / 4) {
            A |= e->a; B |= e->b; E |= e->c;
            e->a = 0; e->b = 0; e->c = 0;
            const uint32_t on = A & ~E;
            if (on) *p = (*p & ~on) | (rc & A & ~B) | (pat[SLG_ROW15(e)] & B & ~E);
        }
        if (e >= end) break;
        e->a = 0; e->b = 0;                                                  // (the last tops and rim ends)
        for (SlgEv *const stop = slgEv + yf1; e < stop; e++, p += GFX_FB_STRIDE / 4) *p = pat[SLG_ROW15(e)];
        A = B = ~0u;
        yf = ye;
    }
    e->a = 0; e->b = 0; e->c = 0;
}

// a ground strip from row y down (posted: a, b, c, d = the starts of bands 0, 2, 3, 4; band 1 starts a row after 0).
// u[2k - 4] .. u[2k - 3]: rows that are band k (2, 3, 4) in all 8 columns, stored without reading or masking.
SLG_IWRAM static void slgStripGround(uint32_t *p, int y, const int *u, const uint32_t *pat) {
    uint32_t C0 = 0, C2 = 0, C3 = 0, C4 = 0;
    SlgEv *e = slgEv + y;
    for (int k = 2;; k++) {
        for (SlgEv *const stop = slgEv + (k < 5 ? u[k * 2 - 4] : SH); e < stop; e++, p += GFX_FB_STRIDE / 4) {
            const uint32_t C1 = C0;
            C0 |= e->a; C2 |= e->b; C3 |= e->c; C4 |= e->d;
            e->a = 0; e->b = 0; e->c = 0; e->d = 0;
            const uint32_t *q = pat + SLG_ROW15(e);
            uint32_t v = (q[0] & C0 & ~C1) | (q[32] & C1 & ~C2) | (q[64] & C2 & ~C3) | (q[96] & C3 & ~C4) | (q[128] & C4);
            if (~C0) v |= *p & ~C0;
            *p = v;
        }
        if (k == 5) break;
        SlgEv *const stop = slgEv + u[k * 2 - 3];
        if (e >= stop) continue;                                             // not uniform anywhere
        e->a = 0; e->b = 0; e->c = 0; e->d = 0;                              // (the last columns into band k)
        const uint32_t *q = pat + k * 32;
        for (; e < stop; e++, p += GFX_FB_STRIDE / 4) *p = q[SLG_ROW15(e)];
        C0 = C2 = ~0u;
        if (k >= 3) C3 = ~0u;
        if (k >= 4) C4 = ~0u;
    }
    e->a = 0; e->b = 0; e->c = 0; e->d = 0;
}

// each layer's columns (see new_drawLayerCols), one pass over the four layers' tops for all three
#if GFX_W == 240 && GFX_H == 160
static uint32_t slgCol[1][SW];               // (StarLift Advance: only the near hills are drawn here: hbgPrep, patch 04)
#define SLG_COL(L) slgCol[0]
#else
static uint32_t slgCol[3][SW];
#define SLG_COL(L) slgCol[L]
#endif
static inline __attribute__((always_inline)) uint32_t slgWord(int y0, int y1, int slope) {
    if (y0 >= y1) return 0;
    int rim = slope > 0 ? 2 + slope * 2 : slope < 0 ? 2 - slope : 1;
    if (rim > y1 - y0) rim = y1 - y0;
    return (uint32_t)y0 | (uint32_t)(y0 + rim) << 8 | (uint32_t)y1 << 16 | (slope < 0 ? 1u << 24 : 0u);
}
#if !(GFX_W == 240 && GFX_H == 160)
SLG_IWRAM static void slgColumns() {
    int p0 = topA[0][0], p1 = topA[1][0], p2 = topA[2][0];                  // (column 0: no slope)
#pragma clang loop unroll(disable)
    for (int x = 0; x < SW; x++) {
        const int a = topA[0][x], b = topA[1][x], c = topA[2][x], d = topA[3][x];
        const int f1 = c < d ? c : d, f0 = b < f1 ? b : f1;               // what is in front: hidden below there
        slgCol[0][x] = slgWord(a, f0, p0 - a);
        slgCol[1][x] = slgWord(b, f1, p1 - b);
        slgCol[2][x] = slgWord(c, d, p2 - c);
        p0 = a; p1 = b; p2 = c;
    }
}
#endif

// as the game's drawLayerCols: a lit (or shaded) rim on the slopes, a stone texture below (plain on the far layer)
SLG_IWRAM static void new_drawLayerCols(int L) {
    const uint8_t body = LBODY[L], lit = LLIT[L], dark = LDARK[L];
    const uint8_t lut[1][4] = { { L ? dark : body, body, body, L == 2 ? lit : body } };
    slgPatterns(cam >> (3 - L), lut, 1);
    // each column: its top | where its rim ends << 8 | where it ends << 16 | a shaded rim << 24 (0: nothing to draw);
    // worked out for all three layers at once when the far one is drawn (the layers' tops are all known by then)
#if !(GFX_W == 240 && GFX_H == 160)
    if (L == 0) slgColumns();
#endif
    const uint32_t *const col = SLG_COL(L);
    uint32_t *fb = (uint32_t *)gfx_fb;
    const uint32_t litW = lit * 0x11111111u, darkW = dark * 0x11111111u;
#pragma clang loop unroll(disable)
    for (int s = 0; s < SW / 8; s++) {
        const uint32_t *cw = col + s * 8;
        int ya = SH, yb = 0, yf = 0, yf1 = SH;
        bool full = true;
#pragma clang loop unroll(disable)
        for (int i = 0; i < 8; i++) {                                        // the strip's rows; all body from yf to yf1
            const uint32_t w = cw[i];
            if (!w) { full = false; continue; }
            const int y0 = w & 255, yr = (w >> 8) & 255, y1 = (w >> 16) & 255;
            if (y0 < ya) ya = y0;
            if (y1 > yb) yb = y1;
            if (yr > yf) yf = yr;
            if (y1 < yf1) yf1 = y1;
        }
        if (ya >= yb) continue;
        uint32_t m = 15, dm = 0;
#pragma clang loop unroll(disable)
        for (int i = 0; i < 8; i++, m <<= 4) {                                // post the rows where spans start
            const uint32_t w = cw[i];
            if (!w) continue;
            slgEv[w & 255].a |= m; slgEv[(w >> 8) & 255].b |= m; slgEv[(w >> 16) & 255].c |= m;
            if (w >> 24) dm |= m;
        }
        if (!full || yf >= yf1) yf = yf1 = yb;
        slgStripLayer(fb + ya * (GFX_FB_STRIDE / 4) + s, ya, yb, yf, yf1, (litW & ~dm) | (darkW & dm), slgPat[0][s & 1]);
    }
}

// as the game's drawGroundCols: the ground in bands below its surface, each with the world's texture
SLG_IWRAM static void new_drawGroundCols() {
    const uint8_t crack = PLR.crack != C_INK ? PLR.crack : C_GNDD;
    const uint8_t LUT[5][4] = { { C_GNDL, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GND, C_GND, C_GNDL },
                                { crack, C_GND, C_GND, C_GND }, { crack, C_GNDD, C_GND, C_GNDD } };
    slgPatterns(cam, LUT, 5);
    uint32_t *fb = (uint32_t *)gfx_fb;
#pragma clang loop unroll(disable)
    for (int s = 0; s < SW / 8; s++) {
        const uint8_t *g = topA[3] + s * 8;
        int ya = SH, yz = 0;
        uint32_t m = 15;
#pragma clang loop unroll(disable)
        for (int i = 0; i < 8; i++, m <<= 4) {
            const int y0 = g[i];
            slgEv[y0].a |= m;                                   // EDGE: 0, 1, 3, 12, 28 rows below the surface
            if (y0 + 3 < SH) slgEv[y0 + 3].b |= m;
            if (y0 + 12 < SH) slgEv[y0 + 12].c |= m;
            if (y0 + 28 < SH) slgEv[y0 + 28].d |= m;
            if (y0 < ya) ya = y0;
            if (y0 > yz) yz = y0;
        }
        int u[6] = { yz + 3, ya + 12, yz + 12, ya + 28, yz + 28, SH };     // where bands 2, 3, 4 cover the strip
        for (int k = 0; k < 6; k++) if (u[k] > SH) u[k] = SH;
        for (int k = 0; k < 6; k += 2) if (u[k] > u[k + 1]) u[k] = u[k + 1];   // (none: start = end)
        slgStripGround(fb + ya * (GFX_FB_STRIDE / 4) + s, ya, u, slgPat[0][s & 1]);
    }
}

#ifndef GBA_SCENECHECK
#if GFX_W == 240 && GFX_H == 160
#include "hills_bg.inc"                       // StarLift Advance: the far and mid hills on background layers (patch 04)
static void drawLayerCols(int L) {
    if (L == 0) hbgDraw();
    else if (L == 2) new_drawLayerCols(2);                                 // (the near hills: still drawn, in strips)
}
#elif defined(GBA_SLG_TWICE)
static void drawLayerCols(int L) { new_drawLayerCols(L); new_drawLayerCols(L); }   // timing experiment only
#else
static void drawLayerCols(int L) { new_drawLayerCols(L); }
#endif
#if GFX_W == 240 && GFX_H == 160
#include "ground_bg.inc"                      // StarLift Advance: the ground on a background layer of its own
static void drawGroundCols() { bgGroundDraw(); }
#else
static void drawGroundCols() { new_drawGroundCols(); }
#endif
#else
// The check build: each call draws with the game's functions and with the new ones from the same framebuffer and
// compares them (a layer: the game's picture down to where it is hidden, nothing touched below). Counts are read by
// the test script.
extern "C" {
uint32_t gba_scChecks, gba_scBad, gba_scBadFirst;
uint8_t gba_scSave[GFX_FB_BYTES] __attribute__((aligned(4))), gba_scRef[GFX_FB_BYTES] __attribute__((aligned(4)));
}
static void scCompare(int L) {
    gba_scChecks++;
    for (int x = 0; x < SW; x++) {
        int ye = SH;
        if (L < 3) for (int j = L + 1; j < 4; j++) if (topA[j][x] < ye) ye = topA[j][x];
        for (int y = 0; y < SH; y++) {
            const int o = y * GFX_FB_STRIDE + (x >> 1), sh = (x & 1) * 4;
            if (((gfx_fb[o] ^ (y < ye ? gba_scRef : gba_scSave)[o]) >> sh) & 15) {
                if (!gba_scBad++) gba_scBadFirst = (uint32_t)(L << 16 | y << 8 | x);
                return;
            }
        }
    }
}
static void drawLayerCols(int L) {
    memcpy(gba_scSave, gfx_fb, GFX_FB_BYTES);
    old_drawLayerCols(L);
    memcpy(gba_scRef, gfx_fb, GFX_FB_BYTES);
    memcpy(gfx_fb, gba_scSave, GFX_FB_BYTES);
    new_drawLayerCols(L);
    scCompare(L);
}
static void drawGroundCols() {
    memcpy(gba_scSave, gfx_fb, GFX_FB_BYTES);
    old_drawGroundCols();
    memcpy(gba_scRef, gfx_fb, GFX_FB_BYTES);
    memcpy(gfx_fb, gba_scSave, GFX_FB_BYTES);
    new_drawGroundCols();
    scCompare(3);
}
#endif
