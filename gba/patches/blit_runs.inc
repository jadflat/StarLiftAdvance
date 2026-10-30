// GBA, patch 02-blit-runs (included into sketch.cpp right after the game's old_blitRd, which it replaces).
// The same pixels as old_blitRd: the reader (Rd::next) in registers, the runs written here instead of by gfx_hline
// and pset, each with its own clipping (gfx_hline: the library's clip rectangle; pset: the screen).
#include "CHGfx_internal.h"

// pixels [x0, x1) of a row in colour c (0..15): halfword and word stores in the middle (span_fast.inc), for runs of up
// to 16 pixels (byte pairs cost an EWRAM access each)
#include "span_fast.inc"
static inline __attribute__((always_inline, target("arm"))) void blrSpan(uint8_t *row, int x0, int x1, uint8_t c) {
    gbaSpan(row, x0, x1, c);
}

// Rd::next(), the reader kept as pointers: p the next byte, pend the end of the card block in hand (null from flash,
// where it never ends), ps where that block's reading started (off: its card offset). A failed card read gives 0xFF
// and leaves p == pend, so the next byte tries the same offset again, as Rd::next does.
#define BLR_NEXT(b) do {                                                                                        \
        if (p == pend) {                                                                                        \
            off += (uint32_t)(p - ps);                                                                          \
            p = ps = card::at(off);                                                                             \
            if (!p) { pend = nullptr; (b) = 0xFF; break; }                                                      \
            pend = p + (512 - (off & 511));                                                                     \
        }                                                                                                       \
        (b) = *p++;                                                                                             \
    } while (0)

// the reader's fields as Rd::next() would have left them
static inline __attribute__((always_inline, target("arm"))) void blrDone(Rd &rd, const uint8_t *p, const uint8_t *ps, const uint8_t *pend, uint32_t off) {
    const uint32_t n = (uint32_t)(p - ps);
    rd.left = rd.fromCard ? (uint16_t)(pend - p) : (uint16_t)(rd.left - n);
    rd.off = off + n;
    rd.p = p;
}

// ---- StarLift Advance: decoded sprites, kept --------------------------------------------------------------------------
// Decoding the runs costs a few branches each and their short pixel runs a read-modify-write of EWRAM a pixel. So each
// picture is decoded once, in the colours it is drawn in (the remap) and its flip, into 8-pixel words with a mask (the
// same nibble order as gfx_fb), kept in EWRAM; drawing it is then a masked word store per 8 pixels, shifted to any x.
// The same pixels as the run loop below, which still draws whatever is clipped (gba_clipY, a clip rectangle).
// Nothing ever goes stale (pictures are in ROM, the colours are part of the key); a full pool is emptied and refilled.
struct BcEnt { uint32_t src, lut[4]; uint16_t w, h, next; uint8_t flip, nw; uint32_t *data; };
enum { BC_POOL = 24 * 1024, BC_ENTS = 384 };                                    // words (96 KB), pictures
static uint32_t gba_bcPool[BC_POOL] __attribute__((section(".ewram")));
static BcEnt gba_bcEnt[BC_ENTS] __attribute__((section(".ewram")));
static uint16_t gba_bcHead[256] __attribute__((section(".ewram")));             // per bucket: an entry + 1 (0: none)
static uint32_t gba_bcUsed __attribute__((section(".ewram"))), gba_bcN __attribute__((section(".ewram")));
uint32_t gba_bcResets;                                                          // (tests) times the pool filled up

__attribute__((noinline)) static BcEnt *bcMake(const uint8_t *p, uint32_t src, const uint32_t *lut4, int w, int h, bool flip, uint32_t bucket) {
    const int nw = (w + 7) >> 3;
    const uint32_t need = 2u * (uint32_t)nw * (uint32_t)h;
    if (need > BC_POOL) return nullptr;
    if (gba_bcUsed + need > BC_POOL || gba_bcN == BC_ENTS) {
        memset(gba_bcHead, 0, sizeof gba_bcHead); gba_bcUsed = gba_bcN = 0; gba_bcResets++;
    }
    BcEnt &e = gba_bcEnt[gba_bcN];
    e.src = src; for (int k = 0; k < 4; k++) e.lut[k] = lut4[k];
    e.w = (uint16_t)w; e.h = (uint16_t)h; e.flip = flip; e.nw = (uint8_t)nw; e.data = gba_bcPool + gba_bcUsed;
    gba_bcUsed += need;
    e.next = gba_bcHead[bucket]; gba_bcHead[bucket] = (uint16_t)++gba_bcN;
    uint32_t *d = e.data;
    memset(d, 0, need * 4);
    const uint8_t *lut = (const uint8_t *)lut4;
    for (int r = 0; r < h; r++, d += 2 * nw)                                     // a row: nw words of colour, nw of mask
        for (int c = 0; c < w;) {
            const uint8_t b = *p++;
            const int n = (b >> 4) + 1, v = b & 15;
            if (v != 15)
                for (int k = 0; k < n; k++) {
                    const int X = flip ? w - 1 - (c + k) : c + k;                // (flipped: as the run loop's x + w - c - n)
                    if ((unsigned)X >= (unsigned)w) continue;
                    d[X >> 3] |= (uint32_t)(lut[v] & 15) << ((X & 7) * 4);
                    d[nw + (X >> 3)] |= 15u << ((X & 7) * 4);
                }
            c += n;
        }
    return &e;
}

// the pool's entry for rd's picture in these colours and flip (decoding it first if new); null: not possible (no card data)
static inline __attribute__((always_inline)) const BcEnt *bcGet(const Rd &rd, int w, int h, bool flip, const uint32_t *lut4) {
    const uint32_t src = rd.fromCard ? (rd.off | 0x80000000u) : (uint32_t)rd.p;
    uint32_t hs = src * 2654435761u ^ (flip ? 0x9E3779B9u : 0u);
    for (int k = 0; k < 4; k++) hs = (hs ^ lut4[k]) * 16777619u;
    const uint32_t bucket = (hs ^ (hs >> 16)) & 255;
    const BcEnt *e = nullptr;
    for (uint32_t i = gba_bcHead[bucket]; i; i = gba_bcEnt[i - 1].next) {
        const BcEnt &c = gba_bcEnt[i - 1];
        if (c.src == src && c.flip == flip && c.w == w && c.h == h && c.lut[0] == lut4[0] && c.lut[1] == lut4[1] && c.lut[2] == lut4[2] && c.lut[3] == lut4[3]) { e = &c; break; }
    }
    if (!e) {
        const uint8_t *p = rd.fromCard ? card::at(rd.off) : rd.p;                // (the GBA's card is all in ROM, in one piece)
        if (!p || !(e = bcMake(p, src, lut4, w, h, flip, bucket))) return nullptr;
    }
    return e;
}

// draw rd's picture from the pool (decoding it first if new); false: not possible (no card data), use the run loop
__attribute__((section(".iwram.text.bc"), noinline, target("arm")))
static bool bcBlit(const Rd &rd, int w, int h, int x, int y, bool flip, const uint32_t *lut4) {
    const BcEnt *e = bcGet(rd, w, h, flip, lut4);
    if (!e) return false;
    const int nw = e->nw, s = (x & 7) * 4, wx = x >> 3;
    int r = y < 0 ? -y : 0, r1 = h;
    if (y + r1 > SH) r1 = SH - y;
    const uint32_t *row = e->data + r * 2 * nw;
    uint32_t *dst = (uint32_t *)gfx_fb + (y + r) * (GFX_FB_STRIDE / 4);
    const uint8_t *const clip = gba_clipY;                                       // (patch 04: behind a hills layer)
    uint8_t gmin[24], gmax[24];                                                  // per 8 columns: the clip's lowest and highest row
    const bool fastClip = clip && nw + 1 <= 24;
    if (fastClip)
        for (int i = 0; i <= nw; i++) {
            const unsigned k = (unsigned)(wx + i);
            uint8_t lo = 255, hi = 0;
            if (k < (unsigned)(GFX_FB_STRIDE / 4)) for (int j = 0; j < 8; j++) { const uint8_t v = clip[k * 8 + j]; if (v < lo) lo = v; if (v > hi) hi = v; }
            gmin[i] = lo; gmax[i] = hi;
        }
#pragma clang loop unroll(disable)
    for (; r < r1; r++, row += 2 * nw, dst += GFX_FB_STRIDE / 4) {
        uint32_t cd = 0, cm = 0;
#pragma clang loop unroll(disable)
        for (int i = 0; i <= nw; i++) {
            uint32_t vd = cd, vm = cm;
            if (i < nw) {
                const uint32_t a = row[i], m = row[nw + i];
                vd |= a << s; vm |= m << s;
                cd = s ? a >> (32 - s) : 0; cm = s ? m >> (32 - s) : 0;
            }
            const unsigned k = (unsigned)(wx + i);
            if (!vm || k >= (unsigned)(GFX_FB_STRIDE / 4)) continue;
            if (clip) {                                                          // only the columns still open at this row
                const uint8_t *c = clip + k * 8;
                const int yy = y + r;
                if (fastClip && yy >= gmax[i]) continue;                         // all 8 columns closed
                if (!fastClip || yy >= gmin[i]) {                                // (else all open)
                    uint32_t open = 0;
#pragma clang loop unroll(disable)
                    for (int j = 0; j < 8; j++) if (yy < c[j]) open |= 15u << (j * 4);
                    vm &= open;
                    if (!vm) continue;
                }
            }
            dst[k] = (dst[k] & ~vm) | (vd & vm);
        }
    }
    return true;
}

// StarLift Advance: the picture as a hardware sprite (gfx_gba.cpp's gba_objAdd) instead of pixels in the framebuffer: the
// pool's decoded rows are already tile rows (8 pixels, the same nibble order), so a sprite is a copy of them into OBJ tiles
// in the shape's order (the colours are the game palette's, OBJ palette 0 has it too). false: no room, draw it as pixels.
volatile uint32_t *gba_objAdd(int shape, int size, int tiles, int x, int y, int prio);
static bool bcObj(const Rd &rd, int w, int h, int x, int y, bool flip, const uint32_t *lut4, int prio) {
    static const uint8_t kShape[12][4] = {                                      // shape, size, tiles across, tiles down
        {0, 0, 1, 1}, {1, 0, 2, 1}, {2, 0, 1, 2}, {0, 1, 2, 2}, {1, 1, 4, 1}, {2, 1, 1, 4},
        {1, 2, 4, 2}, {2, 2, 2, 4}, {0, 2, 4, 4}, {1, 3, 8, 4}, {2, 3, 4, 8}, {0, 3, 8, 8} };
    const int nw = (w + 7) >> 3, nh = (h + 7) >> 3;
    const BcEnt *e = bcGet(rd, w, h, flip, lut4);
    if (!e) return false;
    for (int c0 = 0; c0 < nw; c0 += 8) {                                        // (wider than 8 tiles: one sprite a chunk of 8)
        const int cw = nw - c0 < 8 ? nw - c0 : 8;
        int best = -1;
        for (int k = 0; k < 12; k++)
            if (kShape[k][2] >= cw && kShape[k][3] >= nh && (best < 0 || kShape[k][2] * kShape[k][3] < kShape[best][2] * kShape[best][3])) best = k;
        if (best < 0) return false;
        const int tw = kShape[best][2], th = kShape[best][3];
        volatile uint32_t *t = gba_objAdd(kShape[best][0], kShape[best][1], tw * th, x + c0 * 8, y, prio);
        if (!t) return false;
        for (int ty = 0; ty < th; ty++)
            for (int tx = 0; tx < tw; tx++) {
                if (tx >= cw || ty * 8 >= h) { for (int k = 0; k < 8; k++) *t++ = 0; continue; }        // (outside the picture: clear)
                const uint32_t *src = e->data + ty * 8 * 2 * nw + c0 + tx;
                const int rows = h - ty * 8 < 8 ? h - ty * 8 : 8;
                int k = 0;
                for (; k < rows; k++, src += 2 * nw) *t++ = *src;
                for (; k < 8; k++) *t++ = 0;
            }
    }
    return true;
}

RAMFUNC(blitRd) static void new_blitRd(Rd &rd, int w, int h, int x, int y, bool flip, uint8_t solid, const uint8_t *map) {
    if (x >= SW || y >= SH || x + w <= 0 || y + h <= 0) return;
    uint32_t lut4[4];
    uint8_t *const lut = (uint8_t *)lut4;
    for (int k = 0; k < 14; k++) lut[k] = solid != 255 ? solid : map[k];
    lut[14] = lut[15] = 0;
    const GfxClip cl = gfx__clip;
    if (!cl.x0 && !cl.y0 && cl.x1 == SW && cl.y1 == SH && bcBlit(rd, w, h, x, y, flip, lut4)) return;   // (rd is not read again)
    const uint8_t *p = rd.p, *ps = rd.p, *pend = rd.fromCard ? rd.p + rd.left : nullptr;
    uint32_t off = rd.off;
    const int hx0 = cl.x0, hx1 = cl.x1;                                          // gfx_hline's columns
    const uint8_t *const clip = gba_clipY;                                       // patch 04: behind a hills layer
#pragma clang loop unroll(disable)
    for (int r = 0; r < h; r++) {
        const int yy = y + r;
        if ((unsigned)yy >= SH) {                                               // off screen: only read
#pragma clang loop unroll(disable)
            for (int c = 0; c < w;) { uint8_t b; BLR_NEXT(b); c += (b >> 4) + 1; }
            continue;
        }
        const bool hvis = (unsigned)(yy - cl.y0) < (unsigned)(cl.y1 - cl.y0);  // gfx_hline's rows
        uint8_t *const row = gfx_fb + yy * GFX_FB_STRIDE;
        // px: the edge of what is drawn so far (flipped: the left edge, moving left from x + w)
#pragma clang loop unroll(disable)
        for (int c = 0, px = flip ? x + w : x; c < w;) {
            uint8_t b; BLR_NEXT(b);
            const int n = (b >> 4) + 1, v = b & 15;
            c += n;
            const int x0 = flip ? px - n : px;
            px = flip ? x0 : px + n;
            if (v == 15) continue;
            const uint8_t col = lut[v];
            if (clip) {                                                          // (rare: sky objects, far structures)
                int a = x0 < 0 ? 0 : x0, e = x0 + n > SW ? SW : x0 + n;
                if (n > 2) { if (!hvis) continue; if (a < hx0) a = hx0; if (e > hx1) e = hx1; }
#pragma clang loop unroll(disable)
                for (int k = a; k < e; k++) {
                    if (yy >= clip[k]) continue;
                    uint8_t *q = row + (k >> 1);
                    *q = (k & 1) ? (uint8_t)((*q & 0x0F) | (col << 4)) : (uint8_t)((*q & 0xF0) | (col & 0x0F));
                }
            } else if (n > 2) {                                                  // gfx_hline(x0, yy, n, col)
                const int a = x0 < hx0 ? hx0 : x0, e = x0 + n > hx1 ? hx1 : x0 + n;
                if (hvis && a < e) blrSpan(row, a, e, (uint8_t)(col & 0x0F));
            } else {                                                             // pset(x0 + k, yy, col)
                const int e = x0 + n > SW ? SW : x0 + n;
#pragma clang loop unroll(disable)
                for (int k = x0 < 0 ? 0 : x0; k < e; k++) {
                    uint8_t *q = row + (k >> 1);
                    *q = (k & 1) ? (uint8_t)((*q & 0x0F) | (col << 4)) : (uint8_t)((*q & 0xF0) | col);
                }
            }
        }
    }
    blrDone(rd, p, ps, pend, off);
}

#if !defined(GBA_SCENECHECK) && !defined(GBA_BLITCHECK)
#ifdef GBA_PROF
// (GBA_PROF builds) time per sprite size: key w << 8 | h, microseconds, calls (frames.py prints the biggest)
extern "C" { volatile uint32_t gba_blitStat[64][3]; }
static void blitRd(Rd &rd, int w, int h, int x, int y, bool flip, uint8_t solid, const uint8_t *map) {
    const uint32_t t = micros(), key = (uint32_t)(w << 8 | h);
    new_blitRd(rd, w, h, x, y, flip, solid, map);
    const uint32_t d = micros() - t;
    for (int i = 0; i < 64; i++) if (gba_blitStat[i][0] == key || !gba_blitStat[i][0]) { gba_blitStat[i][0] = key; gba_blitStat[i][1] += d; gba_blitStat[i][2]++; break; }
}
#else
static inline void blitRd(Rd &rd, int w, int h, int x, int y, bool flip, uint8_t solid, const uint8_t *map) { new_blitRd(rd, w, h, x, y, flip, solid, map); }
#endif
#else
// The check builds (GBA_SCENECHECK, or GBA_BLITCHECK for this alone): the game's blitRd and this one from the same framebuffer and reader, compared (pixels and reader).
extern "C" { uint32_t gba_blChecks, gba_blBad, gba_blBadFirst; uint8_t gba_blSave[GFX_FB_BYTES] __attribute__((aligned(4))), gba_blRef[GFX_FB_BYTES] __attribute__((aligned(4))); }
static void blitRd(Rd &rd, int w, int h, int x, int y, bool flip, uint8_t solid, const uint8_t *map) {
    Rd r2 = rd;
    memcpy(gba_blSave, gfx_fb, GFX_FB_BYTES);
    old_blitRd(rd, w, h, x, y, flip, solid, map);
    memcpy(gba_blRef, gfx_fb, GFX_FB_BYTES);
    memcpy(gfx_fb, gba_blSave, GFX_FB_BYTES);
    new_blitRd(r2, w, h, x, y, flip, solid, map);
    gba_blChecks++;
    bool bad = false;                    // (pixels only: the decoded-sprite path leaves the reader as it was, and no caller reads it again)
    for (int i = 0; i < GFX_FB_BYTES && !bad; i++) bad = gfx_fb[i] != gba_blRef[i];
    if (bad && !gba_blBad++) gba_blBadFirst = (uint32_t)((w << 24) | (h << 16) | ((x & 255) << 8) | (y & 255));
}
#endif
