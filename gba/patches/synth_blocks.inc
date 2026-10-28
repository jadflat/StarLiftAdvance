// ---- GBA: patch 10-synth-blocks ------------------------------------------------------------------------------------
// synth::sample() keeps each voice's state in memory and reloads it every sample (about 105 cycles a voice on the
// ARM7). blocks::render() makes the same samples a block at a time: each voice runs alone over the samples between two
// events (an envelope tick or a step: at most ENV_DIV samples) with its state in registers, adding into a music
// accumulator, then passes do the drums, the engine, the effect and sample()'s mixing. The output is the mix after
// SYN_GAIN, before sample()'s knee and 8-bit clamp (gba_audio.cpp has its own); with those applied it is sample()'s
// output bit for bit (tools/synthcheck.py checks it on the PC).
//
// On the GBA (ARM state) the voice loops are hand-written (blk_voice, blk_voice2 below: about 26 cycles a voice a
// sample, 40 for two voices together, against 55 a voice from the compiler, which spilled) and read SIN() from a whole
// 256-entry table instead of folding the quarter wave. The C loops stay as the PC's version, which synthcheck.py
// compares with sample(); a GBA_SNDCHECK build compares the two on the GBA (gba_audio.cpp).
#if defined(__arm__) && !defined(__thumb__)
#define BLK_ASM 1
// blk_voice(acc, n >= 1, voice, drive, sin256): one voice's samples added into acc[0..n).
// blk_voice2(acc, n >= 1, voice a, voice b, sin256): two voices without drive, their sum stored into acc[0..n).
// ph and mph are kept as phase << 16 | increment, so one add steps a phase and keeps its increment; the top byte is
// SIN()'s index.
asm(R"(
    .pushsection .iwram.text.blkvoice,"ax",%progbits
    .arm
    .align 2
    .macro  BLK_LOAD c, ph, mph, t      @ a Voice's ph, mph and their increments
    ldrh    \ph, [\c, #0]
    ldrh    \t, [\c, #4]
    orr     \ph, \t, \ph, lsl #16
    ldrh    \mph, [\c, #2]
    ldrh    \t, [\c, #6]
    orr     \mph, \t, \mph, lsl #16
    .endm
    .macro  BLK_OSC ph, mph, eff, s, t, tab   @ s = SIN(ph >> 8 + (SIN(mph >> 8) * eff >> 5))
    add     \mph, \mph, \mph, lsl #16
    add     \ph, \ph, \ph, lsl #16
    mov     \s, \mph, lsr #24
    ldrsb   \s, [\tab, \s]
    mul     \t, \s, \eff
    mov     \t, \t, asr #5
    add     \t, \ph, \t, lsl #24
    mov     \t, \t, lsr #24
    ldrsb   \s, [\tab, \t]
    .endm
    .macro  BLK_DRIVE                   @ s * drive >> 4, softened above 100: 100 + (s - 100 >> 3) = s + 700 >> 3
    mul     r10, r9, r3
    mov     r9, r10, asr #4
    cmp     r9, #100
    addgt   r9, r9, #700
    movgt   r9, r9, asr #3
    cmn     r9, #100
    sublt   r9, r9, #700
    movlt   r9, r9, asr #3
    .endm
    .macro  BLK_ADD                     @ acc += s * vol >> 7
    mul     r10, r9, r8
    ldrsh   r9, [r0]
    add     r9, r9, r10, asr #7
    strh    r9, [r0], #2
    .endm
    .global blk_voice
    .type blk_voice, %function
blk_voice:
    push    {r4-r10, lr}
    ldr     r6, [sp, #32]               @ the sine table
    BLK_LOAD r2, r4, r5, r7
    ldrb    r7, [r2, #13]               @ eff
    ldrb    r8, [r2, #14]               @ vol
    cmp     r3, #16
    bhi     5f
2:  BLK_OSC r4, r5, r7, r9, r10, r6     @ no drive
    BLK_ADD
    subs    r1, r1, #1
    bne     2b
    b       9f
5:  BLK_OSC r4, r5, r7, r9, r10, r6     @ drive
    BLK_DRIVE
    BLK_ADD
    subs    r1, r1, #1
    bne     5b
9:  mov     r4, r4, lsr #16
    strh    r4, [r2, #0]
    mov     r5, r5, lsr #16
    strh    r5, [r2, #2]
    pop     {r4-r10, lr}
    bx      lr
    .size blk_voice, . - blk_voice

    .global blk_voice2
    .type blk_voice2, %function
blk_voice2:
    push    {r2-r11, lr}
    BLK_LOAD r2, r4, r5, r12
    ldrb    r6, [r2, #13]               @ a: ph r4, mph r5, eff r6, vol r7
    ldrb    r7, [r2, #14]
    BLK_LOAD r3, r8, r9, r12
    ldrb    r11, [r3, #13]              @ b: ph r8, mph r9, eff r11, vol r3
    ldrb    r3, [r3, #14]
    ldr     r10, [sp, #44]              @ the sine table
1:  BLK_OSC r4, r5, r6, r12, lr, r10
    mul     lr, r12, r7
    BLK_OSC r8, r9, r11, r12, r2, r10
    mul     r2, r12, r3
    mov     r2, r2, asr #7
    add     r2, r2, lr, asr #7
    strh    r2, [r0], #2
    subs    r1, r1, #1
    bne     1b
    ldmia   sp!, {r2, r3}
    mov     r4, r4, lsr #16
    strh    r4, [r2, #0]
    mov     r5, r5, lsr #16
    strh    r5, [r2, #2]
    mov     r8, r8, lsr #16
    strh    r8, [r3, #0]
    mov     r9, r9, lsr #16
    strh    r9, [r3, #2]
    pop     {r4-r11, lr}
    bx      lr
    .size blk_voice2, . - blk_voice2
    .popsection
)");
extern "C" void blk_voice(int16_t *acc, int n, void *voice, int drive, const int8_t *sin256);
extern "C" void blk_voice2(int16_t *acc, int n, void *a, void *b, const int8_t *sin256);
#else
#define BLK_ASM 0
#endif

namespace blocks {
#if BLK_ASM
alignas(4) int8_t sin256[256];                                       // SIN() whole (IWRAM .bss), made on the first call
__attribute__((noinline)) static void makeSin() { for (int i = 0; i < 256; i++) sin256[i] = (int8_t)SIN((uint8_t)i); }
#endif

static void voiceC(int16_t *acc, int seg, Voice &c, int dv) {      // the C version of blk_voice
    uint16_t ph = c.ph, mph = c.mph;
    const uint16_t inc = c.inc, minc = c.minc;
    const int eff = c.eff, vol = c.vol;
    if (dv > 16) {
        for (int k = 0; k < seg; k++) {
            ph += inc; mph += minc;
            int s = SIN((uint8_t)((uint8_t)((ph >> 8) + ((SIN((uint8_t)(mph >> 8)) * eff) >> 5))));
            s = (s * dv) >> 4; if (s > 100) s = 100 + ((s - 100) >> 3); else if (s < -100) s = -100 + ((s + 100) >> 3);
            acc[k] += (s * vol) >> 7;
        }
    } else {
        for (int k = 0; k < seg; k++) {
            ph += inc; mph += minc;
            const int s = SIN((uint8_t)((uint8_t)((ph >> 8) + ((SIN((uint8_t)(mph >> 8)) * eff) >> 5))));
            acc[k] += (s * vol) >> 7;
        }
    }
    c.ph = ph; c.mph = mph;
}

template <bool FX>                                                  // sample()'s mix, after the voices and the drums
static inline __attribute__((always_inline)) void mixOut(int16_t *o, const int16_t *acc, const int16_t *fxa, int seg, int fd, int mV, int fV) {
    for (int k = 0; k < seg; k++) {
        int mix = acc[k];
        if (fd < 255) mix = (mix * fd) >> 8;
        mix = (mix * mV) >> 2;
        if (FX) mix += (fxa[k] * fV) >> 2;
        o[k] = (int16_t)((mix * SYN_GAIN) >> 4);
    }
}

typedef uint32_t __attribute__((may_alias)) Pair;                   // clears two samples a store

static inline __attribute__((always_inline)) void renderT(int16_t *out, int n, bool useAsm) {
#if BLK_ASM
#define BLK_SIN(i) (useAsm ? (int)sin256[(uint8_t)(i)] : SIN((uint8_t)(i)))
#else
#define BLK_SIN(i) SIN((uint8_t)(i))
#endif
    alignas(4) int16_t acc[ENV_DIV + 1];
#if BLK_ASM
    if (useAsm && !sin256[64]) makeSin();
#endif
    int i = 0;
    while (i < n) {
        if (++envCnt >= ENV_DIV) { envCnt = 0; envTick(); }
        if (cur && --stepCnt == 0) { stepCnt = stepLen; nextStep(); }
        int seg = ENV_DIV - envCnt;                                 // this sample and the ones before the next event
        if (cur && stepCnt < seg) seg = stepCnt;
        if (n - i < seg) seg = n - i;
        envCnt = (uint16_t)(envCnt + seg - 1);
        if (cur) stepCnt = (uint16_t)(stepCnt - (seg - 1));
        int first = 0;                                              // the voices already in acc
#if BLK_ASM
        if (useAsm) {                                               // the first two sounding voices without drive, together
            int a = -1, b = -1;
            for (int vi = 0; vi < 3; vi++) if (v[vi].vol && ins[vi].drive <= 16) { if (a < 0) a = vi; else if (b < 0) b = vi; }
            if (b >= 0) { blk_voice2(acc, seg, &v[a], &v[b], sin256); first = 1 << a | 1 << b; }
        }
#endif
        if (!first) for (int k = 0; k < (seg + 1) >> 1; k++) ((Pair *)(void *)acc)[k] = 0;
        for (int vi = 0; vi < 3; vi++) {
            Voice &c = v[vi];
            if (!c.vol || (first >> vi & 1)) continue;
#if BLK_ASM
            if (useAsm) { blk_voice(acc, seg, &c, ins[vi].drive, sin256); continue; }
#endif
            voiceC(acc, seg, c, ins[vi].drive);
        }
        // The rest of sample(), unchanged, in passes over the block like the voices (in one loop its state spilled): the
        // drums into acc, the engine and the effect into fxa, then the mix. The drums' noise and a noise effect share
        // dr.lfsr: with both, the drum pass steps the effect too, in sample()'s order.
        const int drVol = dr.vol, eV = engV, sAmp = sf.amp, sKind = sf.kind;
        alignas(4) int16_t fxa[ENV_DIV + 1];
        bool sfDone = !sAmp;
        if (eV) {
            uint16_t x = engLfsr;
            int16_t lp = engLp;
            for (int k = 0; k < seg; k++) {
                x ^= x << 7; x ^= x >> 9; x ^= x << 8;
                lp += (int16_t)((((int)(x >> 8) - 128) - lp) >> 3);
                fxa[k] = (int16_t)((lp * eV) >> 7);
            }
            engLfsr = x; engLp = lp;
        } else if (sAmp) {
            for (int k = 0; k < (seg + 1) >> 1; k++) ((Pair *)(void *)fxa)[k] = 0;
        }
        if (drVol) {
            uint16_t ph = dr.ph, x = dr.lfsr;
            const uint16_t inc = dr.inc;
            const int kind = dr.kind;
            if (kind == 0) {
                for (int k = 0; k < seg; k++) { ph += inc; acc[k] += (BLK_SIN(ph >> 8) * drVol) >> 8; }
            } else if (sAmp && sKind == 2) {
                uint16_t sp = sf.ph;
                const uint16_t si = sf.inc;
                int8_t nz = sf.nz;
                for (int k = 0; k < seg; k++) {
                    ph += inc;
                    x ^= x << 7; x ^= x >> 9; x ^= x << 8;
                    int m = acc[k] + ((((int)(x >> 8) - 128) * drVol) >> 7);
                    if (kind == 1) m += (BLK_SIN(ph >> 8) * drVol) >> 7;
                    acc[k] = (int16_t)m;
                    const uint16_t was = sp;
                    sp += si;
                    if (sp < was) { x ^= x << 7; x ^= x >> 9; x ^= x << 8; nz = (int8_t)(x >> 8); }
                    fxa[k] += (nz * sAmp) >> 7;
                }
                sf.ph = sp; sf.nz = nz; sfDone = true;
            } else {
                for (int k = 0; k < seg; k++) {
                    ph += inc;
                    x ^= x << 7; x ^= x >> 9; x ^= x << 8;
                    int m = acc[k] + ((((int)(x >> 8) - 128) * drVol) >> 7);
                    if (kind == 1) m += (BLK_SIN(ph >> 8) * drVol) >> 7;
                    acc[k] = (int16_t)m;
                }
            }
            dr.ph = ph; dr.lfsr = x;
        }
        if (!sfDone) {
            uint16_t sp = sf.ph;
            const uint16_t si = sf.inc;
            if (sKind == 1) {
                for (int k = 0; k < seg; k++) { sp += si; fxa[k] += (BLK_SIN(sp >> 8) * sAmp) >> 7; }
            } else if (sKind == 2) {
                uint16_t x = dr.lfsr;
                int8_t nz = sf.nz;
                for (int k = 0; k < seg; k++) {
                    const uint16_t was = sp;
                    sp += si;
                    if (sp < was) { x ^= x << 7; x ^= x >> 9; x ^= x << 8; nz = (int8_t)(x >> 8); }
                    fxa[k] += (nz * sAmp) >> 7;
                }
                dr.lfsr = x; sf.nz = nz;
            } else {
                for (int k = 0; k < seg; k++) { sp += si; fxa[k] += (sp & 0x8000) ? sAmp : -sAmp; }
            }
            sf.ph = sp;
        }
        int16_t *o = out + i;
        const int fd = fade, mV = musV, fV = fxV;
        if (eV || sAmp) mixOut<true>(o, acc, fxa, seg, fd, mV, fV); else mixOut<false>(o, acc, fxa, seg, fd, mV, fV);
        i += seg;
    }
}

void render(int16_t *out, int n) { renderT(out, n, BLK_ASM); }

#ifdef GBA_SNDCHECK
// The check build: renderRef() is render() with the C voice loops; save() and restore() let gba_audio.cpp run both
// from the same state.
void renderRef(int16_t *out, int n) { renderT(out, n, false); }
static struct State {
    Voice v[3]; uint8_t lfo; Drum dr; uint8_t musV, fxV, engT, engV; uint16_t engLfsr; int16_t engLp; Sfx sf;
    const uint8_t *cur; int8_t needId; uint8_t sweepNow; int8_t curId, pendingId; uint8_t fade; int8_t fadeDir; uint8_t pos;
    uint16_t stepCnt, stepLen, envCnt;
} s_saved;
#define BLK_STATE(X) X(lfo) X(dr) X(musV) X(fxV) X(engT) X(engV) X(engLfsr) X(engLp) X(sf) X(cur) X(sweepNow) X(curId) \
    X(pendingId) X(fade) X(fadeDir) X(pos) X(stepCnt) X(stepLen) X(envCnt)
void save() {
    for (int k = 0; k < 3; k++) s_saved.v[k] = v[k];
    s_saved.needId = needId;
#define BLK_SAVE(f) s_saved.f = f;
    BLK_STATE(BLK_SAVE)
}
void restore() {
    for (int k = 0; k < 3; k++) v[k] = s_saved.v[k];
    needId = s_saved.needId;
#define BLK_LOAD(f) f = s_saved.f;
    BLK_STATE(BLK_LOAD)
}
// blk_voice against voiceC on made-up voices, every drive, odd and even lengths (the songs a run plays may not use
// them all): the number that differ.
int selfTest() {
    if (!sin256[64]) makeSin();
    uint32_t r = 12345;
    auto rnd = [&r]() { r = r * 1664525u + 1013904223u; return r >> 8; };
    int bad = 0;
    for (int t = 0; t < 4000; t++) {
        Voice a = {};
        a.ph = (uint16_t)rnd(); a.mph = (uint16_t)rnd(); a.inc = (uint16_t)rnd(); a.minc = (uint16_t)rnd();
        a.eff = (uint8_t)rnd(); a.vol = (uint8_t)(rnd() | 1);
        Voice b = a;
        const int dv = (t & 1) ? (int)(rnd() & 255) : (int)(rnd() & 31), n = 1 + (int)(rnd() % ENV_DIV);
        int16_t x[ENV_DIV + 1], y[ENV_DIV + 1];
        for (int k = 0; k <= ENV_DIV; k++) x[k] = y[k] = (int16_t)rnd();
        blk_voice(x, n, &a, dv, sin256);
        voiceC(y, n, b, dv);
        bool d = a.ph != b.ph || a.mph != b.mph;
        for (int k = 0; k <= ENV_DIV; k++) d |= x[k] != y[k];
        Voice a2 = a, b2 = b;                                       // and blk_voice2 with a second voice
        a2.ph = (uint16_t)rnd(); a2.mph = (uint16_t)rnd(); a2.inc = (uint16_t)rnd(); a2.minc = (uint16_t)rnd();
        a2.eff = (uint8_t)rnd(); a2.vol = (uint8_t)rnd();
        b2 = a2;
        for (int k = 0; k < n; k++) y[k] = 0;
        blk_voice2(x, n, &a, &a2, sin256);
        voiceC(y, n, b, 0);
        voiceC(y, n, b2, 0);
        d |= a.ph != b.ph || a.mph != b.mph || a2.ph != b2.ph || a2.mph != b2.mph;
        for (int k = 0; k <= ENV_DIV; k++) d |= x[k] != y[k];
        bad += d;
    }
    return bad;
}
#endif
}  // namespace blocks
