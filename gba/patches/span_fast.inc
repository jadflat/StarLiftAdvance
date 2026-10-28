// GBA: a horizontal run of one colour, pixels [x0, x1) of a framebuffer row, for the framebuffer in EWRAM.
// EWRAM is a 16-bit bus at 2-3 cycles an access: a byte store costs as much as a halfword, a word is two accesses, and
// the loop around each store costs more again. So: the ragged nibble ends, then a byte and a halfword to reach a word
// boundary, then words (8 at a time by store multiple on long runs), then a halfword, a byte and a nibble.
// The same pixels as CHGfx's spanBody. Included by 03-span-fast (CHGfx_draw.cpp) and blit_runs.inc.
#ifndef GBA_SPAN_FAST_INC
#define GBA_SPAN_FAST_INC
#include <stdint.h>
static inline __attribute__((always_inline, target("arm"))) void gbaSpan(uint8_t *row, int x0, int x1, uint8_t c) {
    if (x1 <= x0) return;
    uint8_t *p = row + (x0 >> 1);
    if (x0 & 1) { *p = (uint8_t)((*p & 0x0F) | (c << 4)); p++; x0++; }
    int n = (x1 - x0) >> 1;                                         // whole bytes (pixel pairs)
    if (n < 4) {                                                    // short (most sprite runs): the plain byte loop
        const uint8_t pair = (uint8_t)(c | (c << 4));
#pragma clang loop unroll(disable)
        for (; n; n--) *p++ = pair;
        if ((x1 - x0) & 1) *p = (uint8_t)((*p & 0xF0) | c);
        return;
    }
    const uint32_t quad = (uint32_t)(c | (c << 4)) * 0x01010101u;
    if (n >= 1 && ((uintptr_t)p & 1)) { *p++ = (uint8_t)quad; n--; }
    if (n >= 2 && ((uintptr_t)p & 2)) { *(uint16_t *)p = (uint16_t)quad; p += 2; n -= 2; }
    if (n >= 32) {
        uint32_t k = (uint32_t)n >> 5;
        n &= 31;
        asm volatile("mov r4, %[w]\n mov r5, %[w]\n mov r6, %[w]\n mov r7, %[w]\n"
                     "1: stmia %[d]!, {r4-r7}\n stmia %[d]!, {r4-r7}\n subs %[k], %[k], #1\n bne 1b"
                     : [d] "+r"(p), [k] "+r"(k) : [w] "r"(quad) : "r4", "r5", "r6", "r7", "memory", "cc");   // (not r11: the frame pointer)
    }
#pragma clang loop unroll(disable)
    for (; n >= 4; n -= 4, p += 4) *(uint32_t *)p = quad;
    if (n >= 2) { *(uint16_t *)p = (uint16_t)quad; p += 2; n -= 2; }
    if (n) *p++ = (uint8_t)quad;
    if ((x1 - x0) & 1) *p = (uint8_t)((*p & 0xF0) | c);
}
#endif
