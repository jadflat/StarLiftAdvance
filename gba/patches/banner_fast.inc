// GBA, patch 19-banner-fast (included into sketch.cpp before drawWorld): the big call-out, text35x2's pixels for less work.
// text35x2 draws every font pixel as a gfx_fillRect of 2x2 from the cartridge, every frame the call-out is up: about 3 ms
// for a long one. Here the call-out's pixels are worked out once, when its text (or place) changes, as a mask a font row
// (the framebuffer's own words: 8 pixels, the same nibble order), and each frame stores the colour through the masks,
// each font row twice. The same pixels: the blocks are one colour, so their order does not matter. Anything unusual (a
// clip rectangle, a line break, off the picture) goes to text35x2 as before.
#include "CHGfx_internal.h"
extern "C" { uint32_t gba_banM[8][GFX_FB_STRIDE / 4]; char gba_banS[32]; int16_t gba_banX; uint8_t gba_banW0, gba_banW1; }   // (EWRAM)
static void gbaBanner(int x, int y, const char *s, uint8_t c) {
    const GfxClip cl = gfx__clip;
    int n = 0;
    bool ok = !cl.x0 && !cl.y0 && cl.x1 == SW && cl.y1 == SH && x >= 0 && y >= 0 && y + 16 <= SH;
    for (; s[n]; n++) if (s[n] == '\n') ok = false;
    if (!ok || n > 31 || x + n * 8 > SW) { text35x2(x, y, s, c); return; }
    if (x != gba_banX || strcmp(s, gba_banS)) {                                // a new call-out: its pixels, once
        strcpy(gba_banS, s); gba_banX = (int16_t)x;
        memset(gba_banM, 0, sizeof gba_banM);
        int px = x;
        for (const char *t = s; *t; t++) {
            if (*t == '~') { px += 4; continue; }
            const uint8_t *g = glyph35(*t);
            if (g)
                for (int i = 0; i < 3; i++)
                    for (int r = 0, bits = g[i]; bits; r++, bits >>= 1)
                        if (bits & 1) gba_banM[r][(px + 2 * i) >> 3] |= 15u << (((px + 2 * i) & 7) * 4), gba_banM[r][(px + 2 * i + 1) >> 3] |= 15u << (((px + 2 * i + 1) & 7) * 4);
            px += 8;
        }
        gba_banW0 = (uint8_t)(x >> 3); gba_banW1 = (uint8_t)((px + 7) >> 3);
    }
    const uint32_t cw = (c & 15) * 0x11111111u;
    for (int r = 0; r < 8; r++) {
        const uint32_t *const m = gba_banM[r];
        uint32_t *q = (uint32_t *)gfx_fb + (y + 2 * r) * (GFX_FB_STRIDE / 4);
        for (int i = gba_banW0; i < gba_banW1; i++)
            if (const uint32_t v = m[i]) { q[i] = (q[i] & ~v) | (cw & v); q[i + GFX_FB_STRIDE / 4] = (q[i + GFX_FB_STRIDE / 4] & ~v) | (cw & v); }
    }
}
