// CHGame on the GBA: start-up, clocks, the buttons, the interrupt dispatcher and the few C library bits the game needs.
#include <Arduino.h>
#include <chgame/Input.h>
#include <CHGfx.h>
#include "hw.h"

void setup();
void loop();
extern "C" void gba_main();

GbaSerial Serial;
uint32_t sim_ledState = 0;

// ---- time: timer 2 (262144 Hz) with timer 3 counting its overflows -----------------------------------------------
static inline uint32_t ticks() {
    uint16_t hi, lo;
    do { hi = REG_TM3CNT_L; lo = REG_TM2CNT_L; } while (hi != REG_TM3CNT_L);
    return (uint32_t)hi << 16 | lo;
}
uint32_t micros() { const uint32_t t = ticks(); return (t >> 12) * 15625u + (((t & 4095u) * 15625u) >> 12); }
static bool s_setupClock0;                       // test runs: millis() is 0 until setup() returns (see test_script)
uint32_t millis() { return s_setupClock0 ? 0 : micros() / 1000u; }
void delayMicroseconds(uint32_t us) { const uint32_t t0 = micros(); while (micros() - t0 < us) {} }
void delay(uint32_t ms) { delayMicroseconds(ms * 1000u); }

static uint32_t rngState = 12345;
long random(long howbig) { if (howbig <= 0) return 0; rngState = rngState * 1664525u + 1013904223u; return (long)((rngState >> 8) % (uint32_t)howbig); }
long random(long lo, long hi) { return lo + random(hi - lo); }
void randomSeed(unsigned long s) { rngState = (uint32_t)s; }

// ---- the buttons -------------------------------------------------------------------------------------------------
// A test hook for the emulator scripts (tools/frames.py). A script writes "CHGBATST" into the last 8 bytes of SRAM (no
// save uses them) and resets the GBA; start-up finds it, erases it (so it lasts one boot), and sets test_script:
//  - millis() is 0 until setup() returns: the game seeds its random numbers from it there, and setup() takes longer or
//    shorter in different builds;
//  - the buttons are a fixed pattern of the game's own frame count (not the emulator's), so every build plays exactly
//    the same frames however fast it draws;
//  - the frame numbered test_snapAt is copied to gba_snap as it is shown (here, at the start of the next frame).
// On a GBA the marker is not there (power-on SRAM holding it is a 1 in 2^64 chance) and none of this happens.
volatile uint32_t test_script, test_snapAt, test_snapDone;
uint8_t gba_snap[GFX_FB_BYTES] __attribute__((aligned(4)));
__attribute__((section(".ewtext.boot"), target("arm"), noinline)) static bool testBoot() {   // (in RAM: SRAM can't be read by code running from the cartridge)
    static const char M[8] = { 'C', 'H', 'G', 'B', 'A', 'T', 'S', 'T' };
    volatile uint8_t *const s = SRAM + 0x7FF8;
    for (int i = 0; i < 8; i++) if (s[i] != (uint8_t)M[i]) return false;
    for (int i = 0; i < 8; i++) s[i] = 0xFF;
    return true;
}
static uint8_t scriptButtons(uint32_t g) {
    if (g < 100) return 0;
    if (g < 750) return g % 40 < 3 ? A_BUTTON : 0;                               // through the title and the stage card
    static const uint8_t K[8] = { LEFT_BUTTON | A_BUTTON, RIGHT_BUTTON | A_BUTTON, A_BUTTON, B_BUTTON, UP_BUTTON,
                                  DOWN_BUTTON, LEFT_BUTTON | B_BUTTON, RIGHT_BUTTON | A_BUTTON };
    return K[(g / 22) % 8];                                                       // flying about (smoke.lua's pattern)
}

// the shoulder buttons, which CHGame does not have: StarLift Advance reads them itself (L: the gear, R: the charge shot)
// (both through the player's button map: controls_gba.cpp, read once a frame by chgame_readButtons)
uint8_t gba_remap(uint8_t *shoulders);
static uint8_t s_shoulders;
uint8_t gba_shoulders() { return test_script ? 0 : s_shoulders; }

uint8_t chgame_readButtons() {
    if (test_script) {
        const uint32_t g = chgame.frameCount;
        if (g - 1 == test_snapAt && !test_snapDone) { memcpy(gba_snap, gfx_fb, GFX_FB_BYTES); test_snapDone = 1; }
        return scriptButtons(g);
    }
    return gba_remap(&s_shoulders);
}

// START held 3 s (the CHGame menu on the board): a GBA has no menu, so start over (BIOS SoftReset).
// (the library's simulator build calls exit(0) for it)
extern "C" void exit(int) {
    REG_IME = 0;
    *(volatile uint8_t *)0x03007FFA = 0;
    asm volatile("swi 0");
    for (;;) {}
}

// ---- what the game's simulator hooks become: let time pass until the next interrupt ---------------------------------
void sim_sync() {}
extern volatile uint32_t gba_idleUs;
void sim_advance(uint32_t) { const uint32_t t = micros(); gba_halt(); gba_idleUs += micros() - t; }
void sim_waitInput() {}

// ---- interrupts --------------------------------------------------------------------------------------------------
// The BIOS pushes r0-r3, r12 and lr_irq before calling us, so the interrupted PC (+4) is at [sp, #20]: the 1 kHz timer
// tick records it, a sampling profiler for the emulator harness (tools/pcprof.py).
volatile uint32_t gba_pcN, gba_pcRing[4096];
asm(".section .iwram.text,\"ax\"\n"
    ".arm\n"
    ".global gba_irq\n"
    ".type gba_irq, %function\n"
    "gba_irq:\n"
    "  ldr r0, [sp, #20]\n"
    "  b gba_irq_c\n");
extern "C" void gba_irq();
extern "C" IWRAM_TEXT void gba_irq_c(uint32_t lr) {
    const uint16_t f = REG_IF & REG_IE;
    if (f & 16) gba_pcRing[gba_pcN++ & 4095] = lr - 4;
    if (f & 1) { gba_vblank(); gba_audioVblank(); }
    REG_IF = f;
    BIOS_IF |= f;
}
extern "C" void gba_irq();

extern "C" void gba_main() {
    REG_WAITCNT = 0x4014;                          // cartridge: 3/1 wait states, prefetch on
#ifndef GBA_EWRAM_SLOW
    REG32(0x04000800) = 0x0E000020;               // EWRAM: 1 wait state instead of 3 (GBA/SP hardware; ignored elsewhere)
#endif
    REG_TM2CNT_H = 0; REG_TM3CNT_H = 0; REG_TM2CNT_L = 0; REG_TM3CNT_L = 0;
    REG_TM3CNT_H = 0x84;                           // cascade, enable
    REG_TM2CNT_H = 0x81;                           // 262144 Hz (16.78 MHz / 64)
    REG_TM1CNT_L = 65536 - 262; REG_TM1CNT_H = 0xC1;   // a 1 kHz tick (interrupt) so that halting never oversleeps a frame
    IRQ_VECTOR = (uint32_t)(void *)gba_irq;
    gba_audioInit();
    REG_DISPSTAT = 0x0008;                         // VBlank interrupt
    REG_IE = 1 | (1 << 4);                         // VBlank, timer 1
    REG_IME = 1;
    chgame.lockstep = -1;                          // free-running (the simulator build starts paused)
    test_script = testBoot();                      // (an emulator test run: see test_script)
    s_setupClock0 = test_script;
    setup();
    s_setupClock0 = false;
    for (;;) loop();
}

// ---- C library bits (the game and CHGame use a handful) ---------------------------------------------------------------
extern "C" {
FILE *const stderr = nullptr;
int fprintf(FILE *, const char *, ...) { return 0; }
int printf(const char *, ...) { return 0; }
int snprintf(char *b, size_t n, const char *, ...) { if (n) b[0] = 0; return 0; }
int puts(const char *) { return 0; }
void abort() { exit(1); }
void __cxa_pure_virtual() { for (;;) {} }
int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
void *__dso_handle = nullptr;
}

#ifdef GBA_PROF
// the game's dbg::prof sections (build.py renames the calls): microseconds summed per slot, frames counted
extern "C" { volatile uint32_t gba_profSum[12], gba_profFrames; }
static uint32_t s_profT;
extern "C" void gba_profStart() {
    s_profT = micros(); gba_profFrames++;
}
extern "C" void gba_prof(unsigned char slot) { const uint32_t t = micros(); if (slot < 12) gba_profSum[slot] += t - s_profT; s_profT = t; }
#endif
