// GBA: a horizontal run of one colour, pixels [x0, x1) of a framebuffer row (FB_ROW), for the column-major framebuffer in
// VRAM: a masked word store for each ragged end, a plain word store for each whole group of 8 pixels between.
// The same pixels as CHGfx's spanBody. Included by 03-span-fast (CHGfx_draw.cpp) and blit_runs.inc.
#ifndef GBA_SPAN_FAST_INC
#define GBA_SPAN_FAST_INC
#include <stdint.h>
static inline __attribute__((always_inline, target("arm"))) void gbaSpan(uint8_t *row, int x0, int x1, uint8_t c) {
    if (x1 <= x0) return;
    uint32_t *const w = (uint32_t *)row;                            // FB_ROW(y): group g's word is w[g * 160]
    const uint32_t cc = (uint32_t)c * 0x11111111u;
    const int g0 = x0 >> 3, g1 = (x1 - 1) >> 3;
    const uint32_t m0 = ~0u << ((x0 & 7) * 4), m1 = ~0u >> ((7 - ((x1 - 1) & 7)) * 4);
    if (g0 == g1) { const uint32_t m = m0 & m1; w[g0 * 160] = (w[g0 * 160] & ~m) | (cc & m); return; }
    w[g0 * 160] = (w[g0 * 160] & ~m0) | (cc & m0);
    uint32_t *q = w + (g0 + 1) * 160;
#pragma clang loop unroll(disable)
    for (int g = g0 + 1; g < g1; g++, q += 160) *q = cc;
    w[g1 * 160] = (w[g1 * 160] & ~m1) | (cc & m1);
}
#endif
