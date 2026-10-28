// memcpy and friends for the freestanding build. Word-at-a-time when aligned; in IWRAM as ARM (they are the hottest calls).
#include <stddef.h>
#include <stdint.h>
#include "hw.h"
extern "C" {
IWRAM_TEXT __attribute__((optimize("no-tree-loop-distribute-patterns"))) void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *dp = (uint8_t *)d; const uint8_t *sp = (const uint8_t *)s;
    if ((((uintptr_t)dp | (uintptr_t)sp) & 3) == 0) {
        uint32_t *dw = (uint32_t *)dp; const uint32_t *sw = (const uint32_t *)sp;
        for (; n >= 16; n -= 16) { uint32_t a = sw[0], b = sw[1], c = sw[2], e = sw[3]; dw[0] = a; dw[1] = b; dw[2] = c; dw[3] = e; dw += 4; sw += 4; }
        for (; n >= 4; n -= 4) *dw++ = *sw++;
        dp = (uint8_t *)dw; sp = (const uint8_t *)sw;
    }
    while (n--) *dp++ = *sp++;
    return d;
}
IWRAM_TEXT __attribute__((optimize("no-tree-loop-distribute-patterns"))) void *memmove(void *d, const void *s, size_t n) {
    uint8_t *dp = (uint8_t *)d; const uint8_t *sp = (const uint8_t *)s;
    if (dp <= sp || dp >= sp + n) return memcpy(d, s, n);
    dp += n; sp += n;
    while (n--) *--dp = *--sp;
    return d;
}
IWRAM_TEXT __attribute__((optimize("no-tree-loop-distribute-patterns"))) void *memset(void *d, int c, size_t n) {
    uint8_t *dp = (uint8_t *)d;
    while (n && ((uintptr_t)dp & 3)) { *dp++ = (uint8_t)c; n--; }
    uint32_t w = (uint8_t)c; w |= w << 8; w |= w << 16;
    uint32_t *dw = (uint32_t *)dp;
    if (n >= 32) {                          // 8 words a store multiple: the framebuffer is in EWRAM, where each access costs
        uint32_t k = n >> 5;
        n &= 31;
        asm volatile("mov r4, %[w]\n mov r5, %[w]\n mov r6, %[w]\n mov r7, %[w]\n"
                     "1: stmia %[d]!, {r4-r7}\n stmia %[d]!, {r4-r7}\n subs %[k], %[k], #1\n bne 1b"
                     : [d] "+r"(dw), [k] "+r"(k) : [w] "r"(w) : "r4", "r5", "r6", "r7", "memory", "cc");   // (not r11: the frame pointer)
    }
    for (; n >= 16; n -= 16) { dw[0] = w; dw[1] = w; dw[2] = w; dw[3] = w; dw += 4; }
    for (; n >= 4; n -= 4) *dw++ = w;
    dp = (uint8_t *)dw;
    while (n--) *dp++ = (uint8_t)c;
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (uint8_t)*a - (uint8_t)*b; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
}

// ---- the ARM EABI memory helpers the compiler calls (division is in div.S) -------------------
extern "C" {
void __aeabi_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove8(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset8(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr8(void *d, size_t n) { memset(d, 0, n); }
}
