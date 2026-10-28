// Sound on the GBA: the game's own software synth (Synth.cpp, with patch 10-synth-blocks) drives both Direct Sound
// FIFOs as one 9-bit channel. SOUND-PLAN.md has the measurements behind each choice.
//
// Each VBlank, blocks::render() makes the frame's 134 samples at SYN_RATE (8004 Hz here: 16777216 / 2096), the same
// samples synth::sample() would make, as the signed mix before the synth's knee and 8-bit clamp. The piezo needs that
// knee to be loud; on the GBA it clipped 9-25% of all samples. Here one synth unit is half a FIFO step: the mix plays
// 6 dB quieter than before, with room for its peaks, and only a gentle knee above 220 is left. Each sample becomes four
// by linear interpolation (32017 Hz, timer 0 at 524 cycles), which takes away most of the 8 kHz steps' fizz.
// FIFO A at 100% plays y >> 1 and FIFO B at 50% the half step y & 1, both to both speakers: 9 bits, mono.
//
// DMA 1 and 2 feed the FIFOs from the buffers mixed the frame before. A FIFO DMA reads up to 32 bytes ahead, so at
// each VBlank the FIFOs hold bytes from past the end of the buffer that just played: they are emptied, filled with the
// first 16 samples of the next buffer by hand, and the DMA carries on from there, so the stream has no seam. The padding
// after each buffer repeats its last sample, for the odd sample played before the restart.
//
// The pass after the synth (echo, knee, interpolation) is hand-written ARM: gba_audMix, two samples a turn. mixC is
// what it does. A GBA_SNDCHECK build runs both, and the synth's C voice loops against its ARM ones, from the same state
// every VBlank, and counts the VBlanks that differ (gba_sndBad).
#include <Arduino.h>
#include "hw.h"

namespace synth { uint8_t sample(); void begin(); }
namespace blocks { void render(int16_t *out, int n); }

#define REG_DMA2SAD   REG32(0x040000C8)
#define REG_DMA2DAD   REG32(0x040000CC)
#define REG_DMA2CNT_H REG16(0x040000D2)
#define REG_FIFO_A    REG32(0x040000A0)
#define REG_FIFO_B    REG32(0x040000A4)

enum { N = 134, Q = 4 * N, PERIOD = 524, PAD = 32, KNEE = 220, LIM = 254 };
enum : uint16_t { SND_H = 0x3304, SND_RESET = 0x8800 };   // A 100%, B 50%, both speakers, timer 0; | reset both FIFOs
alignas(4) int8_t gba_audA[2][Q + PAD], gba_audB[2][Q + PAD];   // EWRAM (gba.ld: .bss.gba_*)
int16_t gba_audX[N];
uint8_t gba_audCur;                             // the buffer mixed last (tools/sounddump.py reads it)
// Star Lift GBA FS: a soft echo, for space: 125 ms (1000 samples at 8 kHz) later, at about 28%, feeding back at 35%.
// GBA_ECHO=0 builds it without.
#ifndef GBA_ECHO
#define GBA_ECHO 1
#endif
enum { ECHO_N = 1000, ECHO_OUT = 72, ECHO_FB = 90 };          // /256
int16_t gba_audEcho[ECHO_N];                    // EWRAM (gba.ld: .bss.gba_*), cleared at start-up by crt0
struct Mix { int16_t *echo; int prev; };        // the echo's next sample, the last sample
static Mix s_mix = { gba_audEcho, 0 };

// x[0..N) to N words for each FIFO. In IWRAM: without the echo it is what gba_audioVblank runs (with it, unused).
__attribute__((section(".iwram.text.mixc"), target("arm"), noinline)) static void mixC(const int16_t *x, uint32_t *oa, uint32_t *ob, Mix &m) {
    int p = m.prev;
    int16_t *ep = m.echo;
    for (int i = 0; i < N; i++) {
        int v = x[i];
#if GBA_ECHO
        const int e = *ep;
        *ep = (int16_t)(v + ((e * ECHO_FB) >> 8));
        if (++ep == gba_audEcho + ECHO_N) ep = gba_audEcho;
        v += (e * ECHO_OUT) >> 8;
#endif
        if (v > KNEE) { v = KNEE + ((v - KNEE) >> 2); if (v > LIM) v = LIM; }
        else if (v < -KNEE) { v = -KNEE + ((v + KNEE) >> 2); if (v < -LIM) v = -LIM; }
        const int d = v - p, p4 = p * 4;              // p, p + d/4, p + d/2, p + 3d/4
        const int y0 = p, y1 = (p4 + d) >> 2, y2 = (p4 + 2 * d) >> 2, y3 = (p4 + 3 * d) >> 2;
        oa[i] = (uint32_t)((y0 >> 1) & 255) | (uint32_t)((y1 >> 1) & 255) << 8 | (uint32_t)((y2 >> 1) & 255) << 16 | (uint32_t)(y3 >> 1) << 24;
        ob[i] = (uint32_t)(y0 & 1) | (uint32_t)(y1 & 1) << 8 | (uint32_t)(y2 & 1) << 16 | (uint32_t)(y3 & 1) << 24;
        p = v;
    }
    m.prev = p; m.echo = ep;
}

#if GBA_ECHO && defined(__arm__)
// gba_audMix(x, oa, ob, &s_mix) = mixC(x, oa, ob, s_mix), about 57 cycles a sample against 72. With t = 4p + k * d
// (k = 0..3, y_k = t >> 2), FIFO A's byte is bits 3-10 of t and FIFO B's bit 2 of t; 90e >> 8 = 45e >> 7 and
// 72e >> 8 = 9e >> 5, all shifts and adds. N and ECHO_N are even, so the echo can wrap after each pair.
static_assert(N == 134 && ECHO_N == 1000 && ECHO_OUT == 72 && ECHO_FB == 90 && KNEE == 220 && LIM == 254, "gba_audMix's constants");
extern "C" void gba_audMix(const int16_t *x, uint32_t *oa, uint32_t *ob, Mix *m);
asm(R"(
    .pushsection .iwram.text.audmix,"ax",%progbits
    .arm
    .align 2
    .global gba_audMix
    .type gba_audMix, %function
gba_audMix:
    push    {r3-r11, lr}
    ldr     r4, [r3]                    @ echo
    ldr     r5, [r3, #4]                @ p
    add     r11, r0, #268               @ the end of x
    ldr     r12, 8f                     @ the end of the echo
    mov     r3, #0x7f0
    orr     r3, r3, #8                  @ FIFO A's bits of t
    .macro  MIX_ONE p, v, knee, back
    ldrsh   \v, [r0], #2
    ldrsh   r7, [r4]                    @ e
    add     r8, r7, r7, lsl #2
    add     r8, r8, r8, lsl #3          @ 45e
    add     r8, \v, r8, asr #7
    strh    r8, [r4], #2                @ the echo: v + (90e >> 8)
    add     r7, r7, r7, lsl #3          @ 9e
    add     \v, \v, r7, asr #5          @ v + (72e >> 8)
    cmp     \v, #220
    bgt     \knee
    cmn     \v, #220
    blt     \knee
\back:
    sub     r7, \v, \p                  @ d
    add     r8, r7, \p, lsl #2          @ t1
    mov     r10, \p, lsl #23
    mov     r10, r10, lsr #24           @ A: (y0 >> 1) & 255
    and     lr, \p, #1                  @ B: y0 & 1
    and     r9, r8, r3
    orr     r10, r10, r9, lsl #5
    and     r9, r8, #4
    orr     lr, lr, r9, lsl #6
    add     r8, r8, r7                  @ t2
    and     r9, r8, r3
    orr     r10, r10, r9, lsl #13
    and     r9, r8, #4
    orr     lr, lr, r9, lsl #14
    add     r8, r8, r7                  @ t3
    mov     r9, r8, lsr #3
    orr     r10, r10, r9, lsl #24
    and     r9, r8, #4
    orr     lr, lr, r9, lsl #22
    str     r10, [r1], #4
    str     lr, [r2], #4
    .endm
    .macro  MIX_KNEE v, back            @ 220 + (v - 220 >> 2) = v + 660 >> 2, at most 254; the same below -220
    bgt     1f
    sub     \v, \v, #660
    mov     \v, \v, asr #2
    cmn     \v, #254
    mvnlt   \v, #253
    b       \back
1:  add     \v, \v, #660
    mov     \v, \v, asr #2
    cmp     \v, #254
    movgt   \v, #254
    b       \back
    .endm
0:  MIX_ONE r5, r6, 2f, 3
    MIX_ONE r6, r5, 4f, 5
    cmp     r4, r12
    subeq   r4, r4, #2000
    cmp     r0, r11
    bne     0b
    pop     {r3}
    str     r4, [r3]
    str     r5, [r3, #4]
    pop     {r4-r11, lr}
    bx      lr
2:  MIX_KNEE r6, 3b
4:  MIX_KNEE r5, 5b
8:  .word   gba_audEcho + 2000
    .size gba_audMix, . - gba_audMix
    .popsection
)");
#define AUD_MIX(x, oa, ob) gba_audMix(x, oa, ob, &s_mix)
#else
#define AUD_MIX(x, oa, ob) mixC(x, oa, ob, s_mix)
#endif

// Without patch 10 (build.py --nopatch): the game's own 8-bit samples, at the level they had before.
__attribute__((weak)) void blocks::render(int16_t *out, int n) { for (int i = 0; i < n; i++) out[i] = (int16_t)((synth::sample() - 128) * 2); }

#ifdef GBA_SNDCHECK
// The check build (GBA_CFLAGS=-DGBA_SNDCHECK): each VBlank the C versions (blocks::renderRef, mixC) mix first, from a
// copy of all the state; the state goes back, the real path mixes, and everything must agree. At start-up both ARM
// loops are also tried against C on made-up input (gba_sndSelfBad). Runs from ROM: slow.
namespace blocks { void renderRef(int16_t *out, int n); void save(); void restore(); int selfTest(); }
volatile uint32_t gba_sndChecks, gba_sndBad, gba_sndBadFirst, gba_sndSelfBad = ~0u;
int16_t gba_chkX[N], gba_chkEcho[ECHO_N];       // EWRAM (gba.ld: .bss.gba_*)
uint32_t gba_chkA[N], gba_chkB[N], gba_chkTA[N], gba_chkTB[N];
static Mix s_chkMix;
static void sndCheckRef() {
    blocks::save();
    for (int i = 0; i < ECHO_N; i++) gba_chkEcho[i] = gba_audEcho[i];
    s_chkMix = s_mix;
    blocks::renderRef(gba_chkX, N);
    mixC(gba_chkX, gba_chkA, gba_chkB, s_chkMix);
    blocks::restore();
    for (int i = 0; i < ECHO_N; i++) { const int16_t e = gba_chkEcho[i]; gba_chkEcho[i] = gba_audEcho[i]; gba_audEcho[i] = e; }   // keep C's echo, put the old one back
}
static void sndCheckCompare(const uint32_t *oa, const uint32_t *ob) {
    bool bad = s_mix.prev != s_chkMix.prev || s_mix.echo != s_chkMix.echo;
    for (int i = 0; i < N; i++) bad |= gba_audX[i] != gba_chkX[i] || oa[i] != gba_chkA[i] || ob[i] != gba_chkB[i];
    for (int i = 0; i < ECHO_N; i++) bad |= gba_audEcho[i] != gba_chkEcho[i];
    if (bad && !gba_sndBad++) gba_sndBadFirst = gba_sndChecks;
    gba_sndChecks++;
}
static void sndSelfTest() {                     // gba_audMix against mixC: loud input (the knee and the limit), any echo
    uint32_t r = 777, bad = 0;
#if GBA_ECHO
    auto rnd = [&r]() { r = r * 1664525u + 1013904223u; return (int)(r >> 8); };
    for (int t = 0; t < 300; t++) {
        const int amp = 64 << (t % 6);           // 64 .. 2048
        for (int i = 0; i < N; i++) gba_chkX[i] = (int16_t)(rnd() % (2 * amp + 1) - amp);
        for (int i = 0; i < ECHO_N; i++) gba_chkEcho[i] = gba_audEcho[i] = (int16_t)(rnd() % (2 * amp + 1) - amp);
        Mix m1 = { gba_audEcho + 2 * (rnd() % (ECHO_N / 2)), rnd() % 509 - 254 }, m2 = m1;
        gba_audMix(gba_chkX, gba_chkTA, gba_chkTB, &m1);
        for (int i = 0; i < ECHO_N; i++) { const int16_t e = gba_chkEcho[i]; gba_chkEcho[i] = gba_audEcho[i]; gba_audEcho[i] = e; }
        mixC(gba_chkX, gba_chkA, gba_chkB, m2);
        bool d = m1.prev != m2.prev || m1.echo != m2.echo;
        for (int i = 0; i < N; i++) d |= gba_chkTA[i] != gba_chkA[i] || gba_chkTB[i] != gba_chkB[i];
        for (int i = 0; i < ECHO_N; i++) d |= gba_audEcho[i] != gba_chkEcho[i];
        bad += d;
    }
    for (int i = 0; i < ECHO_N; i++) gba_audEcho[i] = 0;
#endif
    gba_sndSelfBad = bad + 1000 * blocks::selfTest();   // mix + 1000 x voice
}
#endif

void gba_audioInit() {
    REG_SOUNDCNT_X = 0x0080;                    // master enable
    REG_SOUNDCNT_L = 0;
    REG_SOUNDCNT_H = SND_H | SND_RESET;
    REG_TM0CNT_H = 0;
    REG_TM0CNT_L = (uint16_t)(65536 - PERIOD);
    for (int b = 0; b < 2; b++) for (int i = 0; i < Q + PAD; i++) { gba_audA[b][i] = 0; gba_audB[b][i] = 0; }   // crt0 leaves EWRAM as it is
#ifdef GBA_SNDCHECK
    sndSelfTest();
#endif
    REG_TM0CNT_H = 0x0080;
}

volatile uint32_t gba_audUs;                    // total time spent mixing, for the harness

IWRAM_TEXT void gba_audioVblank() {
    const uint32_t t0 = micros();
    REG_DMA1CNT_H = 0; REG_DMA2CNT_H = 0;
    REG_TM0CNT_H = 0;
    REG_SOUNDCNT_H = SND_H | SND_RESET;         // empty both FIFOs: what is left was read past the end of the last buffer
    const uint32_t *a = (const uint32_t *)(void *)gba_audA[gba_audCur], *b = (const uint32_t *)(void *)gba_audB[gba_audCur];
    for (int k = 0; k < 4; k++) { REG_FIFO_A = a[k]; REG_FIFO_B = b[k]; }
    REG_DMA1SAD = (uint32_t)(a + 4); REG_DMA1DAD = 0x040000A0; REG_DMA1CNT_H = 0xB640;   // dest fixed, repeat, 32 bit, FIFO timing, on
    REG_DMA2SAD = (uint32_t)(b + 4); REG_DMA2DAD = 0x040000A4; REG_DMA2CNT_H = 0xB640;
    REG_TM0CNT_L = (uint16_t)(65536 - PERIOD);
    REG_TM0CNT_H = 0x0080;                      // in step with the frame
    gba_audCur ^= 1;

    uint32_t *oa = (uint32_t *)(void *)gba_audA[gba_audCur], *ob = (uint32_t *)(void *)gba_audB[gba_audCur];
#ifdef GBA_SNDCHECK
    sndCheckRef();
#endif
    blocks::render(gba_audX, N);
    AUD_MIX(gba_audX, oa, ob);
#ifdef GBA_SNDCHECK
    sndCheckCompare(oa, ob);
#endif
    const uint32_t la = oa[N - 1] >> 24, lb = ob[N - 1] >> 24;
    for (int i = N; i < N + PAD / 4; i++) { oa[i] = la * 0x01010101u; ob[i] = lb * 0x01010101u; }
    gba_audUs += micros() - t0;
}

// ---- keep the interrupt out of the synth while the game changes its state ---------------------------------------------
namespace audio { struct Effect; }
extern "C" {
void _ZN5synth5musicEi(int);
void _ZN5synth3sfxERKN5audio6EffectE(const void *);
void _ZN5synth6loadedEv();
void __real__ZN5synth5musicEi(int);
void __real__ZN5synth3sfxERKN5audio6EffectE(const void *);
void __real__ZN5synth6loadedEv();
void __wrap__ZN5synth5musicEi(int s) { const uint32_t o = irq_off(); __real__ZN5synth5musicEi(s); irq_restore(o); }
void __wrap__ZN5synth3sfxERKN5audio6EffectE(const void *e) { const uint32_t o = irq_off(); __real__ZN5synth3sfxERKN5audio6EffectE(e); irq_restore(o); }
void __wrap__ZN5synth6loadedEv() { const uint32_t o = irq_off(); __real__ZN5synth6loadedEv(); irq_restore(o); }
}
