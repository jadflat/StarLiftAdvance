// Button remapping: every game control (the eight CHGame buttons, the gear and the charge shot) can be put on any GBA
// key. Hold L + R + SELECT (the real keys, whatever they are mapped to) to open the CONTROLS screen; the map is kept in
// SRAM at 0x7FE0 (16 bytes: "CHMP", 10 key numbers, a check byte), below the test marker and above the save pages.
#include <CHGfx.h>
#include <chgame/Input.h>
#include <stdint.h>
#include "hw.h"

enum { N_ACT = 10, N_KEY = 10 };
// the actions in CHGame's button order, then the shoulders; the default key for each is its GBA key bit
static const char *const ACT[N_ACT] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "GEAR", "CHARGE" };
static const uint8_t DEF[N_ACT] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8 };
static const char *const KEY[N_KEY] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN", "R", "L" };
static uint8_t keyOf[N_ACT] = { 0, 1, 2, 3, 4, 5, 6, 7, 9, 8 };
static bool loaded;

static const uint32_t AT = 0x7FE0;
static uint8_t check(const uint8_t *m) { uint8_t c = 0x5A; for (int i = 0; i < N_ACT; i++) c = (uint8_t)(c * 31 + m[i]); return c; }
// (in RAM: SRAM can't be read by code running from the cartridge)
__attribute__((section(".ewtext.remap"), target("arm"), noinline)) static void sramRead(uint8_t *d) {
    for (int i = 0; i < 16; i++) d[i] = SRAM[AT + i];
}
__attribute__((section(".ewtext.remap"), target("arm"), noinline)) static void sramWrite(const uint8_t *s) {
    for (int i = 0; i < 16; i++) SRAM[AT + i] = s[i];
}
static void load() {
    loaded = true;
    uint8_t b[16]; sramRead(b);
    if (b[0] != 'C' || b[1] != 'H' || b[2] != 'M' || b[3] != 'P' || b[14] != check(b + 4)) return;
    for (int i = 0; i < N_ACT; i++) if (b[4 + i] >= N_KEY) return;
    for (int i = 0; i < N_ACT; i++) keyOf[i] = b[4 + i];
}
static void store() {
    uint8_t b[16] = { 'C', 'H', 'M', 'P' };
    for (int i = 0; i < N_ACT; i++) b[4 + i] = keyOf[i];
    b[14] = check(b + 4); b[15] = 0;
    sramWrite(b);
}

static uint16_t raw() { return (uint16_t)(~REG_KEYINPUT & 0x3FF); }
static void frame() {   // wait for the next vertical blank
    while (REG_VCOUNT >= 160) {}
    while (REG_VCOUNT < 160) {}
}

static void draw(int sel, bool waiting) {
    gfx_wait();
    gfx_fillRect(0, 0, GFX_W, GFX_H, 7);
    gfx_text(84, 4, "CONTROLS", 1);
    for (int i = 0; i < N_ACT + 2; i++) {
        const int y = 18 + i * 11;
        const uint8_t c = i == sel ? 10 : 1;
        if (i == sel) gfx_text(40, y, ">", 10);
        if (i < N_ACT) {
            gfx_text(52, y, ACT[i], c);
            gfx_text(130, y, i == sel && waiting ? "PRESS A KEY" : KEY[keyOf[i]], c);
        } else gfx_text(52, y, i == N_ACT ? "DEFAULTS" : "DONE", c);
    }
    gfx_text(8, 150, "A: CHANGE  B: DONE  L+R+SELECT: HERE", 6);
    gfx_flushAsync();
}

static void screen() {
    int sel = 0;
    uint16_t held = raw();                 // keys down when it opened count only once let go
    for (;;) {
        bool waiting = false;
        uint16_t k;
        do { draw(sel, waiting); frame(); k = raw(); held &= k; k &= ~held; } while (!k);
        held |= k;
        if (k & 0x40) sel = sel ? sel - 1 : N_ACT + 1;
        else if (k & 0x80) sel = sel == N_ACT + 1 ? 0 : sel + 1;
        else if (k & 0x2) break;
        else if (k & 0x1) {
            if (sel == N_ACT + 1) break;
            if (sel == N_ACT) { for (int i = 0; i < N_ACT; i++) keyOf[i] = DEF[i]; continue; }
            waiting = true;                // the next key pressed (any, A included) is the new one
            do { draw(sel, waiting); frame(); k = raw(); held &= k; k &= ~held; } while (!k);
            held |= k;
            int b = 0; while (!(k & (1 << b))) b++;
            keyOf[sel] = (uint8_t)b;
        }
    }
    store();
    while (raw()) { draw(sel, false); frame(); }   // let go before the game sees anything
}

void gba_controls() { if (!loaded) load(); screen(); }   // from the OPTIONS menu (patch 11-options-controls)

// the game's buttons (CHGame's mask) and the shoulders (1: gear, 2: charge), through the map
uint8_t gba_remap(uint8_t *shoulders) {
    if (!loaded) load();
    static bool comboWas;
    const uint16_t k = raw();
    const bool combo = (k & 0x304) == 0x304;
    if (combo && !comboWas) { comboWas = true; screen(); *shoulders = 0; return 0; }
    comboWas = combo;
    if (combo) { *shoulders = 0; return 0; }
    uint8_t m = 0;
    static const uint8_t MASK[8] = { A_BUTTON, B_BUTTON, SELECT_BUTTON, START_BUTTON, RIGHT_BUTTON, LEFT_BUTTON, UP_BUTTON, DOWN_BUTTON };
    for (int i = 0; i < 8; i++) if (k & (1 << keyOf[i])) m |= MASK[i];
    *shoulders = (uint8_t)(((k >> keyOf[8]) & 1) | (((k >> keyOf[9]) & 1) << 1));
    return m;
}
