// GBA, patch 05-structure-obj (included into sketch.cpp after blit(const CSpr &)): a card picture as a hardware sprite.
// The same pixels as blit(c, x, y, flip, 255, map) for a picture whose pixels are not clipped (the full-screen clip
// rectangle); false: do that instead. The sprite's priority puts it where the picture's pixels were: in front of the
// background layers behind it, behind the ones in front and the picture (bcObj, gfx_gba.cpp).
static bool blitObj(const CSpr &s, int x, int y, bool flip, const uint8_t *map, int prio) {
    if (!card::ok() || x >= SW || x + s.w <= 0 || y >= SH || y + s.h <= 0) return true;   // (nothing to draw: as blit)
    const GfxClip cl = gfx__clip;
    if (cl.x0 || cl.y0 || cl.x1 != SW || cl.y1 != SH) return false;
    if (!card::at(CARD_PIC + s.off)) return true;
    uint32_t lut4[4];
    uint8_t *const lut = (uint8_t *)lut4;
    for (int k = 0; k < 14; k++) lut[k] = map[k];
    lut[14] = lut[15] = 0;
    Rd rd = { nullptr, CARD_PIC + s.off, 0, true };
    return bcObj(rd, s.w, s.h, x, y, flip, lut4, prio);
}

// GBA, patch 17-sky-obj: the sky object (a planet, a sun, moons) as a priority 3 sprite: behind every background layer
// (the hills cover it as gba_clipY did) and the picture (what is drawn later still covers it). Pixels drawn before it (the
// stars) where its pixels would have gone (opaque, above the clip) are cleared, as the blit would have covered them.
extern "C" { uint8_t gba_starsOn, gba_starRow[GFX_H]; }                        // (EWRAM) stars were drawn this frame; the rows they are in
int gba_skyBegin(const uint32_t *key); void gba_skyFail(); volatile uint32_t *gba_skyTiles(int off); bool gba_skySpr(int shape, int size, int x, int y, int off);
__attribute__((noinline)) static bool skyObj(const CSpr &s, int x, int y, const uint8_t *map) {
    if (!card::ok() || x >= SW || x + s.w <= 0 || y >= SH || y + s.h <= 0) return true;
    const GfxClip cl = gfx__clip;
    if (cl.x0 || cl.y0 || cl.x1 != SW || cl.y1 != SH || !card::at(CARD_PIC + s.off)) return false;
    uint32_t lut4[4];
    uint8_t *const lut = (uint8_t *)lut4;
    for (int k = 0; k < 14; k++) lut[k] = map[k];
    lut[14] = lut[15] = 0;
    Rd rd = { nullptr, CARD_PIC + s.off, 0, true };
    const BcEnt *e = bcGet(rd, s.w, s.h, false, lut4);
    if (!e) return false;
    const int nw = e->nw, y0 = y < 0 ? 0 : y, y1 = y + s.h > SH ? SH : y + s.h, w0 = x < 0 ? 0 : x >> 3, w1 = ((x + s.w > SW ? SW : x + s.w) + 7) >> 3;
    const uint8_t *const clip = gba_clipY;
    uint8_t cmin[FB_NGROUPS], cmax[FB_NGROUPS];                     // per 8 columns: the clip's lowest and highest row
    int ry = y0;
    for (int i = w0; i < w1; i++) {
        int lo = 255, hi = 255;
        if (clip) { lo = 255; hi = 0; for (int j = 0; j < 8; j++) { const int c = clip[i * 8 + j]; if (c > hi) hi = c; if (c < lo) lo = c; } }
        cmin[i] = (uint8_t)lo; cmax[i] = (uint8_t)hi; if (hi > ry) ry = hi;
    }
    const int sh = (x & 7) * 4, wx = x >> 3;                                    // (word i holds the picture's words i - wx and i - wx - 1)
    // (patch 18: above the hills only the stars are drawn before this, so only their rows are looked at, and none when
    // there are no stars; without a hills clip, every row as before)
    const bool stars = gba_starsOn || !clip;
    gba_starsOn = 0;
    if (stars) for (int r = y0; r < y1 && r < ry; r++) {
        if (clip && !gba_starRow[r]) continue;
        uint32_t *const q = FB_WORDS + FB_WY(r);
        const uint32_t *const m = e->data + 2 * e->h + e->h + (r - y);                // (column k's mask: m[k * 2h]; patch 02's pool is column-major)
        for (int i = w0; i < w1; i++) {
            if (!q[FB_GX(i)] || r >= cmax[i]) continue;
            const int k = i - wx;
            uint32_t v = (unsigned)k < (unsigned)nw ? m[k * 2 * e->h] << sh : 0;
            if (sh && (unsigned)(k - 1) < (unsigned)nw) v |= m[(k - 1) * 2 * e->h] >> (32 - sh);
            if (r >= cmin[i]) { uint32_t open = 0; for (int j = 0; j < 8; j++) if (r < clip[i * 8 + j]) open |= 15u << (j * 4); v &= open; }
            q[FB_GX(i)] &= ~v;
        }
    }
    const uint32_t key[5] = { rd.off, lut4[0], lut4[1], lut4[2], lut4[3] };
    const int fill = gba_skyBegin(key);
    if (fill < 0) return false;
    static const uint8_t kShape[12][4] = {                                      // (as bcObj)
        {0, 0, 1, 1}, {1, 0, 2, 1}, {2, 0, 1, 2}, {0, 1, 2, 2}, {1, 1, 4, 1}, {2, 1, 1, 4},
        {1, 2, 4, 2}, {2, 2, 2, 4}, {0, 2, 4, 4}, {1, 3, 8, 4}, {2, 3, 4, 8}, {0, 3, 8, 8} };
    const int nh = (s.h + 7) >> 3;
    int off = 0;
    for (int c0 = 0; c0 < nw; c0 += 8) {
        const int cw = nw - c0 < 8 ? nw - c0 : 8;
        int best = -1;
        for (int k = 0; k < 12; k++)
            if (kShape[k][2] >= cw && kShape[k][3] >= nh && (best < 0 || kShape[k][2] * kShape[k][3] < kShape[best][2] * kShape[best][3])) best = k;
        if (best < 0 || off + kShape[best][2] * kShape[best][3] > 96 || !gba_skySpr(kShape[best][0], kShape[best][1], x + c0 * 8, y, off)) { gba_skyFail(); return false; }
        const int tw = kShape[best][2], th = kShape[best][3];
        if (fill) {
            volatile uint32_t *t = gba_skyTiles(off);
            for (int ty = 0; ty < th; ty++)
                for (int tx = 0; tx < tw; tx++) {
                    if (tx >= cw || ty * 8 >= s.h) { for (int k = 0; k < 8; k++) *t++ = 0; continue; }
                    const uint32_t *src = e->data + (c0 + tx + 1) * 2 * e->h + ty * 8;
                    const int rows = s.h - ty * 8 < 8 ? s.h - ty * 8 : 8;
                    int k = 0;
                    for (; k < rows; k++) *t++ = *src++;
                    for (; k < 8; k++) *t++ = 0;
                }
        }
        off += tw * th;
    }
    return true;
}
