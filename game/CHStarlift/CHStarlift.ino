// STARLIFT: an original lander rescue game for CHGame.
//
// Colonists are stranded across eight worlds. Fly a rocket lander on Newtonian physics (turn it, burn the main
// engine against gravity, drop the gear and touch down softly), set down beside the colonists so they can run aboard,
// and carry them to the base pad at the far end of each 15-screen stage, which refuels the lander. Flying saucers
// beam colonists up in green tractor beams; stingers spit plasma; mines drift; turrets fire flak. All code, art and
// music are original.
//
// The world is 1920 px wide and one screen tall: the ground is a height per 4 px. Behind it, three parallax layers
// (1/8, 1/4 and 1/2 of the camera's speed) are height functions drawn column by column, each stopping where the
// layer in front starts, with ruins, bases or alien towers standing on them. Sprites are HOUSE-palette pixel art
// (tools/make_sprites.py) mapped onto this game's 16 colours.
#include <CHGame.h>
#include "Sprites.h"
#include "Synth.h"
#include "Game.h"
#include "Card.h"
#include "CardIndex.h"
#include "Music.h"
#include <chgame/Draw.h>
#include <chgame/RamFunc.h>

#define SL_VERSION "0.28"
#define NOINL __attribute__((noinline))                     // small helpers called from many places: one copy saves flash

// HOUSE index -> game colour
static const uint8_t MAP[14] = { C_INK, C_WHITE, C_GREEN, C_GREEN, C_WHITE, C_SILVER, C_RED, C_RED, C_GOLD, C_STEEL, C_ACC, C_STEEL, C_GOLD, C_ACC };
static uint8_t EMAP[14];
// a copy of MAP with some entries changed: up to four bytes of (HOUSE index << 4 | game colour), lowest first
static NOINL void remap(uint8_t *mp, uint32_t p) { memcpy(mp, MAP, 14); for (; p; p >>= 8) mp[(p >> 4) & 15] = p & 15; }
#define RM(i, c) ((uint32_t)((i) << 4 | (c)))                                   // the same for the aliens: each world (and sector) paints its saucers and creatures its own way

// ---- sound: 1950s science fiction. Warbling sines for ray guns and saucers, noise for blasts ------------------
enum Sfx : uint8_t { S_LASER, S_BOOM, S_CRASH, S_HIT, S_BOARD, S_SAVED, S_LAND, S_UFO, S_BEAM, S_TAKEN, S_ZAP, S_FUEL, S_LOW,
                     S_START, S_CLEAR, S_OVER, S_CATCH, S_SQUASH, S_GEAR, S_WARN, S_SLURP, S_ORDER, S_JET, S_PAUSE, S_RESUME, S_GUN, S_WORM, S_CLANG, S_BLAST, S_MOO, S_COUNT };
// every effect's steps, one after another; an effect is where its steps start, how many, and its priority and flags
AUDIO_STEPS(SFX_STEPS) = {
    AUDIO_STEP(2600, 700, 110), AUDIO_STEP(1400, 240, 280), AUDIO_STEP(1200, 160, 480), AUDIO_STEP(300, 60, 500),
    AUDIO_STEP(320, 120, 100), AUDIO_STEP(660, 990, 60), AUDIO_STEP(1320, 0, 60), AUDIO_STEP(600, 1800, 160),
    AUDIO_STEP(1568, 0, 70), AUDIO_STEP(2093, 0, 70), AUDIO_STEP(2637, 0, 220), AUDIO_STEP(240, 80, 70),
    AUDIO_STEP(1200, 1800, 80), AUDIO_STEP(600, 1000, 300), AUDIO_STEP(1000, 600, 300), AUDIO_STEP(380, 640, 110),
    AUDIO_STEP(640, 380, 110), AUDIO_STEP(380, 640, 110), AUDIO_STEP(640, 380, 110), AUDIO_STEP(1500, 200, 500),
    AUDIO_STEP(200, 100, 200), AUDIO_STEP(1500, 400, 70), AUDIO_STEP(1800, 2200, 24), AUDIO_STEP(1600, 0, 80),
    AUDIO_REST(60), AUDIO_STEP(1600, 0, 80), AUDIO_STEP(400, 1400, 320), AUDIO_STEP(1400, 1400, 200),
    AUDIO_STEP(784, 0, 90), AUDIO_STEP(1047, 0, 90), AUDIO_STEP(1319, 0, 90), AUDIO_STEP(1568, 2093, 360),
    AUDIO_STEP(1200, 150, 500), AUDIO_STEP(150, 60, 400), AUDIO_STEP(900, 1800, 140), AUDIO_STEP(300, 100, 140),
    AUDIO_STEP(500, 900, 90), AUDIO_STEP(900, 0, 30), AUDIO_STEP(2200, 0, 50), AUDIO_REST(50),
    AUDIO_STEP(2200, 0, 50), AUDIO_STEP(1000, 160, 260), AUDIO_STEP(300, 520, 90), AUDIO_STEP(1319, 0, 140),
    AUDIO_STEP(1047, 0, 260), AUDIO_STEP(700, 1100, 180), AUDIO_STEP(1400, 500, 160), AUDIO_STEP(500, 1400, 160),
    AUDIO_STEP(1100, 500, 34), AUDIO_STEP(240, 80, 420), AUDIO_STEP(1980, 1860, 30), AUDIO_STEP(1240, 1180, 150),
    AUDIO_STEP(200, 1600, 90), AUDIO_STEP(1600, 300, 260), AUDIO_STEP(210, 150, 420) };
struct SfxAt { uint8_t at, n, flags; };
static const SfxAt SOUNDS[S_COUNT] = {
    { 0, 1, (uint8_t)(2 | synth::WOBBLE) },   // LASER
    { 1, 1, (uint8_t)(5 | synth::NOISE) },   // BOOM
    { 2, 2, (uint8_t)(9 | synth::NOISE) },   // CRASH
    { 4, 1, (uint8_t)(6) },   // HIT
    { 5, 2, (uint8_t)(4 | synth::WOBBLE) },   // BOARD
    { 7, 4, (uint8_t)(6) },   // SAVED
    { 11, 2, (uint8_t)(5) },   // LAND
    { 13, 2, (uint8_t)(3 | synth::WOBBLE) },   // UFO
    { 15, 4, (uint8_t)(2 | synth::WOBBLE) },   // BEAM
    { 19, 2, (uint8_t)(7 | synth::WOBBLE) },   // TAKEN
    { 21, 1, (uint8_t)(1 | audio::SOFT) },   // ZAP
    { 22, 1, (uint8_t)(1 | audio::SOFT) },   // FUEL
    { 23, 3, (uint8_t)(3) },   // LOW
    { 26, 2, (uint8_t)(6 | synth::WOBBLE) },   // START
    { 28, 4, (uint8_t)(8 | synth::WOBBLE) },   // CLEAR
    { 32, 2, (uint8_t)(9 | synth::WOBBLE) },   // OVER
    { 34, 1, (uint8_t)(6 | synth::WOBBLE) },   // CATCH
    { 35, 1, (uint8_t)(6) },   // SQUASH
    { 36, 2, (uint8_t)(3) },   // GEAR
    { 38, 3, (uint8_t)(4) },   // WARN
    { 41, 2, (uint8_t)(5 | synth::WOBBLE) },   // SLURP
    { 43, 2, (uint8_t)(3) },   // ORDER
    { 45, 1, (uint8_t)(2 | synth::NOISE | audio::SOFT) },   // JET
    { 46, 1, (uint8_t)(9 | synth::WOBBLE) },   // PAUSE
    { 47, 1, (uint8_t)(9 | synth::WOBBLE) },   // RESUME
    { 48, 1, (uint8_t)(1 | synth::NOISE | audio::SOFT) },   // GUN
    { 49, 1, (uint8_t)(3 | synth::NOISE) },   // WORM
    { 50, 2, (uint8_t)(6) },   // CLANG
    { 52, 2, (uint8_t)(8 | synth::WOBBLE) },   // BLAST
    { 54, 1, (uint8_t)(6 | synth::WOBBLE) }    // MOO
};
static NOINL void sfx(uint8_t id) { const SfxAt &f = SOUNDS[id]; const audio::Effect e = { SFX_STEPS + f.at, f.n, f.flags }; synth::sfx(e); }

// ---- the planets ------------------------------------------------------------------------------------------------
//                 SKY0           SKY1             FAR              MID              NEAR            GNDD            GND              GNDL             ACC
// the pictures this world draws, from its pack on the card (tools/make_card.py), the ones every frame needs first
struct Pack { CSpr sky, giant, plant[3][2], crit[2][2], st[6], shop, food, base, coll, mother; };
static Pack PK;
static Planet PLR;                                         // the world being shown, loaded from the card (CARD_PLANET)
static_assert(sizeof(Planet) == PLANET_LEN, "Planet must match tools/make_card.py");

static void adBoard(int x, int y, const char *t, uint32_t h);
static void drawPlants();
static inline uint8_t shMax();
static void menuList(const uint8_t *it, int n, int sel, int x, int y);
static bool busyAt(int x);
static void drawCritters();
static void throwGore(int sx, int sy);
static void drawBag(int x, int y);
static void drawBeam(int ux, int uy, int gy);
static void panel(int y, int h);
static void flag(int x, int gy, uint8_t kind);
static void drawAlienBase();
static void drawDiner();

// ---- state ------------------------------------------------------------------------------------------------------
#ifdef SL_SLIM                                     // a board perf build: a little RAM back for the debug image
enum { ESHOTS = 8, PARTS = 40, WXN = 48, CRITS = 4, MEN = 18 };
#else
#ifdef CHGBA
enum { ESHOTS = 12, PARTS = 64, WXN = 48 * (GFX_W + 32) / 160, CRITS = 6, MEN = 20 };   // the GBA: twice the sparks
#else
enum { ESHOTS = 12, PARTS = 32, WXN = 48 * (GFX_W + 32) / 160, CRITS = 6, MEN = 20 };
#endif
#endif
#ifdef CHGBA                                       // StarLift Advance: the GBA's extras (gba/src)
uint8_t gba_shoulders();                           // L (1) and R (2) held
void gba_mosaic(uint8_t frames);                   // the picture breaks into blocks for a moment, settling back
#endif
// the screen: the CHGame board's 128x128, or another size the library is built for (StarLift Advance: 240x160)
enum { SW = GFX_W, SH = GFX_H, WXR = SW + 32 > 255 ? 255 : SW + 32 };   // WXR: the weather's band across (it wraps)
// a taller screen: the world sits WDY rows lower; HUDX of them make the HUD (and its radar) taller, the rest more sky
enum { WDY = SH - 128, HUDX = WDY / 4, SKYX = WDY - HUDX };
// the menus and panels keep their 128x128 layout, centred on a bigger screen
enum { UX = (SW - 128) / 2, UY = (SH - 128) / 2 };
enum { W = 1920, HN = W / 4 + 1, TOP = 16 + HUDX, CEIL = TOP + 4, ENS = 12, SHOTS = 10,
       SEATS = 8, FUEL_MAX = 1500, ZONES = 8 };
static uint8_t hgt[HN];                     // the ground's top (screen y) every 4 px
#ifdef CHGBA
static uint8_t hgtGen = 1;                  // GBA: counts genStage()s, so the scanner's ground line is worked out once a stage
#endif
static uint8_t tile[16][16];                // the ground's texture by column (it repeats every 16 px), 0 crack .. 3 lit, built for each planet
static Man men[MEN];
static uint8_t nMen, nSaved, nLost, quota;                // quota: how many must reach the base
enum : uint8_t { IT_SUPPLY, IT_REPAIR, IT_FUEL, IT_ENGINE };
struct Item { int16_t x; uint8_t kind, st; };              // st 0 none, 1 waiting on the ground, 2 aboard (supplies)
static Item items[8];
static uint8_t engineUp, missed;                           // engine upgrades this game (each +8% thrust); the last stage missed its quota
static Enemy en[ENS];
static Shot shots[SHOTS];
static EShot es[ESHOTS];
static Part pt[PARTS];
static int16_t zoneX[ZONES];
static uint8_t nZones;
struct Boom { int16_t x, y; uint8_t t, r, delay; };
static Boom booms[8];
struct Pop { int16_t x, y; uint16_t v; uint8_t t; };
static Pop pops[4];

static State state;
static int stage, planet, sector, score, hiScore, lives, nextLife;
static bool baseRight;
static int startPadX, basePadX, buildX, doorX;          // pads: the left end of a 40-px slab; the base building's left
static int alienX, burgerX;                               // the aliens' bucket (its left); the Moon's diner (its left, -1 elsewhere)
static uint8_t ticket, tubeT;                             // the diner's next order number; the bucket's tubes glowing
struct Critter { int16_t x, y; int8_t vx; uint8_t kind, t; };   // background wildlife: x px, y px
static Critter crit[CRITS];

static int frameNo, timer, cam, shake, lightT, endT, warnT, spawnT;
static int windV;                                         // the wind's push on the lander, 1/256 px a frame
static uint32_t rng = 1;
static uint8_t wxLvl;
static int topsAt[4] = { -9999, -9999, -9999, -9999 };       // the layer x of column 0 when the tops were made (-9999: none)
                                     // this stage's weather, rolled when it is built: 0 clear .. 3 a storm
static bool loadReq;                                      // the stage's world is to be read from the card and the stage built

// the lander: position 1/256 px, velocity 1/256 px a frame
static int32_t lx, ly;
static int16_t lvx, lvy;
static int8_t ang, facing = 1;
static uint16_t fuel;
static uint8_t seats, shield, lst, gear, rotT, deadT, fireCd, gunCd, onPad, unloadT, invT, dryT, thrLvl;   // thrLvl: the engine spooling, 0-6   // lst: 0 flying, 1 landed, 2 wrecked
static bool thrusting;

static uint8_t banner, bannerT, bannerNew;   // bannerNew: its text is to be read from the card (once, not every frame)
static char bannerTxt[28];                            // a message (MS_...) shown for bannerT frames
static int8_t bannerNum = -1;                              // a number after it (-1: none)
static uint8_t tripN, killT, kills, flashT;
static bool hov;                                                  // the lander hovering over a cow or a worm this frame
static uint8_t dlv, dlvT, lidT, partyT, foodOn, feasts, cowsN, crashN, stars, hoverT, ropeL, patT, spitT, tankT, angryT, angryE, tiltPh;   // tiltPh: its rocking; angryT: a saucer robbed of its order sees red (angryE: which)   // hoverT: hovering over a cow; ropeL: the empty rope, winding up; crashN: landers lost this stage   // feasts: this stage's; cowsN: cows grazing at its end                // dlv: 0 none, 1 the bag falls, 2 the speeder fetches it, 3 brings it, 4 tosses it, 5 zooms off
static int bagX, bagY, scX, scY;                                // px (the speeder's: its middle, the ground under it)
static int8_t scDir = 1;
static uint8_t tripNx, bumpT, charge;                          // charge: frames UP has been held (CHARGED: ready)
enum { CHARGED = 15 };                                   // bumpT: frames until a bump can clang again               // colonists home this landing; the kill chain; the screen flash
#ifndef SL_DIFF
#define SL_DIFF 1
#endif
static uint8_t diff = SL_DIFF;                                   // 0 easy, 1 normal, 2 hard (saved)
#define dE (diff + 1)                                            // the difficulty in play, a step up: easy plays as the old normal, hard is harder still
static uint8_t titleSel, pauseSel, helpPage, optSel, musicV = 4, fxV = 4;
static int gSaved, overT;                                 // colonists home this game; the game-over screen's clock
static bool newHi;
static uint8_t pauseNote;                                  // a message (MS_...), 0 = none

static uint32_t rnd32() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static NOINL int rnd(int n) { return (int)(rnd32() % (uint32_t)n); }
static NOINL uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline int absi(int v) { return v < 0 ? -v : v; }
static inline int sgn(int v) { return (v > 0) - (v < 0); }
static NOINL int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static NOINL int isin(int a) { a &= 255; int b = a & 127, v = (b * (128 - b)) >> 5; return a < 128 ? v : -v; }   // a parabola sine, -128..128
static int isqrtTab(int v) { if (v <= 0) return 0; int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }   // small values only
static inline int LX() { return lx >> 8; }
static inline int LY() { return ly >> 8; }
static NOINL int groundAt(int x) {
    if (x <= 0) return hgt[0];
    if (x >= W) return hgt[HN - 1];
    const int i = x >> 2, f = x & 3;
    return hgt[i] + (((hgt[i + 1] - hgt[i]) * f) >> 2);
}
static const int16_t SIN7[7] = { 0, 66, 128, 181, 222, 247, 256 };       // sin of 0, 15 .. 90 degrees, x 256
static inline int sinA(int a) { return a < 0 ? -SIN7[-a] : SIN7[a]; }
static inline int cosA(int a) { return SIN7[6 - absi(a)]; }

// ---- drawing primitives -------------------------------------------------------------------------------------------
RAMFUNC(pset) static void pset(int x, int y, uint8_t c) {                         // in RAM: every single pixel goes through it
    if ((unsigned)x >= SW || (unsigned)y >= SH) return;
    uint8_t *b = gfx_fb + y * GFX_FB_STRIDE + (x >> 1);
    *b = (x & 1) ? (uint8_t)((*b & 0x0F) | (c << 4)) : (uint8_t)((*b & 0xF0) | c);
}
#define R gfx_fillRect
static NOINL void hl(int x, int y, int w, uint8_t c) { if (w > 0) gfx_hline(x, y, w, c); }

// Run-length rows from flash or from the card. solid: 255 = the sprite's colours, else one colour for every pixel (a hit flash)
RAMFUNC(blitRd) static void blitRd(Rd &rd, int w, int h, int x, int y, bool flip, uint8_t solid, const uint8_t *map) {
    if (x >= SW || y >= SH || x + w <= 0 || y + h <= 0) return;
    uint8_t lut[16];
    for (int k = 0; k < 14; k++) lut[k] = solid != 255 ? solid : map[k];
    for (int r = 0; r < h; r++) {
        const int yy = y + r;
        const bool vis = (unsigned)yy < SH;
        for (int c = 0; c < w;) {
            const uint8_t b = rd.next();
            const int n = (b >> 4) + 1, v = b & 15;
            if (vis && v != 15) {
                const int x0 = flip ? x + w - c - n : x + c;
                if (n > 2) gfx_hline(x0, yy, n, lut[v]);
                else for (int k = 0; k < n; k++) pset(x0 + k, yy, lut[v]);
            }
            c += n;
        }
    }
}
static NOINL void blit(const Spr &s, int x, int y, bool flip, uint8_t solid = 255, const uint8_t *map = MAP) { Rd rd = { s.d, 0, 0, false }; blitRd(rd, s.w, s.h, x, y, flip, solid, map); }
static NOINL void blitBC(const Spr &s, int cx, int by, bool flip, uint8_t solid = 255, const uint8_t *map = MAP) { blit(s, cx - s.w / 2, by - s.h + 1, flip, solid, map); }
static NOINL void blit(const CSpr &s, int x, int y, bool flip, uint8_t solid = 255, const uint8_t *map = MAP) {    // from the card (nothing without one)
#ifdef SL_NOPICS
    if (s.w != 96 && s.w != 108) return;                           // a perf test: card pictures off (the logos stay)
#endif
    if (!card::ok() || x >= SW || x + s.w <= 0 || y >= SH || y + s.h <= 0) return;
    if (!card::at(CARD_PIC + s.off)) return;                       // (scenery past this frame's card budget: next frame)
    Rd rd = { nullptr, CARD_PIC + s.off, 0, true };
    blitRd(rd, s.w, s.h, x, y, flip, solid, map);
}
static NOINL void blitBC(const CSpr &s, int cx, int by, bool flip, uint8_t solid = 255, const uint8_t *map = MAP) { blit(s, cx - s.w / 2, by - s.h + 1, flip, solid, map); }
// text with a number in it, without printf (which costs about 2 KB of flash): fmt(buf, T(MS_SCORE), n, T(MS_PTS))
static NOINL char *fmt(char *d, const char *a, int v = -1, const char *b = "", int v2 = -1, const char *c = "") {
    char *o = d;
    auto str = [&](const char *t) { while (*t) *o++ = *t++; };
    auto num = [&](int n) { char tmp[11]; int k = 0; if (n < 0) { *o++ = '-'; n = -n; } do { tmp[k++] = (char)('0' + n % 10); n /= 10; } while (n); while (k) *o++ = tmp[--k]; };
    str(a); if (v >= 0) num(v); str(b); if (v2 >= 0) num(v2); str(c);
    *o = 0;
    return d;
}
// a message from the card's table (MS_... in CardIndex.h): two bytes of offset, then the text. Only while drawing.
// The text is copied out (two buffers in turn), as a busy frame may reuse the card block it came from before it is drawn.
static NOINL const char *T(uint8_t id) {
    static char buf[2][32];
    static uint8_t turn;
    char *d = buf[turn ^= 1];
    d[0] = 0;
    const uint8_t *p = card::at(CARD_MSG + id * 2u);
    if (p) {
        const uint32_t off = CARD_MSG + (uint32_t)(p[0] | p[1] << 8);
        if (const uint8_t *q = card::at(off)) { uint8_t k = 0; while (k < 31 && q[k]) { d[k] = (char)q[k]; k++; } d[k] = 0; }
    }
    return d;
}
// all the text is the small 3 x 5 font (the call-outs at twice its size): one font in flash, not two
static NOINL void textC(int y, const char *s, uint8_t c) { text35((SW - text35Width(s)) / 2, y + UY + 1, s, c); }
static NOINL void textSh(int x, int y, const char *s, uint8_t c) { text35s(x, y + 1, s, c, C_INK); }
static NOINL void textCS(int y, const char *s, uint8_t c) { textSh((SW - text35Width(s)) / 2, y + UY, s, c); }

// ---- palette ------------------------------------------------------------------------------------------------------
static const uint8_t PAL_SLOT[9] = { C_SKY0, C_SKY1, C_FAR, C_MID, C_NEAR, C_GNDD, C_GND, C_GNDL, C_ACC };
static void applyPalette() {
    const Planet &p = PLR;
    for (int i = 0; i < 9; i++) gfx_setPaletteEntry(PAL_SLOT[i], gfx_rgb(p.pal[i][0], p.pal[i][1], p.pal[i][2]));
}

// ---- noise for the scenery ----------------------------------------------------------------------------------------
// A value noise: knots every 2^sh px, a hashed height 0-255 at each, joined smoothly (or straight, for jagged rock)
static int vn(uint32_t seed, int x, int sh, bool smooth) {
    const int k = x >> sh;
    int t = (x & ((1 << sh) - 1)) << (8 - sh);
    static uint32_t ms[8]; static int mk[8]; static uint8_t ma[8], mb[8];  // a few knots remembered: the columns walk along them
    const uint8_t slot = (uint8_t)((seed * 7 + (uint32_t)sh) & 7);
    if (ms[slot] != seed + 1 || mk[slot] != k) {
        ms[slot] = seed + 1; mk[slot] = k;
        ma[slot] = (uint8_t)hash(seed + (uint32_t)k * 2654435761u); mb[slot] = (uint8_t)hash(seed + (uint32_t)(k + 1) * 2654435761u);
    }
    const int a = ma[slot], b = mb[slot];
    if (smooth) t = (t * t * (768 - 2 * t)) >> 16;
    return a + (((b - a) * t) >> 8);
}

// a background layer's top at layer x: 0 far (1/8 speed), 1 mid (1/4), 2 near (1/2)
static const uint8_t LBASE[3] = { 56 + WDY, 72 + WDY, 86 + WDY }, LAMP[3] = { 42, 36, 28 }, LSH[3] = { 6, 5, 5 };
static int layerH(int L, int x) {
    const Planet &p = PLR;
    const uint32_t seed = (uint32_t)planet * 7919u + (uint32_t)L * 104729u;
    const int base = LBASE[L], amp = LAMP[L], sh = LSH[L];
    switch (p.style[L]) {
    case ST_JAGGED:
        return base - ((vn(seed, x, sh, false) * amp) >> 8) - ((vn(seed + 1, x, sh - 2, false) * amp) >> 10) - ((vn(seed + 2, x, sh - 3, false) * amp) >> 11);
    case ST_MESA: {                                            // flat tops and cliffs: the height in four steps
        const int q = vn(seed, x, sh + 1, true) >> 6;
        return base - q * amp / 4 - (vn(seed + 1, x, 3, false) >> 7);
    }
    case ST_SPIKES: {                                          // pyramids and crystal spires: a peak in every cell
        const int s2 = sh - 1, k = x >> s2, t = ((x & ((1 << s2) - 1)) << 9) >> s2, tri = t < 256 ? t : 511 - t;
        const int hk = 80 + (int)(hash(seed + (uint32_t)k * 977u) % 176u);
        return base - ((((hk * tri) >> 8) * amp) >> 8);
    }
    case ST_CONES: {                                           // volcanoes: a cone with a crater in some wide cells
        const int s2 = sh + 1, k = x >> s2, t = ((x & ((1 << s2) - 1)) << 9) >> s2;
        int tri = t < 256 ? t : 511 - t;
        if ((hash(seed + (uint32_t)k * 31u) & 3) == 0) { if (tri > 236) tri = 236 - (tri - 236); return base - ((tri * amp) >> 8) - 4; }
        return base - ((vn(seed, x, sh, true) * amp) >> 9);
    }
    default:
        return base - ((vn(seed, x, sh + 1, true) * amp) >> 8) - ((vn(seed + 1, x, sh - 1, true) * amp) >> 10);
    }
}

// ---- building a stage ---------------------------------------------------------------------------------------------
static void buildTile() {
    const Planet &p = PLR;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            uint8_t v = 2;
            switch (p.tile) {
            case TL_BRICKS: {                                  // cut blocks, each row offset by half a block
                const int row = y >> 2, bx = (x + (row & 1) * 4) & 7, by = y & 3;
                v = (by == 3 || bx == 7) ? 0 : by == 0 ? 3 : by == 2 ? 1 : 2;
                if (v && (hash((uint32_t)((x + (row & 1) * 4) >> 3) * 31u + row * 7u) & 7) == 0) v = v > 1 ? v - 1 : v;   // a worn block
                break;
            }
            case TL_STRATA: {                                  // layered rock with wavy bands and cracks
                const int w = y + ((isin(x * 8) * 2) >> 8), b = (w & 7);
                v = b == 0 ? 3 : b < 3 ? 2 : b < 6 ? 1 : 2;
                if (((x * 5 + y * 3) & 31) == 0 || (hash((uint32_t)(x * 16 + y)) & 31) == 0) v = 0;
                break;
            }
            case TL_CRYSTAL: {                                 // faceted shards: diagonal planes with bright edges
                const int a = (x + y) & 7, b = (x - y + 16) & 7;
                v = a == 0 || b == 0 ? 0 : a < 4 ? 3 : b < 4 ? 1 : 2;
                break;
            }
            default: {                                         // round stones: the nearest of nine jittered centres (wrapping)
                int best = 9999, second = 9999, bdx = 0, bdy = 0;
                for (int k = 0; k < 6; k++) {
                    const uint32_t h = hash((uint32_t)k * 101u + planet);
                    const int cx = (k & 1) * 8 + (int)(h & 3), cy = (k >> 1) * 5 + (int)((h >> 3) & 3);
                    for (int wx = -16; wx <= 16; wx += 16)
                        for (int wy = -16; wy <= 16; wy += 16) {
                            const int dx = x - cx - wx, dy = (y - cy - wy) * 2, d = dx * dx + dy * dy;
                            if (d < best) { second = best; best = d; bdx = dx; bdy = dy; } else if (d < second) second = d;
                        }
                }
                v = second - best < 9 ? 0 : (bdy < -1 && bdx < 2) ? 3 : bdy > 2 ? 1 : 2;
            }
            }
            tile[x][y] = v;
        }
}

static void flatten(int cx, int w) {                          // a level patch of ground w px wide, ramped at its ends
    int i0 = (cx - w / 2) >> 2, i1 = (cx + w / 2) >> 2;
    i0 = clampi(i0, 0, HN - 1); i1 = clampi(i1, 0, HN - 1);
    int lv = hgt[(i0 + i1) / 2];
    if (lv < 60) lv = 60;
    for (int i = i0; i <= i1; i++) hgt[i] = (uint8_t)lv;
    for (int k = 1; k <= 6; k++) {
        if (i0 - k >= 0) hgt[i0 - k] = (uint8_t)(lv + (hgt[i0 - k] - lv) * k / 7);
        if (i1 + k < HN) hgt[i1 + k] = (uint8_t)(lv + (hgt[i1 + k] - lv) * k / 7);
    }
}

static NOINL Enemy *spawnEn(uint8_t k, int x, int y) {
    for (auto &e : en) if (!e.kind) {
        e = {};
        e.kind = k; e.x = (int16_t)(x * 16); e.y = (int16_t)(y * 16);
        e.hp = k == E_UFO ? 3 : k == E_TURRET ? 2 : 1;
        e.target = 255;
        e.t = (uint8_t)rnd(60);
        return &e;
    }
    return nullptr;
}

static void placeLanderOnPad(int padX) {
    lx = (int32_t)(padX + 20) << 8;
    ly = (int32_t)(groundAt(padX + 20) - 8) << 8;
    lvx = lvy = 0; ang = 0; lst = 1; gear = 1; onPad = padX == basePadX ? 2 : 1;
    fuel = FUEL_MAX; shield = shMax(); deadT = 0; invT = 60; dryT = 0;
    facing = (int8_t)(baseRight ? 1 : -1);
    if (onPad == 2) facing = (int8_t)-facing;
}

static void genStage() {
#ifdef CHGBA
    hgtGen++;
#endif
    planet = stage & 7;
    sector = stage >> 3;
    baseRight = ((stage + sector) & 1) == 0;
    const uint32_t s = (uint32_t)stage * 977u + 13u;
    const int rough = 10 + planet * 3 + sector * 6;
    for (int i = 0; i < HN; i++) {
        const int x = i * 4;
        int h = 116 + WDY - ((vn(s, x, 7, true) * 24) >> 8) - ((vn(s + 1, x, 5, true) * 14) >> 8) - ((vn(s + 2, x, 3, false) * rough) >> 8);
        const int pk = vn(s + 3, x, 7, true);
        if (pk > 170) h -= (pk - 170) * (26 + planet * 3) / 85;                // ridges to climb over
        hgt[i] = (uint8_t)clampi(h, 46 + WDY, 122 + WDY);
    }
    // the pads: the start at one end, the base (pad and building) at the other
    if (baseRight) { startPadX = 30; basePadX = W - 116; buildX = W - 54; doorX = buildX + BASE_DOOR[PLR.base]; }
    else { startPadX = W - 70; basePadX = 76; buildX = 10; doorX = buildX + 43 - BASE_DOOR[PLR.base]; }
    flatten(startPadX + 20, 64);
    flatten(baseRight ? W - 70 : 70, 128);
    alienX = baseRight ? 170 : W - 210;
    flatten(alienX + 20, 60);
    burgerX = W / 2 - 160 + (int)(hash(s + 5u) % 200) - 100;            // the takeaway, somewhere in the middle
    if (burgerX >= 0) flatten(burgerX + 50, 130);
    // the colonists' camps: flat ground, the colonists on one half, room to land on the other
    nZones = (uint8_t)clampi(4 + stage / 3, 4, ZONES);
    const int span = W - 360;
    for (int z = 0; z < nZones; z++) {
        int cx = 200 + span * (2 * z + 1) / (2 * nZones) + (int)(hash(s + z * 7u) % 60) - 30;
        if (burgerX >= 0 && cx > burgerX - 50 && cx < burgerX + 150) cx = cx < burgerX + 50 ? burgerX - 50 : burgerX + 150;   // not in the diner's lot
        zoneX[z] = (int16_t)cx;
        flatten(cx, 60);
    }
    memset(men, 0, sizeof men);
    nMen = (uint8_t)clampi(8 + stage, 8, MEN);
    quota = (uint8_t)clampi(3 + stage + diff, 3, nMen * 3 / 4);      // 3, 4, 5, 6 ... one more each stage (and each difficulty)
    memset(items, 0, sizeof items);
    { const uint8_t *a = PLR.alien[sector & 1]; remap(EMAP, RM(3, a[0]) | RM(5, a[1]) << 8 | RM(11, a[2]) << 16 | RM(6, a[3]) << 24); }
    dlv = foodOn = partyT = lidT = feasts = cowsN = crashN = 0;
    const int nIt = clampi((stage + 2) / 3, 0, 5);
    for (int k = 0; k < nIt + 2; k++) {                      // (and every stage a repair kit and a fuel cell)                // things to pick up: supplies, repairs, fuel, now and then an engine part
        int x = 300 + (int)(hash(s + k * 53u) % (W - 600));
        while (busyAt(x)) x += 23;
        items[k] = { (int16_t)x, (uint8_t)(k >= nIt ? (k > nIt ? IT_FUEL : IT_REPAIR) : k == 0 ? IT_SUPPLY : (stage % 5 == 4 && k == 1) ? IT_ENGINE : (k & 1) ? IT_REPAIR : IT_FUEL), 1 };
        flatten(x, 24);
    }
    for (int i = 0, z = 0, c = 0, pass = 0; i < nMen; i++) {
        Man &m = men[i];
        if (c >= 2 + (z + stage) % 3) { if (++z == nZones) { z = 0; pass++; } c = 0; }   // this camp is full: on to the next (round again: the other side)
        const int k = c++;                                                    // its place in the camp
        const int side = (baseRight ? -1 : 1) * (pass & 1 ? -1 : 1);         // camp on the side away from the base
        m.home = (uint8_t)z;
        m.x = (int16_t)(zoneX[z] + side * (6 + (k & 3) * 8 + (int)(hash(s + i * 13u) % 3)));
        m.y = (int16_t)groundAt(m.x);
        m.st = M_IDLE; m.act = D_STAND; m.t = (uint8_t)rnd(60); m.face = (int8_t)(rnd(2) ? 1 : -1); m.look = (uint8_t)(hash(s + i * 71u) & 31);
    }
    nSaved = nLost = 0;
    memset(en, 0, sizeof en);
    const int turrets = clampi(planet / 2 + sector * 2, 0, 4);
    for (int k = 0; k < turrets; k++) {                                       // turrets on open ground between the camps
        int x = 260 + (int)(hash(s + k * 41u) % (W - 520));
        for (int z = 0; z < nZones; z++) if (absi(x - zoneX[z]) < 60) x += 70;
        spawnEn(E_TURRET, x, groundAt(x) - 3);
    }
    for (int k = 0; k < PLR.worms; k++) if (Enemy *e = spawnEn(E_WORM, 500 + k * 700, 100 + WDY)) e->hp = 12;
    if ((stage & 3) == 3) if (Enemy *e = spawnEn(E_MOTHER, alienX + 20, 34 + WDY)) e->hp = (uint8_t)clampi(24 + stage, 24, 70);
    buildTile();
    memset(shots, 0, sizeof shots); memset(es, 0, sizeof es); memset(pt, 0, sizeof pt); memset(booms, 0, sizeof booms); memset(pops, 0, sizeof pops);
    placeLanderOnPad(startPadX);
    cam = clampi(LX() - SW / 2, 0, W - SW);
    spawnT = 600; endT = 0; lightT = 0; warnT = 0;
    for (auto &t : topsAt) t = -9999;                              // a new world: the layers' tops are made afresh
    wxLvl = (uint8_t)rnd(4);
#ifdef SL_STORM
    wxLvl = 3;                                                     // a perf test: the worst weather
#endif ticket = 0; tubeT = 0;
    memset(crit, 0, sizeof crit);
}

// ---- effects ------------------------------------------------------------------------------------------------------
static NOINL void part(int x, int y, int vx, int vy, uint8_t life, uint8_t col) {          // x, y px; vx, vy 1/16 px a frame
    for (auto &q : pt) if (!q.life) { q = { (int16_t)(x * 16), (int16_t)(y * 16), (int8_t)vx, (int8_t)vy, life, col }; return; }
}
// n sparks from (x, y), x speeds -sx/2..sx/2, y speeds -up..sy-up (1/16 px a frame), in two colours by turns
static NOINL void burst(int x, int y, int n, int sx, int sy, int up, uint8_t life, uint8_t c1, uint8_t c2) {
    for (int i = 0; i < n; i++) part(x, y, rnd(sx) - sx / 2, rnd(sy) - up, life, i & 1 ? c1 : c2);
}
static NOINL void blast(int x, int y, int r, int delay) {
    for (auto &b : booms) if (!b.t) { b = { (int16_t)x, (int16_t)y, 16, (uint8_t)r, (uint8_t)delay }; break; }
}
// an explosion: a fireball and its shock ring, sparks and debris thrown out, smoke rising after
static void boom(int x, int y, int n, int r) {
    static const uint8_t COLS[4] = { C_GOLD, C_RED, C_WHITE, C_GOLD };
    for (int i = 0; i < n; i++) part(x, y, rnd(56) - 28, rnd(56) - 36, (uint8_t)(12 + rnd(22)), COLS[rnd(4)]);
    for (int i = 0; i < n / 3; i++) part(x + rnd(9) - 4, y + rnd(7) - 3, rnd(9) - 4, -4 - rnd(6), (uint8_t)(24 + rnd(24)), i & 1 ? C_STEEL : C_SILVER);   // smoke
    blast(x, y, r, 0);
}
static NOINL void say(uint8_t id, uint8_t t = 50, int num = -1) { banner = id; bannerT = t; bannerNum = (int8_t)num; bannerNew = 1; }
static NOINL void addScore(int v, int x, int y) {
    score += v;
    if (score > hiScore) hiScore = score;
    if (score >= nextLife) { nextLife += 10000; if (lives < 9) lives++; say(MS_EXTRA_LANDER); }
    if (x >= 0) for (auto &q : pops) if (!q.t) { q = { (int16_t)x, (int16_t)y, (uint16_t)v, 30 }; break; }
}
static inline uint8_t shMax() { return (uint8_t)(diff ? 4 - diff : 5); }           // hits the hull takes: 5 easy, 3 normal, 2 hard
static int gHund() { const int g = PLR.grav + (sector ? PLR.grav / 8 : 0); return g * 100 / 64; }   // gravity in hundredths of a G

// ---- save ---------------------------------------------------------------------------------------------------------
static const uint32_t MAGIC = save::magic("SLFT");
static SaveData sv;
static bool saveDue, saveFlag, hasSave;
static void saveProgress(bool inGame) {
    sv = { hiScore, score, (uint8_t)stage, (uint8_t)lives, musicV, fxV, diff };
    saveFlag = inGame; saveDue = true;
}

static void newStage() {
    planet = stage & 7;
    loadReq = true;
    state = INTRO; timer = 210;                                            // the stage card, then who's who
    synth::music((stage & 3) == 3 ? 9 : 1 + planet);                     // a boss stage: SHOWDOWN
}
static uint8_t selStage;                                         // (testing) the stage a new game starts on
static void newGame() {
    stage = selStage;
#ifdef SL_START
    stage = SL_START - 1;
#endif
    score = 0; lives = 3; nextLife = 10000; gSaved = 0; newHi = false;
    sfx(S_START);
    newStage();
    saveProgress(true);
}
static void gameOver() { state = OVER; overT = 0; newHi = score > 0 && score >= hiScore; sfx(S_OVER); synth::music(0); synth::engine(0); saveProgress(false); }

// ---- the lander -----------------------------------------------------------------------------------------------------
static void wreck() {
#ifdef CHGBA
    gba_mosaic(12);                                                 // the GBA: the picture shatters for a moment
#endif
    lst = 2; deadT = 80; shake = 18; flashT = 6; crashN++;
    boom(LX(), LY(), 34, 20);
    for (int k = 0; k < 4; k++) blast(LX() + rnd(25) - 12, LY() + rnd(15) - 9, 8 + rnd(8), 4 + k * 5);   // the tanks go one after another
    burst(LX(), LY(), 14, 60, 50, 60, 50, C_SILVER, C_GOLD);           // the hull in pieces
    sfx(S_CRASH);
    synth::engine(0);
    int lost = 0;
    for (auto &m : men) if (m.st == M_ABOARD) { m.st = M_LOST; nLost++; lost++; }
    for (auto &it : items) if (it.st == 2) it.st = 0;                // the supplies go down with it (and the takeaway)
    foodOn = 0;
    seats = 0;
    say(lost ? MS_CREW_LOST : MS_LANDER_LOST, 60);
}
static void damage(int n) {
    if (invT || lst == 2) return;
    sfx(S_HIT); shake = 6; invT = 80;
    if (shield <= n) { shield = 0; wreck(); } else shield -= (uint8_t)n;
}

// a point of the hull, turned with the lander: local (dx, dy) from its centre
static void hullPt(int dx, int dy, int &x, int &y) {
    const int s = sinA(ang), c = cosA(ang);
    x = LX() + ((dx * c - dy * s) >> 8);
    y = LY() + ((dx * s + dy * c) >> 8);
}

static NOINL void rope(int x1, int y1) {                         // a taut rope from under the lander (tilting with it) to x1, y1 on screen
    int x0, y0;
    hullPt(0, 9, x0, y0); x0 -= cam;
    const int dx = x1 - x0, dy = y1 - y0, n = absi(dx) > absi(dy) ? absi(dx) : absi(dy);
    for (int k = 0; k <= n; k++) pset(x0 + (n ? dx * k / n : 0), y0 + (n ? dy * k / n : 0), C_SILVER);
}

static void touchDown() {
    const int x = LX();
    const int gl = groundAt(x - 7), gr = groundAt(x + 7);
    const bool level = absi(gl - gr) <= 4, upright = absi(ang) <= 3, soft = lvy <= 420 - dE * 40 && absi(lvx) <= 270 - dE * 30;
    if (gear && level && upright && !soft && shield > 1 && lvy <= 620 - dE * 40 && absi(lvx) <= 380 - dE * 30) {
        shield--; sfx(S_HIT); shake = 6;                                 // a heavy bump: it holds
    } else if (!gear || !level || !upright || !soft) {
        if (!gear) say(MS_GEAR_UP, 60);
        else if (!level) say(MS_TOO_STEEP, 60);
        else if (!upright) say(MS_NOT_LEVEL, 60);
        wreck();
        return;
    }
    const int pts = lvy < 80 ? 100 : 25;
    ang = 0; lvx = lvy = 0; lst = 1;
    ly = (int32_t)((gl < gr ? gl : gr) - 8) << 8;
    onPad = (x >= basePadX && x < basePadX + 40) ? 2 : (x >= startPadX && x < startPadX + 40) ? 1 : 0;
    addScore(pts, x, LY() - 14);
    say(pts == 100 ? MS_PERFECT_LANDING : MS_LANDED, 40);
    sfx(S_LAND);
    burst(x, gl, 8, 40, 6, 6, 14, C_GNDL, C_GND);                      // dust kicked out both ways
    for (auto &m : men)                                                  // landing on a colonist
        if (m.st == M_IDLE && absi(m.x - x) <= 9) { m.st = M_LOST; nLost++; sfx(S_SQUASH); say(MS_SQUASHED, 50); part(m.x, m.y - 4, 0, -12, 20, C_WHITE); }
}

static void updateLander(bool l, bool r, bool thr, bool gearEdge, bool fire, bool up) {
    if (invT) invT--;
    if (fireCd) fireCd--;
    if (lst == 2) {
        if (--deadT == 0) {
            if (lives <= 1) { lives = 0; gameOver(); return; }
            lives--;
            placeLanderOnPad(basePadX);
            say(MS_NEW_LANDER, 50);
        }
        return;
    }
    if (gear == 2 && groundAt(LX()) - LY() > 20) { gear = 0; sfx(S_GEAR); }   // clear of the ground after take-off: the gear folds up by itself
    if (gearEdge && lst == 0) { gear = !gear; sfx(S_GEAR); say(gear ? MS_GEAR_DOWN : MS_GEAR_UP_22, 30); }
    if (gunCd) gunCd--;
    if (up) {                                                        // holding UP charges the plasma (quickly)
        if (charge < CHARGED) {
            if (++charge == CHARGED) sfx(S_FUEL);
            else if (charge > 2) { const int dx = rnd(17) - 8, dy = rnd(17) - 8; part(LX() + facing * 10 + dx, LY() + dy, -dx * 3, -dy * 3, 5, (charge & 2) ? C_WHITE : C_ACC); }
        }
    } else {
        if (charge == CHARGED) {                                     // let go: three balls, forwards, backwards and down
            int n = 0;
            for (auto &s : shots) if (!s.life && n < 3) {
                const int8_t d = (int8_t)(n == 0 ? facing : n == 1 ? -facing : 0);
                s = { (int16_t)(LX() + d * 10), (int16_t)(LY() + (d ? 0 : 10)), d, 40, (uint8_t)(d ? 2 : 3) };   // kind 3: falls
                n++;
            }
            sfx(S_BLAST); shake = 4;
        }
        charge = 0;
    }
    if (fire) {                                                      // B: the rotary cannon chatters, the laser fires with it
        for (auto &s : shots) if (!s.life && !gunCd) {
            s = { (int16_t)(LX() + facing * 8), (int16_t)(LY() + 2 + ((frameNo >> 1) & 1) - (frameNo & 1)), facing, 9, 1 };
            gunCd = 3; sfx(S_GUN);
        }
        for (auto &s : shots) if (!s.life && !fireCd) {
            s = { (int16_t)(LX() + facing * 9), (int16_t)(LY() - 1), facing, 26, 0 };
            fireCd = 7; sfx(S_LASER);
        }
    }
    if (thr && fuel > 0) { if (thrLvl < 6) thrLvl++; } else if (thrLvl) thrLvl = thrLvl > 2 ? thrLvl - 2 : 0;   // the engine spools up and down
    thrusting = thrLvl > 0;
    if (lst == 1) {                                                  // on the ground
        rotT = 0;
        if (l != r) facing = r ? 1 : -1;                             // LEFT/RIGHT on the ground turn the guns round
        if (thrLvl >= 4) { lst = 0; gear = 2; ly -= 512; lvy = -40; onPad = 0; unloadT = 0; tripN = 0; }
        else {
            synth::engine(0);
            for (auto &it : items)                                   // things beside the lander are loaded
                if (it.st == 1 && absi(it.x - LX()) < 20) {
                    it.st = it.kind == IT_SUPPLY ? 2 : 0;
                    sfx(S_CATCH);
                    burst(it.x, groundAt(it.x) - 3, 8, 20, 20, 20, 16, C_GOLD, C_WHITE);
                    if (it.kind == IT_REPAIR) { shield = shMax(); say(MS_HULL_REPAIRED); }
                    else if (it.kind == IT_FUEL) { fuel = (uint16_t)(fuel + FUEL_MAX / 2 > FUEL_MAX ? FUEL_MAX : fuel + FUEL_MAX / 2); say(MS_FUEL_CELL); }
                    else if (it.kind == IT_ENGINE) { if (engineUp < 3) engineUp++; say(MS_ENGINE_UP); }
                    else say(MS_SUPPLIES);
                    addScore(100, it.x, groundAt(it.x) - 12);
                }
            if (onPad == 2) {                                        // the base pad: refuel, repair, let the colonists off
                for (auto &it : items) if (it.st == 2) { it.st = 0; addScore(500 * (sector + 1), doorX, groundAt(doorX) - 20); say(MS_DELIVERED); sfx(S_SAVED); }
                if (foodOn) {
                    foodOn = 0; partyT = 180; feasts++; addScore(2000 * (sector + 1), doorX, groundAt(doorX) - 28); sfx(S_CLEAR);
                    const int k = planet % 3;                                    // the world's food: a lander, a better engine or a mended hull
                    if (k == 0) { if (lives < 9) lives++; } else if (k == 1) { if (engineUp < 3) engineUp++; } else shield = shMax();
                    say((uint8_t)(MS_FED_LANDER + k), 90);
                }
                if (fuel < FUEL_MAX) { fuel = (uint16_t)(fuel + 10 > FUEL_MAX ? FUEL_MAX : fuel + 10); if ((frameNo & 3) == 0) sfx(S_FUEL); }
                if ((frameNo & 31) == 0 && shield < shMax()) shield++;
                if (seats && ++unloadT >= 12) {
                    unloadT = 0;
                    bool off = false;
                    for (auto &m : men) if (m.st == M_ABOARD) {
                        m.st = M_WALK; m.x = (int16_t)(LX() + (doorX > LX() ? 5 : -5)); m.y = (int16_t)groundAt(m.x); m.face = (int8_t)sgn(doorX - m.x); seats--; off = true;
                        break;
                    }
                    if (!off) seats = 0;                                     // nobody left aboard: the count was out
                }
            } else if (!fuel) {                                      // stranded: the colonists get off and a new lander comes
                if (++dryT > 90) {
                    for (auto &m : men) if (m.st == M_ABOARD) { m.st = M_IDLE; m.act = D_STAND; m.x = (int16_t)(LX() + rnd(24) - 12); m.y = (int16_t)groundAt(m.x); }
                    seats = 0;
                    say(MS_OUT_OF_FUEL, 60);
                    lst = 2; deadT = 1;
                }
            }
            return;
        }
    }
    // rotation: a step at once, then one every four frames while held
    if (l || r) {
        if (rotT == 0 || rotT >= 6) {
            ang = (int8_t)clampi(ang + (r ? 1 : -1), -6, 6);
            if (rotT >= 6) rotT = gear ? 4 : 3;                         // (gear down: finer, quicker turns for the landing)
        }
        rotT++;
    } else rotT = 0;
    if (ang) facing = (int8_t)sgn(ang);
    // Newton: gravity, the engine along the hull, a little drag in an atmosphere, the wind
    const Planet &p = PLR;
    static uint8_t gacc;                                             // gravity in 1/1024 px a frame squared: carry the quarters
    gacc = (uint8_t)(gacc + p.grav * 12 / 5 + (sector ? p.grav / 4 : 0));    // a heavy lander: two and three quarter times the worlds' gravity
    lvy += gacc >> 2; gacc &= 3;
    if (thrusting) {
        int t = (104 + p.grav * 5 / 4 + engineUp * 7) * 48 / (48 + seats) * thrLvl / 6;   // the engine suits the world (a full load still climbs on the heaviest); passengers are heavy; it spools up
        if (LY() < TOP) t = LY() > -2 ? t * (LY() + 2) / (TOP + 2) : 0;            // above the screen the air thins fast: no hanging up there
        lvx += (int16_t)((t * 5 * sinA(ang)) >> 12);                          // the push sideways a quarter as strong again
        lvy -= (int16_t)((t * cosA(ang)) >> 10);
        if (thr) fuel--;
        if (fuel == FUEL_MAX / 3) { sfx(S_LOW); say(MS_LOW_FUEL, 50); }
        if (!fuel) say(MS_FUEL_EMPTY, 60);
        if (frameNo & 1) { int nx, ny; hullPt(0, 10, nx, ny); part(nx, ny, (-sinA(ang) >> 5) + rnd(5) - 2, (cosA(ang) >> 5) + rnd(3), 10, (frameNo & 2) ? C_STEEL : C_SILVER); }
        const int gy = groundAt(LX());
        if (gy - LY() < 34 && (frameNo & 1)) for (int k = -1; k <= 1; k += 2) part(LX(), gy - 1, k * (12 + rnd(14)), -rnd(5), 16, (frameNo & 4) ? C_GNDL : C_GND);   // dust kicked up
    }
    synth::engine(thrusting ? 200 : 0);
    if (p.air) {                                                     // the air drags the lander towards the wind's speed
        lvx += (int16_t)((windV * 3 / 4 - lvx) >> (9 - p.air));       // (the wind three quarters as strong on the lander)
        lvy -= lvy >> 8;
    }
    lvx = (int16_t)clampi(lvx, -760, 760); lvy = (int16_t)clampi(lvy, -700, 1150);
    lx += lvx; ly += lvy;
    if (LY() < -2) { ly = -2 << 8; if (lvy < 0) lvy = 0; }            // the ceiling: the bottom third of the lander stays on screen
    if (lx < (10 << 8)) { lx = 10 << 8; lvx = 0; }
    if (lx > ((W - 10) << 8)) { lx = (W - 10) << 8; lvx = 0; }
    // the ground: the feet (or, gear up, the descent stage) and the hull's other corners
    static const int8_t PTS[6][2] = { { -7, 8 }, { 7, 8 }, { 0, 7 }, { -7, 0 }, { 7, 0 }, { 0, -9 } };
    bool hit = false, body = false;
    for (int k = 0; k < 6; k++) {
        int x, y;
        hullPt(PTS[k][0], PTS[k][1] - (k < 2 && !gear ? 2 : 0), x, y);
        if (y >= groundAt(x)) { hit = true; if (k >= 3 || (k == 2 && gear)) body = true; }
        if (x >= buildX + 2 && x < buildX + 42 && y > groundAt(buildX + 22) - PK.base.h + 4) body = true;   // the base building
    }
    if (body && LY() < groundAt(LX()) - 4) { say(MS_CRASHED, 60); wreck(); return; }
    if (hit) {
        if (lvy <= 0) { ly -= 256; lvy = 0; return; }                       // rising (or just lifted off): pushed clear, not landed
        touchDown(); return;
    }
    // a warning: low, falling and the gear up
    if (!gear && lvy > 40 && groundAt(LX()) - LY() < 30) { if (++warnT % 20 == 1) { sfx(S_WARN); say(MS_GEAR_TAP_DOWN_TWICE, 20); } } else warnT = 0;
}

// ---- the colonists --------------------------------------------------------------------------------------------------
static NOINL int nearestIdle(int x, int maxd) {
    int best = -1, bd = maxd;
    for (int i = 0; i < nMen; i++) if (men[i].st == M_IDLE && absi(men[i].x - x) < bd) { bd = absi(men[i].x - x); best = i; }
    return best;
}

static NOINL bool walking() { for (const auto &m : men) if (m.st == M_WALK) return true; return false; }   // colonists still on their way in

static NOINL bool roomBeside(int x) {                            // flat ground on one side: twice the lander's width
    const int g = groundAt(x);
    for (int d = -1; d <= 1; d += 2) {
        int k = 6;
        while (k <= 38 && absi(groundAt(x + d * k) - g) <= 2) k += 8;
        if (k > 38) return true;
    }
    return false;
}

static NOINL void manStep(Man &m, int d) { m.x = (int16_t)(m.x + d); m.y = (int16_t)groundAt(m.x); }   // a step along the ground

static void updateMen() {
    const int X = LX(), Y = LY();
    for (int i = 0; i < nMen; i++) {
        Man &m = men[i];
        switch (m.st) {
        case M_IDLE: {
            if (m.act == D_TRIP && m.t) { m.t--; break; }                   // still flat on its face
            const int dx = X - m.x;
            if (lst == 1 && absi(dx) < 64 && absi(groundAt(X) - m.y) < 10 && seats < SEATS) {   // run for it
                m.st = M_RUN; m.face = (int8_t)sgn(dx); m.t = 0; break;
            }
            if (m.act == D_TREK) {                                                               // on its way to a camp: to the edge of its flat ground
                const int hd = zoneX[m.home] - m.x;
                if (absi(hd) > 20) { m.face = (int8_t)sgn(hd); if (frameNo & 1) manStep(m, m.face); break; }
                m.act = D_WAVE; m.t = 30;
            }
            if (lst == 0 && absi(dx) < 80 && Y < m.y - 10) {                                     // the lander is near: wave
                if (m.act != D_WAVE) { m.act = D_WAVE; m.face = (int8_t)(sgn(dx) ? sgn(dx) : 1); }
                m.t = 8;
                break;
            }
            if (lst == 1 && absi(dx) < 90 && seats >= SEATS) { m.act = D_CHEER; m.t = 10; break; }
            if (m.t) { m.t--; }
            if (m.act == D_LOOK && (m.t % 24) == 12) m.face = (int8_t)-m.face;
            if (m.act == D_WANDER && (frameNo & 3) == 0) {
                const int nx = m.x + m.face;
                if (absi(nx - zoneX[m.home]) > 26 || absi(nx - X) < 12) m.face = (int8_t)-m.face; else manStep(m, m.face);
            }
            if (!m.t) {                                                                         // something else to do
                const int r = rnd(10);
                int buddy = -1;
                for (int j = 0; j < nMen; j++) if (j != i && men[j].st == M_IDLE && absi(men[j].x - m.x) < 18 && absi(men[j].x - m.x) > 3) buddy = j;
                if (buddy >= 0 && r < 4) {                                                       // a chat: turn to each other
                    Man &b = men[buddy];
                    m.act = D_CHAT; m.t = (uint8_t)(90 + rnd(60)); m.face = (int8_t)sgn(b.x - m.x);
                    if (b.act != D_CHAT) { b.act = D_CHAT; b.t = m.t; b.face = (int8_t)-m.face; }
                    m.by = (uint8_t)(i < buddy ? 0 : 1);                                           // who speaks first
                } else if (r < 6) { m.act = D_LOOK; m.t = 72; }
                else if (r < 7) { m.act = D_GAZE; m.t = (uint8_t)(50 + rnd(40)); }
                else if (r < 9) { m.act = D_WANDER; m.t = (uint8_t)(40 + rnd(60)); m.face = (int8_t)(rnd(2) ? 1 : -1); }
                else { m.act = D_STAND; m.t = (uint8_t)(40 + rnd(60)); }
            }
            break;
        }
        case M_RUN:
            if (lst != 1) { m.st = M_IDLE; m.act = D_STAND; m.t = 30; break; }
            if ((frameNo & 3) != 3) manStep(m, sgn(X - m.x));
            if (rnd(400) == 0) { m.st = M_IDLE; m.act = D_TRIP; m.t = 36; break; }   // a trip: splat
            if (absi(X - m.x) <= 2) {
                if (seats < SEATS) { m.st = M_ABOARD; seats++; sfx(S_BOARD); addScore(10, -1, 0); if (seats == SEATS) say(MS_SEATS_FULL, 40); }
                else { m.st = M_IDLE; m.act = D_STAND; m.t = 40; }
            }
            break;
        case M_JET: {                                                          // jet-packing down (1/4 px): the burners fire near the ground
            const int alt = groundAt(m.x) - (m.y >> 2);
            if (alt > 22) { if (m.vy < 8 && (frameNo & 1)) m.vy++; }
            else { if (m.vy > 2) m.vy -= 2; if (((frameNo + i * 5) & 15) == 0) sfx(S_JET); }
            m.y = (int16_t)(m.y + m.vy);
            if (m.t) { m.t--; m.x = (int16_t)(m.x + m.face); }   // (spat out sideways)
            if ((frameNo & 7) == 0) m.x = (int16_t)(m.x + (isin(frameNo * 4 + i * 40) > 0 ? 1 : -1));
            if (!m.t && lst == 0 && seats < SEATS && absi(m.x - X) < 9 && absi((m.y >> 2) - Y) < 12) {   // caught in the air
                m.st = M_ABOARD; seats++; sfx(S_CATCH); addScore(500, X, Y - 14); say(MS_NICE_CATCH, 40);
            } else if ((m.y >> 2) >= groundAt(m.x)) { m.st = M_IDLE; m.y = (int16_t)groundAt(m.x); m.act = D_CHEER; m.t = 40;   // down safely
                if (!roomBeside(m.x)) {                                         // no room to land beside it: off to the nearest camp
                    m.act = D_TREK;
                    for (uint8_t z = 0; z < nZones; z++) if (absi(zoneX[z] - m.x) < absi(zoneX[m.home] - m.x)) m.home = z;
                }
            }
            break;
        }
        case M_DROP:                                                           // falling into the aliens' bucket, 1/4 px
            m.y = (int16_t)(m.y + 3);
            if ((m.y >> 2) >= groundAt(alienX + 20) - COLL_TOP[PLR.coll] + 3) { m.st = M_LOST; m.by = 255; nLost++; tubeT = 45; sfx(S_SLURP); }   // (255: in the collector)
            break;
        case M_WALK:
            if (frameNo & 1) manStep(m, sgn(doorX - m.x));
            if (absi(m.x - doorX) <= 1) {
                m.st = M_SAVED; nSaved++; gSaved++; sfx(S_SAVED); tripN++;
                if (seats == 0 && tripN > 1) {                                  // the last of the trip in: a bonus for bringing many
                    addScore(tripN * tripN * 50, doorX, m.y - 28);
                    say(MS_TRIP_BONUS_X, 60, tripN);
                }
                burst(doorX, m.y - 6, 6, 24, 14, 22, 18, C_GOLD, C_WHITE);   // a little fanfare of sparks
                addScore(250 * (sector + 1), doorX, m.y - 16);
            }
            break;
        default: break;
        }
    }
}

// ---- the enemies ----------------------------------------------------------------------------------------------------
static NOINL void eshot(int x, int y, int tx, int ty, int speed, uint8_t kind) {          // x, y px; speed in 1/16 px a frame
    const int dx = tx - x, dy = ty - y, d = absi(dx) > absi(dy) ? absi(dx) + absi(dy) / 2 : absi(dy) + absi(dx) / 2;
    if (!d) return;
    speed = speed * (2 + dE) / 4;                                  // easy half speed, normal three quarters
    for (auto &s : es) if (!s.life) {
        s = { (int16_t)(x * 16), (int16_t)(y * 16), (int8_t)(dx * speed / d), (int8_t)(dy * speed / d), 90, kind };
        sfx(S_ZAP);
        return;
    }
}

// a saucer collecting its order (4), or away with it (5, cargo 0), loses it: a shot or a bump knocks the bag loose
static NOINL void snatch(Enemy &e, int x, int y) {
    if (e.st != 4 && (e.st != 5 || e.cargo)) return;
    const int by = e.st == 4 ? groundAt(burgerX + 35) - 8 - e.cargo / 2 : y + 6;   // the bag where it was: in the beam, or aboard
    addScore(500 * (sector + 1), x, by - 16); say(MS_FOOD_SNATCHED, 60);
#ifdef SL_TRACE
    fprintf(stderr, "snatch f%d\n", frameNo);
#endif
    if (!dlv && !foodOn) { dlv = 1; bagX = x; bagY = by; }
    e.st = 5; e.cargo = 1; angryT = 110; angryE = (uint8_t)(&e - en);  // off it goes, empty-handed, seeing red
}

static void killEn(Enemy &e) {
    static const uint16_t PTS[7] = { 0, 150, 100, 50, 200, 1000, 5000 };
    const int x = e.x >> 4, y = e.y >> 4;
    addScore(PTS[e.kind] * (sector + 1), x, y - 8);
    if (killT) { kills++; if (kills >= 2) { say(kills == 2 ? MS_DOUBLE : kills == 3 ? MS_TRIPLE : MS_RAMPAGE, 40); addScore(100 * kills, -1, 0); } } else kills = 1;
    killT = 45;
    if (e.kind == E_UFO) {
        flashT = 3;
        for (auto &c : crit) if (c.kind == 5 && c.vx == (int8_t)(&e - en)) { c.kind = 3; c.vx = 1; c.t = 0; addScore(300, x, y - 10); say(MS_COW_SAVED, 50); }   // its cow falls free
    }
    if (e.kind == E_STINGER && rnd(4) == 0) for (auto &it : items) if (!it.st) { it = { (int16_t)x, IT_FUEL, 1 }; break; }   // a fuel cell falls from it
    if (e.kind == E_WORM) {                                                            // it bursts: the colonists it swallowed jet out
        addScore(1000, x, y - 20);                                                    // (a bonus for a worm)
        int n = 0;
        for (auto &m : men) if (m.st == M_LOST && m.by == 100 + (&e - en)) { m.st = M_JET; m.t = 0; m.x = (int16_t)(x + n * 5 - 5); m.y = (int16_t)(y << 2); m.vy = -6; m.by = 0; nLost--; n++; }
    }
    if (e.kind == E_MOTHER) {                                                          // it bursts: everyone it took jets down
        for (int k = 0; k < 5; k++) blast(x + rnd(41) - 20, y + rnd(13) - 6, 10 + rnd(8), k * 4);
        int n = 0;
        for (auto &m : men) if (m.st == M_LOST && m.by == 255) { m.st = M_JET; m.t = 0; m.x = (int16_t)(x + n * 5 - 20); m.y = (int16_t)(y << 2); m.vy = -6; m.by = 0; nLost--; n++; }
        say(MS_FREED, 70); flashT = 8; shake = 20;
    }
    boom(x, y, 16, 10);
    sfx(S_BOOM);
    if (e.kind == E_UFO) {
        const uint8_t me = (uint8_t)(&e - en + 1);
        int k = 0;
        for (auto &m : men)                                                            // the colonists bail out on jet packs
            if ((m.st == M_BEAM || m.st == M_HELD) && m.by == me) {
                const int sy = m.st == M_HELD ? y : (m.y < y ? y : m.y);
                m.st = M_JET; m.t = 0; m.x = (int16_t)(x + (k & 1 ? 1 : -1) * (k * 3)); m.y = (int16_t)(sy << 2); m.vy = -6; k++;
            }
        if (k) { say(k > 1 ? MS_JETPACKS_GO : MS_BAIL_OUT, 50); sfx(S_CATCH); }
        snatch(e, x, y);
        burst(x, y, 10, 64, 30, 24, 30, C_SILVER, C_GREEN);              // the saucer's plates
        shake = 6;
    }
    e.kind = E_NONE;
}

static void updateEnemies() {
    const int X = LX(), Y = LY();
    int fliers = 0, ufos = 0;
    for (auto &e : en) if (e.kind && e.kind < E_TURRET && !(e.kind == E_UFO && e.st >= 3 && e.st <= 5)) { fliers++; if (e.kind == E_UFO) ufos++; }
    if (tubeT) tubeT--;
    if (burgerX >= 0 && (frameNo % 280) == 100) {                       // the Moon: hungry saucers come to the diner
        int n = 0;
        for (auto &e : en) if (e.kind == E_UFO && e.st >= 3 && e.st <= 5) n++;
        if (n < 3) if (Enemy *e = spawnEn(E_UFO, burgerX + 160 + rnd(80), 20 + WDY)) { e->st = 3; e->target = ticket++; e->hp = 2; }
    }
    bool idle = false;
    for (int i = 0; i < nMen; i++) if (men[i].st == M_IDLE) idle = true;
    const int want = clampi((dE + stage) / 2, 1, 5 + dE);            // (one at a time on the first stage)
    if (spawnT) spawnT--;
    else if (fliers < want) {
        const int side = rnd(2) ? 1 : -1;
        const int x = clampi(cam + SW / 2 + side * (150 + (SW - 128) / 2 + rnd(110)), 20, W - 20);
        uint8_t k = (idle && ufos < 1 + stage / 3 && rnd(10) < 6) ? E_UFO : (rnd(3) && stage ? E_STINGER : E_MINE);   // (no stingers on the first stage)
        if (Enemy *e = spawnEn(k, x, 28 + WDY + rnd(30))) { if (k == E_UFO) { sfx(S_UFO); } e->vx = (int8_t)(-side * 8); }
        spawnT = clampi(300 - stage * 12, 90, 300) * (9 - 2 * dE) / 7;                  // enemies come spaced out: time for a rescue between them
    }
    for (int ei = 0; ei < ENS; ei++) {
        Enemy &e = en[ei];
        if (!e.kind) continue;
        e.t++;
        if (e.hit) e.hit--;
        const int ex = e.x >> 4, ey = e.y >> 4;
        switch (e.kind) {
        case E_UFO: {
            const int sp = (9 + stage / 2) * (8 - e.cargo) / 8;            // slower with each colonist aboard
            if (e.st == 0) {                                                  // seek a colonist on the ground
                if (e.target == 255 || men[e.target].st != M_IDLE) {
                    e.target = 255;
                    int bd = 700;
                    for (int i = 0; i < nMen; i++) {
                        if (men[i].st != M_IDLE) continue;
                        bool taken = false;
                        for (auto &o : en) if (&o != &e && o.kind == E_UFO && o.target == i) taken = true;
                        if (!taken && absi(men[i].x - ex) < bd) { bd = absi(men[i].x - ex); e.target = (uint8_t)i; }
                    }
                }
                if (e.target == 255 && e.cargo) { e.st = 2; break; }            // nobody left to take: home with what it has
                int tx = X + 40, ty = 34;
                if (e.target != 255) { tx = men[e.target].x; ty = men[e.target].y - 44; }
                e.vx = (int8_t)clampi((tx - ex) * 2, -sp, sp);
                e.vy = (int8_t)clampi((ty - ey) * 2, -10, 10);
                if (e.target != 255 && absi(tx - ex) < 2 && absi(ty - ey) < 2) {     // over the colonist: the beam
                    e.st = 1; e.t = 0; e.vx = e.vy = 0;
                    Man &m = men[e.target];
                    m.st = M_BEAM; m.by = (uint8_t)(ei + 1);
                    sfx(S_BEAM);
                }
            } else if (e.st == 1) {                                           // lifting
                Man &m = men[e.target];
                if (m.st != M_BEAM) { e.st = 0; e.target = 255; break; }
                if (e.t & 1) m.y--;
                if ((e.t % 14) == 0) sfx(S_BEAM);
                if (m.y <= ey + 6) {                                          // taken aboard
                    m.st = M_HELD; m.by = (uint8_t)(ei + 1);
                    e.cargo++; e.target = 255;
                    e.st = (e.cargo >= 5 || !idle) ? 2 : 0;
                    sfx(S_TAKEN); say(e.st == 2 ? MS_UFO_HEADING_HOME : MS_ABDUCTED, 50);
                }
            } else if (e.st == 2) {                                           // home to the bucket, slow when full
                const int tx = alienX + 20, ty = groundAt(tx) - 54;
                e.vx = (int8_t)clampi((tx - ex) * 2, -sp, sp);
                e.vy = (int8_t)clampi((ty - ey) * 2, -8, 8);
                if (absi(tx - ex) < 2 && absi(ty - ey) < 3) { e.st = 6; e.t = 0; e.vx = e.vy = 0; }
            } else if (e.st == 6) {                                           // one colonist at a time into the bucket
                if (e.t % 20 == 10) {
                    if (e.cargo) {
                        for (auto &m : men) if (m.st == M_HELD && m.by == ei + 1) { m.st = M_DROP; m.x = (int16_t)ex; m.y = (int16_t)((ey + 4) << 2); break; }
                        e.cargo--;
                    } else e.st = 0;
                }
            } else if (e.st == 3) {                                           // in the queue at the diner, shuffling
                if ((e.t & 127) == 64 && rnd(3) == 0) { e.hit = 10; sfx(S_WARN); }   // impatient: a honk
                int pos = 0;
                for (auto &o : en) if (o.kind == E_UFO && (o.st == 3 || o.st == 4) && o.target < e.target) pos++;
                const int win = burgerX + 35, tx = win + 4 + pos * 26 + (((e.t + e.target * 37) % 110) < 12 ? isin(e.t * 32) / 64 : 0), ty = groundAt(win) - 26;
                e.vx = (int8_t)clampi((tx - ex) * 2, -10, 10);
                e.vy = (int8_t)clampi((ty - ey) * 2, -8, 8);
                if (!pos && absi(tx - ex) < 2 && absi(ty - ey) < 3 && ++e.cargo > 100) { e.st = 4; e.cargo = 0; sfx(S_ORDER); }
            } else if (e.st == 4) {                                           // the order comes up in the beam
                e.vx = e.vy = 0;
                if ((e.t % 14) == 0 && absi(ex - X) < 90) sfx(S_BEAM);
                if ((frameNo & 3) == 0 && ++e.cargo >= 2 * (groundAt(burgerX + 35) - 8 - ey)) { e.st = 5; e.cargo = 0; }   // slowly
            } else {                                                          // away up into the sky
                e.vx = 0; e.vy = -22;
                if (ey < -10) { e.kind = E_NONE; break; }
            }
            if (e.st != 1 && e.st != 6 && e.st != 4) e.y = (int16_t)(e.y + isin(e.t * 6) / 32);
            break;
        }
        case E_STINGER: {                                                     // keeps its distance and spits plasma
            const int tx = X + (ex < X ? -48 : 48), ty = Y - 6 + isin(e.t * 3) / 16;
            e.vx = (int8_t)clampi(e.vx + sgn(tx - ex), -14 - sector * 4, 14 + sector * 4);
            e.vy = (int8_t)clampi(e.vy + sgn(ty - ey), -10, 10);
            const int rate = clampi(130 - planet * 6 - sector * 20, 50, 130) * (9 - 2 * dE) / 7;
            if (dlv == 3 && absi(scX - ex) < 110 && (e.t % rate) == 0) eshot(ex, ey, scX, scY, 22 + sector * 6, 0);   // the speeder first
            else if (lst != 2 && absi(X - ex) < 110 && (e.t % rate) == 0) eshot(ex, ey, X, Y, 22 + sector * 6, 0);
            break;
        }
        case E_MINE:
            if (absi(X - ex) < 160) { e.vx = (int8_t)clampi(e.vx + sgn(X - ex), -6, 6); e.vy = (int8_t)clampi(e.vy + sgn(Y - ey), -5, 5); }
            else e.vx = e.vy = 0;
            break;
        case E_WORM: {
            e.vy = 0; e.vx = 0;
            if (e.st >= 4) {                                                  // 4 on the rope, 5 dropped into the collector, 6 chewed up
                const int cy = groundAt(alienX + 20) - COLL_TOP[PLR.coll];
                if (e.st == 4) {
                    if (lst == 2) { e.st = 0; e.t = 0; hoverT = 0; break; }   // the lander lost: back into the sand
                    if (e.cargo < 34) {                                       // still in the sand: the lander has to haul it out
                        const int gy = groundAt(ex);
                        e.y = (int16_t)((gy - e.cargo) * 16);
                        if (thrusting && (frameNo & 3) == 0 && ++e.cargo == 34) { say(MS_WORM_DROP, 150); addScore(1500, X, Y - 16); sfx(S_CLEAR); }   // out: off to the aliens with it
                        if (LY() < gy - e.cargo - 40) { ly = (int32_t)(gy - e.cargo - 40) << 8; if (lvy < 0) lvy = 0; }   // the rope holds the lander down
                        if (absi(X - ex) > 20) { lx = (int32_t)(ex + (X > ex ? 20 : -20)) << 8; lvx = 0; }
                        for (int k = 0; k < 2; k++) part(ex + rnd(31) - 15, gy - 1, rnd(40) - 20, -6 - rnd(16), 18, (k & 1) ? C_GNDL : C_GNDD);   // sand everywhere
                        shake = 3;
                        if (e.t == 90) say(MS_PULL, 120);                       // (in case it is not obvious)
                        if ((frameNo & 3) == 0) { lvx = (int16_t)(lvx + rnd(41) - 20); lvy = (int16_t)(lvy + 10); }   // it fights: yanks the lander about
                        if (rnd(160) == 0 && e.cargo > 4) { e.cargo -= 3; shake = 10; sfx(S_CRASH); }   // and drags itself back in
                        if ((e.t & 7) == 0) sfx((e.t & 16) ? S_WORM : S_CLANG);   // roaring and straining
                    } else {
                        e.y = (int16_t)(e.y + clampi((Y + 16) * 16 - e.y, -16, 64)); e.x = (int16_t)(e.x + (((X - lvx / 32) * 16 - e.x) >> 3));   // swinging under the lander: it trails behind, then swings back
                        lvy = (int16_t)(lvy + 3);                                 // heavy: the lander feels it
                        if (spitT) {                                              // its mouth open: out comes a colonist it swallowed
                            if (--spitT == 10) for (auto &m : men) if (m.st == M_LOST && m.by == 100 + ei) {
                                m.st = M_JET; m.x = (int16_t)ex; m.y = (int16_t)((ey - 6) << 2); m.vy = -16; m.by = 0; nLost--;
                                m.face = (int8_t)(rnd(3) - 1); m.t = (uint8_t)(16 + rnd(30));   // straight up, or up and off to one side
                                sfx(S_SLURP); break;
                            }
                        } else if ((e.t & 63) == 32) for (const auto &m : men) if (m.st == M_LOST && m.by == 100 + ei) { spitT = 24; sfx(S_WORM); break; }
                        if ((e.t & 31) == 0) { sfx(S_WORM); shake = 2; }      // it roars and thrashes all the way
                    }
                    if (absi(X - alienX - 20) < 18) { e.st = 5; e.t = 0; sfx(S_WORM); }   // over the collector: let go
                } else if (e.st == 5) {
                    e.x = (int16_t)((alienX + 20) * 16); e.y = (int16_t)(e.y + 48);
                    if ((e.y >> 4) + 30 >= cy) { e.st = 6; e.t = 0; }
                } else {                                                      // chewed: gore and grinding, howling
                    tubeT = 45; shake = 4 + (e.t & 3);
                    for (int k = 0; k < 4; k++) part(alienX + 20 + rnd(21) - 10, cy - 2 - rnd(6), rnd(140) - 70, -20 - rnd(60), 30 + rnd(20), k < 2 ? C_RED : k == 2 ? C_STEEL : (e.t & 1) ? C_SILVER : C_GOLD);   // blood and bits up and out
                    if ((e.t & 5) == 0) sfx((e.t & 8) ? S_CRASH : S_SLURP);
                    if ((e.t % 12) == 6) sfx((e.t & 16) ? S_TAKEN : S_WORM);                  // howling in pain
                    if ((e.t % 28) == 3) throwGore(alienX + 20 - cam, cy - 6);                    // and some of it straight at the player
                    if (e.t >= 110) { e.kind = E_NONE; hoverT = 0; sfx(S_BLAST); addScore(2000, alienX + 20, cy - 20); say(MS_WORM_BONUS, 90, 2000); tankT = 100;   /* (and a full tank, filled after) */ for (int k = 0; k < 24; k++) part(alienX + 20, cy - 4, rnd(200) - 100, -30 - rnd(70), 45, (k & 1) ? C_RED : C_STEEL); }   // a last burst
                }
                break;
            }
            if (lst == 0 && hoverT < 255 && absi(ex - X) < (e.st ? 24 : 16) && groundAt(ex) - (e.st ? 38 : 0) - Y < (e.st ? 70 : 56)) {   // (measured from its head when it is up)   // the lander hovering over it: beeps, then the rope pulls it out
                hov = true;
                if ((++hoverT & (hoverT > 36 ? 7 : 15)) == 1) sfx(S_WARN);
                if (hoverT >= 54 && e.st == 0 && rnd(2)) { e.st = 1; e.t = 0; hoverT = 0; say(MS_WORM, 40); shake = 10; sfx(S_WORM); break; }   // a coin toss: it bursts up at the lander instead
                if (hoverT >= 54) { e.cargo = e.st ? 14 : 0; e.st = 4; e.t = 0; hoverT = 255; sfx(S_WORM); shake = 8; say(MS_HOLD_STEADY, 90); addScore(500, ex, groundAt(ex) - 20); break; }
            }
            if (e.st == 0) {                                                  // roaming under the sand
                int ti = nearestIdle(ex, 400);
                if (ti >= 0 && absi(men[ti].x - cam - SW / 2) > 150 + (SW - 128) / 2) ti = -1;   // it only hunts where the lander can see it coming
                int tx = ti >= 0 ? men[ti].x : ex + ((e.t & 128) ? 80 : -80);
                if (lst == 1 && absi(X - ex) < 300) tx = X;                   // a lander on the sand draws it
                e.vx = (int8_t)(sgn(tx - ex) * (6 + stage / 3));
                e.y = (int16_t)(groundAt(ex) * 16);
                if ((e.t & 31) == 0 && absi(ex - X) < 110) { sfx(S_WORM); shake = 3; }
                if (absi(tx - ex) < 3 && (ti >= 0 || lst == 1) && e.t > 120) { e.st = 1; e.t = 0; e.vx = 0; say(MS_WORM, 40); shake = 10; }   // it rests a while after a dive
            } else {
                e.vx = 0;
                e.y = (int16_t)((groundAt(ex) - 16) * 16);                    // up: it can be shot
                if (e.st == 1 && e.t >= 20) {                                 // the mouth closes on whoever is there
                    e.st = 2; e.t = 0;
                    int n = 0;
                    for (auto &mm : men) if ((mm.st == M_IDLE || mm.st == M_RUN) && absi(mm.x - ex) < 11) { mm.st = M_LOST; mm.by = (uint8_t)(100 + (&e - en)); nLost++; n++; }   // (by: in this worm)
                    if (n) { say(MS_SAND, 60); sfx(S_TAKEN); }
                    if (lst == 1 && absi(X - ex) < 12) { say(MS_SWALLOWED, 60); wreck(); }
                } else if (e.st == 2) {                                       // up: it stays while the lander is near, sinks when it has been gone a while
                    if (lst != 2 && absi(X - ex) < 48 && groundAt(ex) - Y < 80) e.t = 0;
                    else if (e.t >= 90) { e.st = 3; e.t = 0; }
                }
                else if (e.st == 3 && e.t >= 20) { e.st = 0; e.t = 0; e.x = (int16_t)(e.x + (rnd(2) ? 960 : -960)); }   // dives and goes elsewhere
            }
            break;
        }
        case E_MOTHER:                                                        // sways over the collector, fires, sends out saucers
            e.x = (int16_t)((alienX + 20 + isin(e.t + frameNo / 2) / 4) * 16);
            e.y = (int16_t)((34 + WDY + isin(frameNo * 3) / 32) * 16);
            e.vx = e.vy = 0;
            {
                const int v = (stage >> 2) & 3;                               // the boss: 0 mothership, 1 hive, 2 prism warship, 3 dreadnought
                if (lst != 2 && absi(X - ex) < 120 && (frameNo % (v == 3 ? 40 : 60)) == 0) {
                    eshot(ex - 8, ey + 6, X, Y, 20, 1); eshot(ex + 8, ey + 6, X, Y, 20, 1);
                    if (v == 2) eshot(ex, ey + 6, X, Y + 40, 20, 1);          // a spread
                }
                if ((frameNo % 450) == 150 && fliers < 4 && spawnEn(v == 1 || (v == 3 && (frameNo & 256)) ? E_STINGER : v == 2 ? E_MINE : E_UFO, ex, ey + 8)) sfx(S_UFO);
            }
            break;
        case E_TURRET:
            if (lst != 2 && absi(X - ex) < 100 && Y < ey - 16 && (e.t % 70) == 0) eshot(ex, ey - 4, X, Y, 26 + sector * 6, 1);
            break;
        }
        if (!e.kind) continue;
        e.x = (int16_t)(e.x + e.vx); e.y = (int16_t)(e.y + e.vy);
        const int nx = e.x >> 4, ny = e.y >> 4;
        if (e.kind < E_TURRET && e.kind != E_UFO && ny > groundAt(nx) - 6) e.y = (int16_t)((groundAt(nx) - 6) * 16);
        if (e.kind < E_TURRET && absi(nx - X) > 420 && !(e.kind == E_UFO && (e.cargo || e.st != 0))) { e.kind = E_NONE; continue; }
        // touching the lander
        if (e.kind == E_WORM) { if (e.x < 160) e.x = 160; if (e.x > (W - 10) * 16) e.x = (W - 10) * 16; if (e.st != 2) continue; }
        if (lst != 2 && absi(nx - X) < 10 && absi(ny - Y) < (e.kind == E_WORM ? 20 : 9)) {
            if (e.kind == E_UFO) {                                            // a bump: no harm done, the ships bounce apart
                snatch(e, nx, ny);
                lvx = X >= nx ? 160 : -160; if (Y < ny) lvy = -160;   // (from above: up; from below: sideways only, never slammed into the ground)
                e.x = (int16_t)(e.x - lvx / 3);
                if (!bumpT) {
                    if (rnd(5) == 0) for (auto &c : crit) if (!c.kind) { c = { (int16_t)nx, (int16_t)(ny + 4), (int8_t)(X < nx ? 1 : -1), 3, 0 }; say(MS_COW, 50); break; }
                    sfx(S_CLANG); shake = 3; bumpT = 12;
#ifdef SL_TRACE
                    fprintf(stderr, "clang f%u shield %d/%d lvx %d lvy %d\n", (unsigned)frameNo, shield, shMax(), lvx, lvy);
#endif
                }
                continue;
            }
            damage(e.kind == E_MINE ? 2 : 1);
            killEn(e);
        }
    }
}

static NOINL int toward(int v, int t, int step) { return v + clampi(t - v, -step, step); }

static void updateDelivery() {
    if (lidT) lidT--;
    if (tankT && --tankT < 70) { if (tankT == 69) say(MS_TANK_FULL, 70); if ((tankT % 5) == 0) sfx(S_FUEL); fuel = (uint16_t)(tankT ? fuel + (FUEL_MAX - fuel) / (tankT + 1) : FUEL_MAX); }   // the worm bonus: the tank filling, tick by tick
    if (angryT) { angryT--; tiltPh = (uint8_t)(tiltPh + 14 + ((angryT * 7) & 31)); if (angryT % 10 == 0) sfx((angryT & 16) ? S_BLAST : S_CLANG); }   // honks and clangs: furious   // rocking, now quick, now slow
    if (dlvT && !--dlvT) invT = 90;                                     // the cut-scene's last look is over: a moment's safety after it
    if (partyT && (frameNo & 1) && (!--partyT ? (dlvT = 45, false) : (partyT & 7) == 0)) {   // the feast (its clock at half speed): fireworks every half second
        const int fx = basePadX + rnd(81) - 20, fy = 24 + rnd(30);   // over the pad, where the lander sits
        const uint8_t c = (partyT & 16) ? C_GOLD : (partyT & 8) ? C_GREEN : C_RED;
        for (int a = 0; a < 256; a += 21) part(fx, fy, isin(a) / 4, isin(a + 64) / 4, 22, c);
        sfx(S_ZAP);
    }
    const int X = LX(), Y = LY();
    int tx, ty;                                                         // where the speeder is heading
    switch (dlv) {
    case 0: return;
    case 1:                                                             // the bag falls; a speeder sets off from the takeaway
        if ((bagY += 2) >= groundAt(bagX) - 3) { dlv = 2; scX = burgerX + 30; scY = groundAt(scX) - 7; sfx(S_JET); }
        return;
    case 2: tx = bagX; ty = groundAt(scX) - 7; break;                  // low along the ground to the bag
    case 3: tx = X - 12 * scDir; ty = Y - 12; bagX = scX; bagY = scY - 2; if (lst == 2) dlv = 5; break;   // up to the lander with it
    case 4:                                                             // tossed into the open lid
        if (frameNo & 3) { bagX = toward(bagX, X, 1); bagY = toward(bagY, Y - 8, 1); }   // (three frames in four: unhurried)
        if (bagX == X && bagY == Y - 8) {
#ifdef SL_TRACE
            fprintf(stderr, "orderup f%d\n", frameNo);
#endif
            dlv = 5; dlvT = 45; foodOn = 1; sfx(S_CATCH); say(MS_ORDER_UP, 50); }
        return;
    default: tx = scX - scDir * 40; ty = -40; if (scY < -10) dlv = 0; break;   // and away
    }
    if (dlv == 2) scDir = (int8_t)(tx < scX ? -1 : 1);                // (fixed once it has the bag: a side chosen every frame never settled)
    if (frameNo & 3) { scX = toward(scX, tx, 1); scY = toward(scY, ty, 1); }
    if (absi(tx - scX) < 4 && absi(ty - scY) < 4) { if (dlv == 2) dlv = 3; else if (dlv == 3) { dlv = 4; lidT = 40; sfx(S_GEAR); } }
}

// a cut-scene playing (the delivery, the feast, then a pause on the lander): where the camera looks, or -999
static NOINL int cutAt() { return partyT ? (doorX + basePadX + 20) / 2 : (uint8_t)(dlv - 2) < 3 ? scX : dlvT ? LX() : -999; }   // (dlvT: a last look at the lander)

static void updateShots() {
    updateDelivery();
    if (bumpT) bumpT--;
    for (auto &s : shots) {
        if (!s.life) continue;
        s.life--;
        for (int step = 0; step < 7 && s.life; step++) {
            if (s.kind == 3) s.y++; else s.x = (int16_t)(s.x + s.dir);
            if (s.y >= groundAt(s.x) && s.kind != 2) { part(s.x, s.y, -s.dir * 8, -10, 8, C_GOLD); s.life = 0; break; }
            for (auto &e : en) {
                if (!e.kind) continue;
                const int ex = e.x >> 4, ey = e.y >> 4, hw = e.kind == E_UFO ? 11 : e.kind == E_MOTHER ? 22 : 6;
                if (e.kind == E_UFO && ey < TOP) continue;
                if (absi(s.x - ex) <= hw && absi(s.y - ey) <= (s.kind >= 2 ? 8 : 5)) {
                    if (s.kind >= 2) {
                        if (e.hit || (e.kind == E_WORM && e.st == 0)) continue;
                        e.hit = 30; part(s.x, s.y, -s.dir * 10, -8, 10, C_WHITE);
                        if (e.hp <= 4) killEn(e); else { e.hp -= 4; sfx(S_HIT); }
                        continue;
                    }
                    s.life = 0;
                    e.hit = 4;
                    if (s.kind == 1 && e.kind == E_MOTHER) break;                    // a cannon round does nothing to a mothership's armour
                    part(s.x, s.y, -s.dir * 10, -8, 8, C_WHITE);
                    if (e.kind == E_WORM && e.st == 0) { s.life = 1; e.hit = 0; e.hp++; }   // under the sand: the shot goes into the ground
                    if (--e.hp == 0) killEn(e); else { sfx(S_HIT); if (e.kind == E_WORM && e.st == 1) { e.st = 3; e.t = 0; } }
                    break;
                }
            }
        }
    }
    const int X = LX(), Y = LY();
    for (auto &s : es) {
        if (!s.life) continue;
        s.life--;
        s.x = (int16_t)(s.x + s.vx); s.y = (int16_t)(s.y + s.vy);
        const int x = s.x >> 4, y = s.y >> 4;
        if (y >= groundAt(x) || y < TOP) { s.life = 0; continue; }
        if (lst != 2 && absi(x - X) < 7 && absi(y - Y) < 8) { s.life = 0; damage(1); }
        if (dlv == 3 && absi(x - scX) < 8 && absi(y - scY + 4) < 6) { s.life = 0; dlv = 1; boom(scX, scY, 10, 0); sfx(S_BOOM); }   // the bag falls; another speeder will come
    }
    for (auto &q : pt) {
        if (!q.life) continue;
        q.life--;
        q.x = (int16_t)(q.x + q.vx); q.y = (int16_t)(q.y + q.vy);
        if (q.col != C_STEEL && q.col != C_SILVER) q.vy = (int8_t)(q.vy + 1);          // smoke drifts, sparks and debris fall
        if ((q.y >> 4) > groundAt(q.x >> 4)) { q.y = (int16_t)(groundAt(q.x >> 4) * 16); q.vy = (int8_t)(-q.vy / 3); q.vx = (int8_t)(q.vx / 2); }
    }
    for (auto &b : booms) {
#ifdef CHGBA
        enum { BOOM_PARTS = 10 };                                    // the GBA: bigger bursts
#else
        enum { BOOM_PARTS = 6 };
#endif
        if (b.delay) { if (--b.delay == 0) { sfx(S_BOOM); burst(b.x, b.y, BOOM_PARTS, 40, 40, 26, 16, C_GOLD, C_RED); shake = 8; } }
        else if (b.t) b.t--;
    }
}

// ---- weather --------------------------------------------------------------------------------------------------------
static uint8_t wxX[WXN], wxY[WXN];
static int lastCam;
// rain, (ash,) drizzle, acid: how fast a streak falls, its head and its tail
static const uint8_t FALL[5][3] = { { 5, C_SILVER, C_STEEL }, { 0, 0, 0 }, { 2, C_GNDL, C_SKY1 }, { 0, 0, 0 }, { 4, C_GREEN, C_GREEN } };
static void updateWeather() {
    const Planet &p = PLR;
    const int dc = cam - lastCam;
    lastCam = cam;
    windV = isin(frameNo / 3 + stage * 50) * p.wind * (3 + (isin(frameNo * 2) > 60 ? 2 : 0)) * (wxLvl + 1 + stage / 16) / 24;   // the wind, 1/256 px a frame: it veers and gusts, harder in a storm
    const int gust = windV / 2, nWx = WXN * wxLvl / 3;
    for (int i = 0; i < nWx; i++) {
        int x = wxX[i] - dc, y = wxY[i];
        switch (p.weather) {
        case WX_DUST: x += gust / 24 + (i & 1); y += (i % 3) == 0; break;
        case WX_SNOW: {                                             // flakes ride the wind: small ones are blown hard, big ones lag
            const bool big = (i & 3) == 0;
            x += (windV * (big ? 2 : 3 + (i & 3)) + (int)(hash((uint32_t)(frameNo * 64 + i)) & 255)) >> 8;   // a fraction of a pixel, rounded at random
            if (absi(windV) < 50) x += (isin(frameNo * 2 + i * 23) > 40) - (isin(frameNo * 2 + i * 23) < -40);   // still air: they sway
            y += big || (((frameNo + i) & 1) && absi(windV) < 110);  // a strong gust holds the small ones up
            break;
        }
        case WX_ASH: if (i & 1) { y -= 1; x += (frameNo + i) % 3 == 0; } else { y += (frameNo & 1); } break;
        case WX_RAIN: case WX_DRIZZLE: case WX_ACID: y += FALL[p.weather - WX_RAIN][0]; x -= p.weather == WX_RAIN; break;
        default: break;
        }
        while (x < 0) x += WXR;
        while (x >= WXR) x -= WXR;
        if (y >= SH) { y = TOP; x = rnd(WXR); }
        if (y < TOP) { y = SH - 1; x = rnd(WXR); }
        wxX[i] = (uint8_t)x; wxY[i] = (uint8_t)y;
    }
    if (!lightT && wxLvl >= 2 && p.weather != WX_SNOW && p.weather != WX_GLINT && p.weather != WX_METEOR && rnd(wxLvl == 3 ? 150 : 500) == 0) { lightT = 8; sfx(S_BOOM); }   // lightning
}

static void drawWeather() {
    const Planet &p = PLR;
    const int nWx = WXN * wxLvl / 3;
#ifdef SL_TRACE
    if (frameNo % 60 == 0) { int n = 0; for (int i = 0; i < WXN; i++) n += wxX[i] < SW; fprintf(stderr, "wx f%d weather%d on%d wind%d:", frameNo, p.weather, n, windV); for (int i = 0; i < 20; i++) fprintf(stderr, " %d,%d", wxX[i], wxY[i]); fprintf(stderr, "\n"); }
#endif
    for (int i = 0; i < nWx; i++) {
        const int x = wxX[i], y = wxY[i];
        if (x >= SW) continue;
        switch (p.weather) {
        case WX_DUST: pset(x, y, (i & 1) ? C_GNDL : C_SKY1); pset(x - 1, y, C_SKY1); if (i & 2) pset(x - 2, y, C_GNDL); break;
        case WX_SNOW: pset(x, y, C_WHITE); if ((i & 3) == 0) { pset(x + 1, y, C_WHITE); pset(x, y + 1, C_WHITE); pset(x + 1, y + 1, C_SILVER); } if (absi(windV) > 80) pset(x - sgn(windV), y, C_SILVER); break;   // a streak in a gale
        case WX_RAIN: case WX_DRIZZLE: case WX_ACID: {                // a falling streak: its speed and colours from the table
            const uint8_t *f = FALL[p.weather - WX_RAIN];
            pset(x, y, f[1]); pset(x + (p.weather == WX_RAIN), y - 1, f[2]); pset(x + (p.weather == WX_RAIN), y - 2, f[2]);
            break;
        }
        case WX_ASH: if (i & 1) pset(x, y, ((frameNo + i) & 4) ? C_GOLD : C_RED); else pset(x, y, C_STEEL); break;
        case WX_GLINT: {
            const int ph = (frameNo + i * 11) & 31;
            if (ph < 6) { const uint8_t c = ph < 3 ? C_WHITE : C_ACC; pset(x, y, c); if (ph == 1 || ph == 2) { pset(x - 1, y, C_ACC); pset(x + 1, y, C_ACC); pset(x, y - 1, C_ACC); pset(x, y + 1, C_ACC); } }
            break;
        }
        default: break;
        }
    }
}

// ---- the scene ------------------------------------------------------------------------------------------------------
static uint8_t topA[4][SW];                       // each layer's top per screen column: far, mid, near, ground

#ifdef CHGBA
void gba_skyGradient(int y0, int y1);   // gba/src/gfx_gba.cpp: the backdrop's gradient, from SKY0 at y0 to SKY1 then haze at y1
#endif
static void drawSky() {
#ifdef CHGBA
    // the GBA paints the sky itself, smoothly (a colour each screen line, behind the picture): here it is just left clear
    int low = TOP;
    for (int x = 0; x < SW; x++) if (topA[0][x] > low) low = topA[0][x];
    for (int y = TOP; y < low; y++) memset(gfx_fb + y * GFX_FB_STRIDE, C_SKY0 << 4 | C_SKY0, GFX_FB_STRIDE);
    gba_skyGradient(TOP + SKYX, TOP + SKYX + 80);
#else
    // a dithered gradient: SKY0 above, SKY1 lower down, then SKY1 into FAR towards the horizon (a haze); only down to the
    // far hills' lowest point, as everything under it is covered
    int low = TOP;
    for (int x = 0; x < SW; x++) if (topA[0][x] > low) low = topA[0][x];
    for (int y = TOP; y < low; y++) {
        const int lv = y < TOP + SKYX ? 0 : (y - TOP - SKYX) * 7 / 80;   // (the extra sky: the top colour)
        const uint8_t s0 = (C_SKY0 << 4) | C_SKY0, s1 = (C_SKY1 << 4) | C_SKY1, d01 = (C_SKY1 << 4) | C_SKY0, d10 = (C_SKY0 << 4) | C_SKY1;
        const uint8_t f1 = (C_FAR << 4) | C_SKY1, f2 = (C_SKY1 << 4) | C_FAR;
        uint8_t v;
        switch (lv) {
        case 0: case 1: v = s0; break;
        case 2: v = (y & 1) ? d01 : s0; break;
        case 3: v = (y & 1) ? d01 : d10; break;
        case 4: v = (y & 1) ? d01 : s1; break;
        case 5: case 6: v = s1; break;
        default: v = (y & 1) ? ((y & 2) ? f1 : s1) : s1; break;
        }
        memset(gfx_fb + y * GFX_FB_STRIDE, v, GFX_FB_STRIDE);
    }
#endif
}


static void drawSkyObjects() {
    const Planet &p = PLR;
    const int pc = cam >> 6;
    if (p.sky == SK_EARTH || p.sky == SK_AURORA || p.sky == SK_MOONS || p.sky == SK_TWINSUN) {    // stars
        const int sc = cam >> 5;
        for (int i = 0; i < 40; i++) {
            const uint32_t h = hash((uint32_t)i * 7777u + 1u);
            const int x = (int)((h & 255) + 512 - (uint32_t)sc) & 255, y = TOP + 2 + (int)((h >> 8) % 60);
            if (x < SW) pset(x, y, ((h >> 16) & 7) == (uint32_t)((frameNo >> 3) & 7) ? C_SILVER : ((h >> 20) & 1) ? C_WHITE : C_STEEL);
        }
    }
    switch (p.sky) {
    default: {                                                  // a planet, a sun or moons: a picture from the card
        uint8_t mp[14];
        remap(mp, RM(7, C_SKY0) | RM(9, C_FAR) << 8 | RM(12, C_GNDL) << 16);
        const CSpr &c = PK.sky;
        blit(c, SKY_AT[p.sky][0] - pc - c.w / 2, SKY_AT[p.sky][1] + (WDY + HUDX) / 2 - c.h / 2, false, 255, mp);
        break;
    }
    case SK_AURORA:                                             // curtains of light, shimmering
        for (int x = 0; x < SW; x++) {
            const int wx = x + (cam >> 5), top = TOP + 6 + (isin(wx * 3 + frameNo) >> 5) + (isin(wx * 7 - frameNo * 2) >> 6);
            const int len = 10 + (isin(wx * 5 + frameNo / 2) >> 5);
            for (int y = top; y < top + len; y++) if (((x + y + (frameNo >> 1)) & 1) == 0 || y < top + 3) pset(x, y, (y - top) < 4 ? C_GREEN : C_ACC);
        }
        break;
    }
    {                                                           // the world's sky giant, drifting across far away
        uint8_t mp[14];
        remap(mp, RM(7, C_MID) | RM(9, C_FAR) << 8 | RM(12, C_GNDL) << 16);
        const CSpr &g = PK.giant;
        const int gx = 150 - (int)(((uint32_t)frameNo / 3 + stage * 97u) % 420u), gy = (stage & 7) == 1 ? 50 + WDY : TOP + 14 + isin(frameNo + stage * 30) / 20;
        if ((stage & 7) != 1 || ((frameNo >> 8) & 1)) blit(g, gx, gy, false, 255, mp);   // the far worm only shows now and then
    }
    if (p.clouds) {                                             // dithered cloud banks drifting slowly
        const int cc = (cam >> 4) + (frameNo >> 3);
        for (int k = (cc >> 6) - 1; k <= ((cc + SW) >> 6) + 1; k++) {
            const uint32_t h = hash((uint32_t)k * 4243u + planet);
            const int cx = k * 64 - cc + (int)(h & 15), cy = TOP + 10 + (int)((h >> 4) & 31), hw = 16 + (int)((h >> 9) & 15), hh = 3 + (int)((h >> 13) & 3);
            for (int r = -hh; r <= hh; r++) {
                const int w = hw - (hw * r * r) / (hh * hh + 1) - (r < 0 ? -r * 2 : 0);
                if (w <= 0) continue;
                const int y = cy + r;
                hl(cx - w + 3, y, 2 * w - 6, p.clouds);
                for (int x = cx - w; x < cx - w + 3; x++) if ((x + y) & 1) pset(x, y, p.clouds);
                for (int x = cx + w - 3; x < cx + w; x++) if ((x + y) & 1) pset(x, y, p.clouds);
            }
        }
    }
    if (lightT > 4) {                                          // a lightning bolt from the clouds
        int x = 30 + (frameNo * 37) % 70;
        for (int y = TOP + 8; y < 80 + WDY; y++) { x += ((hash((uint32_t)(y + frameNo)) & 3) == 0) - ((hash((uint32_t)(y * 3 + frameNo)) & 3) == 0); pset(x, y, PLR.weather == WX_ACID ? C_GREEN : C_WHITE); }
    }
}

// a layer's colours: its own, a lighter one for slopes facing the light, a darker one for the others
static const uint8_t LBODY[3] = { C_FAR, C_MID, C_NEAR }, LLIT[3] = { C_SKY1, C_FAR, C_MID }, LDARK[3] = { C_MID, C_NEAR, C_GNDD };

// each layer's tops (3 the ground) are kept from frame to frame: when the camera moves they slide along and only the
// columns coming into view are worked out (the far layer, at 1/8 of the speed, mostly does not move at all)
static void layerTops(int L) {
    const int lc = L == 3 ? cam : cam >> (3 - L), d = lc - topsAt[L];
    if (!d) return;
    int x0 = 0, x1 = SW;
    if (d > 0 && d < SW) { memmove(topA[L], topA[L] + d, SW - d); x0 = SW - d; }
    else if (d < 0 && d > -SW) { memmove(topA[L] - d, topA[L], SW + d); x1 = -d; }
    for (int x = x0; x < x1; x++) topA[L][x] = (uint8_t)clampi(L == 3 ? groundAt(lc + x) : layerH(L, lc + x), TOP, SH - 1);
    topsAt[L] = lc;
}

// fill one layer's columns from its top down to the next layer's top: slope lighting and a stone texture
// the inner loops of the scenery: a run of one colour, or of colours looked up from the column's texture
RAMFUNC(colFill) static void colFill(uint8_t *p, uint8_t keep, uint8_t v, int n) { for (; n > 0; n--, p += GFX_FB_STRIDE) *p = (uint8_t)((*p & keep) | v); }
RAMFUNC(colTex) static void colTex(uint8_t *p, uint8_t keep, int sh, int y, int y1, const uint8_t *lut, const uint8_t *tc) {
    for (; y < y1; y++, p += GFX_FB_STRIDE) *p = (uint8_t)((*p & keep) | (lut[tc[y & 15]] << sh));
}

// fill one layer's columns from its top down to the next layer's top: a lit (or shaded) rim on its slopes, a stone texture below
static void drawLayerCols(int L) {
    const uint8_t body = LBODY[L], lit = LLIT[L], dark = LDARK[L];
    const uint8_t lut[4] = { dark, body, body, L == 2 ? lit : body };
    const int lc = cam >> (3 - L);
    for (int x = 0; x < SW; x++) {
        const int y0 = topA[L][x], y1 = topA[L + 1][x];
        if (y0 >= y1) continue;
        const int slope = x ? (int)topA[L][x - 1] - y0 : 0;            // > 0: rising to the right, facing the light
        const int sh = (x & 1) ? 4 : 0;
        const uint8_t keep = (x & 1) ? 0x0F : 0xF0;
        uint8_t *p = gfx_fb + y0 * GFX_FB_STRIDE + (x >> 1);
        int rim = slope > 0 ? 2 + slope * 2 : slope < 0 ? 2 - slope : 1;
        if (rim > y1 - y0) rim = y1 - y0;
        colFill(p, keep, (uint8_t)((slope < 0 ? dark : lit) << sh), rim);
        p += rim * GFX_FB_STRIDE;
        if (L) colTex(p, keep, sh, y0 + rim, y1, lut, tile[(lc + x) & 15]);
        else colFill(p, keep, (uint8_t)(body << sh), y1 - y0 - rim);
    }
}

// two ground pixels a byte: neither has to be read back
RAMFUNC(colPair) static void colPair(uint8_t *p, int y, int y1, const uint8_t *la, const uint8_t *ta, const uint8_t *lb, const uint8_t *tb) {
    for (; y < y1; y++, p += GFX_FB_STRIDE) *p = (uint8_t)(la[ta[y & 15]] | (lb[tb[y & 15]] << 4));
}

// the ground in bands below its surface: the lit edge, a light skin, then darker and darker rock with the world's texture
static void drawGroundCols() {
    const uint8_t crack = PLR.crack != C_INK ? PLR.crack : C_GNDD;                  // some worlds' seams glow
    const uint8_t LUT[5][4] = { { C_GNDL, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GNDL, C_GNDL, C_GNDL }, { C_GND, C_GND, C_GND, C_GNDL },
                                { crack, C_GND, C_GND, C_GND }, { crack, C_GNDD, C_GND, C_GNDD } };
    static const uint8_t EDGE[6] = { 0, 1, 3, 12, 28, 255 };                        // where each band starts below the surface
    for (int xb = 0; xb < SW / 2; xb++) {
        const int y0[2] = { topA[3][xb * 2], topA[3][xb * 2 + 1] };
        const uint8_t *tc[2] = { tile[(cam + xb * 2) & 15], tile[(cam + xb * 2 + 1) & 15] };
        uint8_t *col = gfx_fb + (xb);
        const int hi = y0[0] < y0[1] ? 0 : 1, yTop = y0[hi], yBoth = y0[hi ^ 1];
        for (int b = 0; b < 5 && yTop + EDGE[b] < yBoth; b++) {                    // where only one of the two is ground yet
            const int ya = yTop + EDGE[b], yb = yTop + EDGE[b + 1] < yBoth ? yTop + EDGE[b + 1] : yBoth;
            colTex(col + ya * GFX_FB_STRIDE, hi ? 0x0F : 0xF0, hi ? 4 : 0, ya, yb, LUT[b], tc[hi]);
        }
        int y = yBoth, ba = 0, bb = 0;                                            // both: a byte at a time
        while (ba < 4 && y0[0] + EDGE[ba + 1] <= y) ba++;
        while (bb < 4 && y0[1] + EDGE[bb + 1] <= y) bb++;
        while (y < SH) {
            const int na = y0[0] + EDGE[ba + 1], nb = y0[1] + EDGE[bb + 1];
            int ye = na < nb ? na : nb;
            if (ye > SH) ye = SH;
            colPair(col + y * GFX_FB_STRIDE, y, ye, LUT[ba], tc[0], LUT[bb], tc[1]);
            y = ye;
            if (y >= na && ba < 4) ba++;
            if (y >= nb && bb < 4) bb++;
        }
    }
}

// ---- structures on the background layers ----------------------------------------------------------------------------
// a background building from the card, in the layer's colours, with its light animated
static void structure(int n, int sx, int base, int L, uint32_t h) {
    const CSpr &c = PK.st[n];
    uint8_t mp[14];
    remap(mp, RM(2, LDARK[L]) | RM(3, LBODY[L]) << 8 | RM(4, LLIT[L]) << 16);
    const int x = sx, y = base - c.h + 1;
    blit(c, x, y, ((h >> 22) & 1) && n != 3, 255, mp);
    const int8_t *li = STRUCT_LIGHT[PLR.theme - 1][n];
    const int lx = x + li[0], ly = y + li[1];
    switch (li[2]) {
    case 1: pset(lx, ly, ((frameNo + (int)h) & 16) ? C_RED : C_GOLD); break;                       // a beacon
    case 2: for (int r = 0; r < 3; r++) R(lx - 3, ly - ((frameNo / 2 + r * 12) % 36), 6, 1, C_ACC); break;   // rings of light climbing
    case 3:                                                                                         // an arc between pylons
        if ((frameNo & 7) < 5) for (int k = 0, yy = ly; k < 18; k++) { yy = clampi(yy + (int)(hash((uint32_t)(k + frameNo / 2 * 31)) % 3) - 1, ly - 4, ly + 4); pset(lx + k, yy, (k & 1) ? C_ACC : C_WHITE); }
        break;
    case 4: { const uint8_t e = ((frameNo + (int)h) & 63) < 40 ? C_GOLD : C_RED; pset(lx, ly, e); pset(lx + 6, ly, e); break; }   // eyes that glow
    }
}

static void drawStructures(int L) {
    const Planet &p = PLR;
    if (p.theme == TH_MOON) return;
    if (L == 0) {
        if (p.style[0] == ST_CONES) {                                   // volcanoes: lava at the craters, blobs thrown up
            const int lc = cam >> 3;
            for (int k = (lc >> 7) - 1; k <= ((lc + SW) >> 7) + 1; k++) {
                if ((hash((uint32_t)planet * 7919u + (uint32_t)k * 31u) & 3) != 0) continue;
                const int sx = k * 128 + 64 - lc, top = layerH(0, k * 128 + 64);
                R(sx - 3, top + 1, 7, 2, C_ACC); R(sx - 1, top, 3, 1, C_GOLD);
                for (int i = 0; i < 4; i++) {
                    const int ph = (frameNo * 2 + i * 23) % 60, bx = sx + (i - 2) * ph / 10, by = top - ph + ph * ph / 34;
                    if (by < top) pset(bx, by, ph < 30 ? C_GOLD : C_ACC);
                }
                for (int d = 2; d < 14; d++) pset(sx + ((d * (k & 1 ? 1 : -1)) >> 1), top + d, (d + frameNo / 4) & 3 ? C_ACC : C_RED);   // a lava flow
            }
        }
        if (p.theme == TH_RUINS && p.style[0] == ST_SPIKES) {           // capstones glint on the pyramids
            const int lc = cam >> 3, s2 = LSH[0] - 1;
            for (int k = (lc >> s2); k <= ((lc + SW) >> s2); k++) {
                const int sx = (k << s2) + (1 << (s2 - 1)) - lc, top = layerH(0, (k << s2) + (1 << (s2 - 1)));
                if (top < 64) { pset(sx, top, C_GOLD); pset(sx - 1, top + 1, C_GOLD); pset(sx, top + 1, C_GOLD); }
            }
        }
        return;
    }
    const int cw = L == 1 ? 96 : 80, lc = cam >> (3 - L);
    for (int k = (lc / cw) - 1; k <= (lc + SW) / cw + 1; k++) {
        const uint32_t h = hash((uint32_t)k * 2246822519u + (uint32_t)L * 3266489917u + planet);
        if ((h & 3) == 0) continue;
        const int lxp = k * cw + 8 + (int)((h >> 4) % (uint32_t)(cw - 40));
        const int sx = lxp - lc;
        if (sx < -40 || sx > SW + 12) continue;
        const int base = layerH(L, lxp + 6) + 3;
        const int ty = (int)((h >> 12) % 4);
        if (L == 2 && ((h >> 18) & 3) == 0) {                          // a billboard on two legs
            const char *t = PLR.ads[(h >> 20) % 3];
            const int w = text35Width(t) + 6;
            R(sx + 3, base - 16, 1, 16, LDARK[L]); R(sx + w - 4, base - 16, 1, 16, LDARK[L]);
            adBoard(sx, base - 27, t, h);
            continue;
        }
        structure((int)((h >> 12) % 6), sx, base, L, h);
    }
}

// ---- things on the ground: pads, the base, the Moon's flag and footprints, turrets ----------------------------------
static void drawPad(int px, bool base) {
    const int sx = px - cam, gy = groundAt(px + 20);
    if (sx < -48 || sx > SW + 2) return;
    R(sx, gy, 40, 3, C_STEEL); R(sx, gy, 40, 1, C_SILVER);
    for (int k = 4; k < 36; k += 8) R(sx + k, gy + 1, 4, 1, C_GOLD);
    const uint8_t lamp = base ? ((frameNo & 8) ? C_GREEN : C_ACC) : ((frameNo & 8) ? C_RED : C_GOLD);
    R(sx - 1, gy - 6, 1, 6, C_STEEL); R(sx + 40, gy - 6, 1, 6, C_STEEL);
    pset(sx - 1, gy - 7, lamp); pset(sx + 40, gy - 7, lamp);
    if (base) {                                                     // the fuel pump
        const int fx = baseRight ? sx + 42 : sx - 6;
        blit(C_PUMP, fx, gy - 9, false);
        if (lst == 1 && onPad == 2 && (frameNo & 4)) R(fx + 1, gy - 6, 2, 2, C_GREEN);                // pumping
        text35(sx + 8, gy + 4, "BASE", C_GOLD);
    } else {                                                        // a launch gantry
        const int gx = baseRight ? sx - 10 : sx + 44;
        blit(C_GANTRY, gx, gy - 32, false);
        if (frameNo & 16) pset(gx + 2, gy - 32, C_STEEL);
    }
}

static void windsock(int x, int gy) {
    if (x < -12 || x > SW + 8) return;
    R(x, gy - 12, 1, 12, C_STEEL);
    const int dir = windV < 0 ? -1 : 1, len = 2 + absi(windV) / 32;
    for (int k = 1; k <= len; k++) pset(x + dir * k, gy - 12 + (len < 4 ? k / 2 : 0), ((k + 1) >> 1) & 1 ? C_RED : C_WHITE);   // stiff in a gale, drooping in a breeze
}

static void drawMoonTableau() {
#ifndef SL_SLIM
    // a flag on its pole (rippling: the crossbar holds it out), an old descent stage and boot prints in the dust
    const int fxw = baseRight ? startPadX + 74 : startPadX - 34, fx = fxw - cam;
    if (fx < -60 || fx > SW + 52) return;
    const int gy = groundAt(fxw);
    flag(fx, gy, 2); R(fx, gy - 25, 13, 1, C_SILVER);                      // the flag, held out by its crossbar
    blitBC(C_DESCENT, fx + (baseRight ? 28 : -20), groundAt(fxw + (baseRight ? 28 : -20)), !baseRight);   // the descent stage left behind
    for (int wx = fxw - 70; wx < fxw + 70; wx += 3) {               // boot prints from the pad to the flag and around
        const int s = wx - cam;
        if ((unsigned)s >= SW || (hash((uint32_t)wx) & 7) == 0) continue;
        const int y = groundAt(wx) + 1 + ((wx / 3) & 1);
        pset(s, y, C_GNDD); pset(s + 1, y, C_GNDD);
    }
#endif
}

static void drawItems() {
    for (const auto &it : items) {
        if (it.st != 1) continue;
        const int x = it.x - cam - 3, y = groundAt(it.x) - 5;
        if (x < -8 || x > SW + 2) continue;
        static const uint8_t BODY[4] = { C_GOLD, C_STEEL, C_RED, C_ACC };
        R(x, y, 7, 5, BODY[it.kind]); R(x, y, 7, 1, C_WHITE); R(x + 6, y + 1, 1, 4, C_INK);
        if (it.kind == IT_SUPPLY) { R(x + 3, y + 1, 1, 3, C_RED); R(x + 2, y + 2, 3, 1, C_RED); }   // a red cross
        else if (it.kind == IT_REPAIR) { pset(x + 2, y + 2, C_WHITE); pset(x + 3, y + 3, C_WHITE); pset(x + 4, y + 2, C_WHITE); }
        else if (it.kind == IT_FUEL) R(x + 1, y + 2, 5, 1, C_WHITE);
        else pset(x + 3, y + 2, (frameNo & 4) ? C_WHITE : C_GOLD);
        if ((frameNo & 16) && absi(it.x - LX()) < 60) pset(x + 3, y - 3, C_GOLD);   // a beacon when the lander is near
    }
}

struct Gore { int16_t x, y, tx, ty; uint8_t t; };                  // blood and guts flying at the screen: 1-20 in flight (growing), 21-90 splatted, dripping
static Gore gore[8];
static void throwGore(int sx, int sy) { for (auto &g : gore) if (!g.t) { g = { (int16_t)sx, (int16_t)sy, (int16_t)(8 + rnd(SW - 16)), (int16_t)(20 + rnd(SH - 40)), 1 }; break; } }
static void drawGore() {
    for (auto &g : gore) {
        if (!g.t) continue;
        if (g.t <= 20) {                                              // coming at you: closer, bigger
            const int x = g.x + (g.tx - g.x) * g.t / 20, y = g.y + (g.ty - g.y) * g.t / 20, r = 1 + g.t / 3;
            R(x - r, y - r / 2, 2 * r, r, C_RED); R(x - r / 2, y - r, r, 2 * r, C_RED); if (g.t & 2) pset(x, y, C_STEEL);
        } else {                                                      // splat: a blot with runs of blood sliding down, fading
            const int r = 8, d = (g.t - 20) / 3;
            if (g.t < 80 || (frameNo & 1)) {
                R(g.tx - r, g.ty - r / 2, 2 * r, r, C_RED); R(g.tx - r / 2, g.ty - r, r, 2 * r, C_RED); R(g.tx - 2, g.ty - 2, 3, 2, C_STEEL);
                R(g.tx - 5, g.ty, 2, r / 2 + d, C_RED); R(g.tx + 3, g.ty, 1, r / 2 + d * 2 / 3, C_RED); R(g.tx, g.ty, 1, r / 2 + d / 2, C_RED);
            }
        }
        if (++g.t > 90) g.t = 0;
    }
}

static void drawPointer() {                                 // an arrow at the edge of the screen: the base when full, else the nearest colonist
    if (state != PLAY || lst == 2 || !(frameNo & 8)) return;
    int tx = -1;
    uint8_t c = C_WHITE;
    const int i = nearestIdle(LX(), 9999);
    if (seats >= SEATS || (seats && i < 0)) { tx = basePadX + 20; c = C_ACC; }
    else if (i >= 0) tx = men[i].x;
    for (const auto &e : en) if (e.kind == E_WORM && e.st == 4 && e.cargo >= 34) { tx = alienX + 20; c = C_RED; }   // a worm on the rope: to the aliens' collector
    for (const auto &cw : crit) if (cw.kind == 6) { tx = basePadX + 20; c = C_ACC; }                                // a cow on the rope: home
    if (tx < 0 || (tx >= cam && tx < cam + SW)) return;
    const int left = tx < cam, x = left ? 1 : SW - 2, y = 70 + WDY;
    for (int k = 0; k < 4; k++) R(left ? x + k : x - k, y - k, 1, 2 * k + 1, c);
}

static void drawGroundThings() {
    drawPlants();
    drawCritters();
    drawPad(startPadX, false);
    drawPad(basePadX, true);
    if (PLR.wind) { windsock(startPadX + (baseRight ? 46 : -6) - cam, groundAt(startPadX)); windsock(basePadX + (baseRight ? -6 : 46) - cam, groundAt(basePadX)); }
    const int bx = buildX - cam;
    if (bx > -50 && bx < SW + 2) {
        const int gy = groundAt(buildX + 22);
        uint8_t mp[14];                                                  // the base in this world's colours
        remap(mp, RM(5, PLR.basec));
        blit(PK.base, bx, gy - PK.base.h + 1, !baseRight, 255, mp);
        flag(baseRight ? bx - 4 : bx + 46, gy, 0);
    }
    drawAlienBase();
    drawItems();
    drawDiner();
    if (planet == 0) drawMoonTableau();
}

// a flag on a pole, rippling: the colony's (a gold star) or the aliens' (an eye)
static void flag(int x, int gy, uint8_t kind) {
    if (x < -14 || x > SW + 2) return;
    R(x, gy - 24, 1, 24, C_SILVER); pset(x, gy - 25, C_GOLD);
    static const int8_t WAVE[8] = { 0, 0, 1, 1, 1, 0, 0, -1 };
    for (int c = 0; c < 11; c++) {
        const int off = c > 1 ? WAVE[(c + (frameNo >> 2)) & 7] : 0;
        for (int r = 0; r < 7; r++) {
            uint8_t col;
            if (kind == 2) col = (c < 5 && r < 4) ? ((((c + r) & 1) && c && r && r < 3) ? C_WHITE : C_ACC) : (r & 1) ? C_WHITE : C_RED;   // the stars and stripes
            else if (kind) {
                const int d = (c - 5) * (c - 5) + (r - 3) * (r - 3) * 2;
                col = d == 0 ? C_INK : d < 3 ? C_RED : d < 9 ? C_WHITE : C_GREEN;
            } else {
                col = (r == 0 || r == 6) ? C_ACC : C_WHITE;
                if ((c == 5 && r >= 2 && r <= 4) || (r == 3 && c >= 4 && c <= 6)) col = C_GOLD;
            }
            pset(x + 1 + c, gy - 24 + r + off, col);
        }
    }
}

// the aliens' collecting bucket: its tubes run down to pods in a chamber under the ground
static void drawAlienBase() {
    const int bx = alienX - cam;
    if (bx < -50 || bx > SW + 2) return;
    const int gy = groundAt(alienX + 20), cy = gy + 9;
    const uint8_t *TX = COLL_TUBE[PLR.coll];
    if (cy < SH) {
        R(bx + 1, cy, 38, 15, C_INK); R(bx + 1, cy, 38, 1, C_STEEL); R(bx + 1, cy + 14, 38, 1, C_STEEL);
        R(bx + 1, cy, 1, 15, C_STEEL); R(bx + 38, cy, 1, 15, C_STEEL);
        for (int k = 0; k < 4; k++) {                                   // pods, glowing
            const int px = bx + 5 + k * 8;
            R(px, cy + 4, 6, 9, C_STEEL); R(px + 1, cy + 5, 4, 7, ((frameNo >> 3) + k) & 3 ? C_GREEN : C_WHITE);
            pset(px + 2, cy + 7, C_INK); pset(px + 3, cy + 7, C_INK);
        }
        for (int k = 3; k < 37; k += 5) pset(bx + k, cy + 2, ((frameNo >> 2) + k) & 4 ? C_RED : C_GOLD);
    }
    for (int t = 0; t < 3; t++) R(bx + TX[t], gy, 2, cy - gy, C_STEEL);
    blit(PK.coll, bx, gy - PK.coll.h + 1, false);
    if (tubeT) for (int t = 0; t < 3; t++) {                           // green slime pulsing down the tubes
        const int y = gy - 12 + ((45 - tubeT) * 2 + t * 3) % 22;
        R(bx + TX[t], y, 2, 3, C_GREEN); pset(bx + TX[t], y + 1, C_WHITE);
    }
    if (tubeT && (frameNo & 2)) R(bx + 6, gy - COLL_TOP[PLR.coll] + 1, 28, 1, C_WHITE);       // a splash as one goes in
    flag(bx + 39, gy, 1);
}

// the Moon's drive-through diner: a lit neon sign, an open sign, arrows down the lane
static void drawDiner() {
    if (burgerX < 0) return;
    const int bx = burgerX - cam;
    if (bx < -60 || bx > SW + 12) return;
    const int gy = groundAt(burgerX + 30), top = gy - 37;
    uint8_t mp[14];                                                      // the shop's own colour
    remap(mp, RM(6, PLR.shopc));
    blit(PK.shop, bx, top, false, 255, mp);
    blit(PK.food, bx + 44, top + 1, false);
    const bool flicker = (hash((uint32_t)(frameNo >> 1)) & 63) == 0;
    if (!flicker) text35(bx + 23 - text35Width(PLR.shop) / 2, top + 5, PLR.shop, (frameNo & 32) ? C_GOLD : C_WHITE);
    for (int k = 0; k < 46; k += 2) {                                   // chasing bulbs round the board
        const uint8_t c = ((k / 2 + (frameNo >> 2)) & 3) == 0 ? C_GOLD : C_WHITE;
        pset(bx + k, top + 2, c); pset(bx + 45 - k, top + 12, c);
    }
    if ((frameNo & 63) < 50) text35(bx + 7, top + 28, "OPEN", C_RED);
    for (int k = 0; k < 4; k++) {                                       // the lane
        const int ax = bx + 46 + k * 16, ay = groundAt(burgerX + 46 + k * 16);
        pset(ax + 1, ay + 1, C_GOLD); pset(ax, ay + 2, C_GOLD); pset(ax + 1, ay + 3, C_GOLD);
    }
}

// the alien wildlife on the ground: rocks and plants of each world, swaying; an eye plant watches the lander
static bool busyAt(int x) {
    if (absi(x - startPadX - 20) < 34 || absi(x - basePadX - 20) < 34 || absi(x - buildX - 22) < 30 || absi(x - alienX - 20) < 26) return true;
    if (burgerX >= 0 && x > burgerX - 6 && x < burgerX + 116) return true;
    for (int z = 0; z < nZones; z++) if (absi(x - zoneX[z]) < 32) return true;
    return false;
}
static void plant(uint8_t k, int x, int gy, uint32_t h) {          // from the card; the ground's tones are this world's
    uint8_t mp[14];
    remap(mp, RM(9, C_GND) | RM(12, C_GNDL) << 8 | RM(7, C_GNDD) << 16);
    blitBC(PK.plant[k][((frameNo >> 4) + (int)(h >> 9)) & 1], x, gy + 1, (h >> 11) & 1, 255, mp);
}
static void drawPlants() {
    for (int c = cam / 9 - 1; c <= (cam + SW) / 9 + 1; c++) {
        const uint32_t h = hash((uint32_t)c * 2654435761u + (uint32_t)stage * 97u);
        if (((h >> 3) & 3) == 0 && !(planet == 0 && ((h >> 5) & 1))) continue;
        if (((h >> 3) & 3) == 1) continue;
        const int x = c * 9 + (int)(h & 7);
        if (busyAt(x)) continue;
        plant((uint8_t)((h >> 16) % 3), x - cam, groundAt(x), h);
    }
}

// background wildlife: fliers and crawlers that live their own lives (nothing touches them)
static void updateCritters() {
    for (auto &c : crit) {
        if (!c.kind) {
            if ((PLR.fly || PLR.crawl) && rnd(90) == 0) {
                const bool left = rnd(2);
                c.kind = (uint8_t)(PLR.fly && PLR.crawl ? 1 + rnd(2) : PLR.fly ? 1 : 2); c.x = (int16_t)(cam + (left ? -8 : 136)); c.vx = (int8_t)(left ? 1 : -1); c.t = (uint8_t)rnd(255);
                c.y = (int16_t)(c.kind == 1 ? TOP + 10 + rnd(36) : groundAt(c.x));
            }
            continue;
        }
        c.t++;
        if (c.kind >= 6) {                                              // a cow in its harness: 6 on the rope, 7 let down at the base, 8 led in by the crew
            const int d = sgn(doorX - c.x);
            if (c.kind == 6) {
                if (lst == 2) { c.kind = 3; c.t = 0; hoverT = 0; continue; }  // the lander lost: down it drops
                c.x = (int16_t)LX();
                if ((c.t & 63) == 0) sfx(S_MOO);                             // mooing all the way
                c.y = (int16_t)(c.y + clampi(LY() + 22 - c.y, -1, 4));       // winched up a little, then along under the lander
                if (absi(c.x - basePadX - 20) < 24) c.kind = 7;               // over the base: let down
            } else if (c.kind == 7) { c.x = (int16_t)LX(); if ((c.t & 3) == 0) c.y++; }   // slowly down the rope, still under the lander
            else if (c.t / 2 >= absi(c.x - doorX) && patT) { if (--patT == 30) sfx(S_MOO); c.t--; }   // a pat on the head first
            else if (c.t / 2 >= absi(c.x - doorX) && (c.t & 1)) { c.x = (int16_t)(c.x + d); c.t--; }   // the crew come out, take the cow and lead it in
            if (c.y >= groundAt(c.x)) { c.y = (int16_t)groundAt(c.x); if (c.kind == 7) { c.kind = 8; c.t = 0; c.vx = (int8_t)d; patT = 60; ropeL = (uint8_t)clampi(c.y - LY() - 16, 0, 255); } }
            if (!d && c.kind == 8) { c.kind = 0; hoverT = 0; cowsN++; addScore(1000, doorX, c.y - 20); say(MS_COWS_HOME, 60); sfx(S_SAVED); tankT = 100; shield = shMax(); }   // home with the colonists: steaks up, and the lander topped up
            continue;
        }
        if (c.kind == 4 && lst == 0 && hoverT < 255 && absi(c.x - LX()) < 16 && c.y - LY() < 56) {   // hovering over a cow: beeps, then down comes the rope
            hov = true;
            if ((++hoverT & (hoverT > 36 ? 7 : 15)) == 1) sfx(S_WARN);     // beeps, quicker as the rope comes
            if (hoverT >= 54) { c.kind = 6; hoverT = 255; say(MS_COW_ABOARD, 150); continue; }
        }
        if (c.kind >= 3) {                                              // a cow: tumbles down, then wanders and grazes
            if (c.kind == 5) {                                              // in a beam
                const Enemy &e = en[c.vx];
                if (e.kind != E_UFO || rnd(160) == 0) { c.kind = 3; c.vx = 1; c.t = 0; continue; }   // (its saucer gone, or a kick)
                c.x = (int16_t)(e.x >> 4);
                if (--c.y < (e.y >> 4) + 4) c.kind = 0;                       // gone
                continue;
            }
            if (c.kind == 4) for (int k = 0; k < ENS; k++) {                  // a saucer passing over: up it goes, now and then
                const Enemy &e = en[k];
                if (e.kind == E_UFO && e.st == 0 && absi((e.x >> 4) - c.x) < 4 && rnd(8) == 0) { c.kind = 5; c.vx = (int8_t)k; break; }
            }
            if (c.kind == 5) continue;
            if (c.kind == 3) c.y = (int16_t)(c.y + 1 + c.t / 8);
            else if ((c.t & 127) < 40 && (c.t & 3) == 0) { if (c.x < 30 || c.x > W - 30 || rnd(16) == 0) c.vx = (int8_t)-c.vx; } else continue;
            c.x = (int16_t)(c.x + c.vx);
            if (c.y >= groundAt(c.x) || c.kind == 4) { if (c.kind == 3 || rnd(80) == 0) sfx(S_MOO); c.kind = 4; c.y = (int16_t)groundAt(c.x); }
            continue;
        }
        if (c.kind == 1) { if (c.t & 1) c.x = (int16_t)(c.x + c.vx); if ((c.t & 7) == 0) c.y = (int16_t)(c.y + (isin(c.t * 4) > 0 ? 1 : -1)); }
        else { if ((c.t & 3) == 0) c.x = (int16_t)(c.x + c.vx); c.y = (int16_t)groundAt(c.x); if (rnd(300) == 0) c.vx = (int8_t)-c.vx; }
        if (absi(c.x - cam - SW / 2) > 170 + (SW - 128) / 2) c.kind = 0;
    }
    if (!hov && hoverT < 255) hoverT = 0;
    hov = false;
    if (ropeL && (frameNo & 1)) ropeL--;                             // the rope wound up slowly
}
static void drawCritters() {
    for (const auto &c : crit) if (c.kind == 4 && hoverT && hoverT < 255 && absi(c.x - LX()) < 16) R(c.x - cam - 7, c.y - 12, hoverT / 4, 2, C_GOLD);   // hovering: the bar fills
    if (ropeL && lst != 2) R(LX() - cam, LY() + 8, 1, ropeL, C_SILVER);   // the empty rope going up
    for (const auto &c : crit) {
        if (!c.kind) continue;
        if (c.kind >= 3) {                                              // the cow
            if (c.kind == 6 || c.kind == 7) { R(c.x - cam, LY() + 8, 1, c.y - LY() - 16, C_SILVER); R(c.x - cam - 4, c.y - 7, 9, 1, C_GOLD); }   // the rope and the harness
            if (c.kind == 8) {
                const int d = sgn(c.x - doorX), at = c.t / 2 >= absi(c.x - doorX);
                if (at && patT) { blitBC(C_MAN_STAND, c.x - d * 12 - cam, groundAt(c.x - d * 12), d < 0); R(c.x + (d > 0 ? -10 : 7) - cam, c.y - 7 - ((patT & 8) ? 3 : 0), 4, 2, C_WHITE); }   // at its head: a hand patting it
                else { const int mx = at ? c.x + d * 9 : doorX + d * (c.t / 2); blitBC((frameNo & 4) ? C_MAN_RUN1 : C_MAN_RUN2, mx - cam, groundAt(mx), d > 0); }
            }   // the crew
            if (c.kind == 5) { const Enemy &e = en[c.vx]; drawBeam((e.x >> 4) - cam, e.y >> 4, c.y + 1); }   // beamed up, legs kicking
            const bool fall = c.kind != 4 && c.kind != 8, eat = c.kind == 4 && (c.t & 127) >= 40;
            blitBC(fall ? (((c.t >> 2) & 1) ? C_COWUP : C_COW) : eat ? C_COWEAT : C_COW, c.x - cam, c.y, fall ? ((c.t >> 3) & 1) : c.vx < 0);
            continue;
        }
        const int fl = c.kind == 1, kind = (fl ? PLR.fly : PLR.crawl) - 1, f = (c.t >> (fl ? 2 : 3)) & 1;
        const int hop = (!fl && kind == 0 && (c.t & 31) < 8) ? ((c.t & 31) * (8 - (c.t & 31))) / 4 : 0;
        blitBC(PK.crit[!fl][f], c.x - cam, c.y - hop, c.vx < 0);
    }
}

// adverts for each world's goods: neon boards on the hills
static void adBoard(int x, int y, const char *t, uint32_t h) {
    const int w = text35Width(t) + 6, ht = 11;
    if (x > SW || x + w < 0) return;
    R(x, y, w, ht, C_INK);
    for (int k = 0; k < w; k += 2) { const uint8_t c = ((k / 2 + (frameNo >> 2)) & 1) ? C_GOLD : C_RED; pset(x + k, y, c); pset(x + w - 1 - k, y + ht - 1, c); }
    for (int k = 2; k < ht - 1; k += 2) { const uint8_t c = ((k / 2 + (frameNo >> 2)) & 1) ? C_GOLD : C_RED; pset(x, y + k, c); pset(x + w - 1, y + ht - 1 - k, c); }
    const int ph = (frameNo + (int)(h & 255)) & 63;
    const bool on = (h & 4) ? ph < 44 || (ph & 4) : true;             // some boards blink off and on, the rest pulse
    if (on && (hash((uint32_t)(frameNo >> 1) + h) & 63) != 0) text35(x + 3, y + 3, t, (ph & 32) ? C_ACC : (h & 8) ? C_GOLD : C_WHITE);   // neon, flickering now and then
}

#ifdef CHGBA
void gba_beam(int ux, int uy, int gy, int frameNo);                    // gba/src/gfx_gba.cpp: the same beam, see-through
#endif
static void drawBeam(int ux, int uy, int gy) {                       // the tractor beam: a shimmering green cone
#ifdef CHGBA
    gba_beam(ux, uy, gy, frameNo);
    return;
#endif
    const int top = uy + 5, len = gy - top;
    if (len <= 0) return;
    for (int y = top; y < gy; y++) {
        const int hw = 2 + (y - top) * 6 / len;
        for (int dx = -hw; dx <= hw; dx++) {
            const int x = ux + dx;
            const int k = (y * 2 - frameNo * 3 + absi(dx)) & 7;
            if (absi(dx) == hw) { if ((y + frameNo) & 1) pset(x, y, C_GREEN); }
            else if (k == 0) pset(x, y, C_WHITE);
            else if (k < 3 && ((x + y + frameNo) & 1)) pset(x, y, C_GREEN);
        }
    }
}

// suits: the suit, its shade and the visor; a colonist's look picks one, and a kit (an aerial, a pack, both or none)
static const uint8_t SUIT[6][3] = { { C_WHITE, C_SILVER, C_GOLD }, { C_GOLD, C_RED, C_ACC }, { C_WHITE, C_GREEN, C_GOLD },
                                    { C_WHITE, C_ACC, C_GOLD }, { C_WHITE, C_GOLD, C_ACC }, { C_SILVER, C_RED, C_GOLD } };   // highlight, suit, visor
static NOINL const uint8_t *manMap(uint8_t look) {
    static uint8_t m[14];
    const uint8_t *su = SUIT[look % 6];
    remap(m, RM(1, su[0]) | RM(5, su[1]) << 8 | RM(8, su[2]) << 16);
    return m;
}
static void manKit(const Man &m, int sx, int by, bool jet) {
    const int kit = (m.look >> 3) & 3, back = sx - m.face * 4;
    if (kit & 1) { pset(sx + m.face, by - 12, C_STEEL); pset(sx + m.face, by - 13, (frameNo & 16) ? C_RED : C_STEEL); }
    if ((kit & 2) || jet) { R(back - (m.face < 0 ? 0 : 1), by - 8, 2, 4, C_STEEL); pset(back - (m.face < 0 ? 0 : 1), by - 8, C_SILVER); }
    if (jet) {                                                          // the burners
        const int f = 1 + ((frameNo + sx) & 3) / 2 + (groundAt(m.x) - by < 24 ? 2 : 0);
        for (int k = 0; k < f; k++) { pset(back, by - 4 + k, k ? C_RED : C_GOLD); if (k == 0) pset(back - 1 + (m.face > 0), by - 4, C_WHITE); }
    }
}

static void drawMen() {
    for (int i = 0; i < nMen; i++) {
        const Man &m = men[i];
        const int sx = m.x - cam;
        if (sx < -10 || sx > SW + 10) continue;
        const bool flip = m.face < 0;
        const int phase = (frameNo + i * 7);
        const uint8_t *mp = manMap(m.look);
        switch (m.st) {
        case M_IDLE: {
            const CSpr *s = &C_MAN_STAND;
            int by = m.y;
            switch (m.act) {
            case D_GAZE: s = &C_MAN_LOOKUP; break;
            case D_TRIP: s = SPIN[2]; break;                                // flat out
            case D_WANDER: case D_TREK: s = (phase & 8) ? &C_MAN_RUN1 : &C_MAN_STAND; break;
            case D_WAVE: s = (phase & 4) ? &C_MAN_WAVE1 : &C_MAN_WAVE2; break;
            case D_CHEER: if (phase & 8) { s = &C_MAN_CHEER; by--; } break;
            case D_CHAT: {
                const bool speaking = (((m.t >> 5) & 1) == m.by);
                if (speaking) {
                    if (phase & 8) s = &C_MAN_TALK;
                    const int bx = sx + m.face * 4 - 3, byy = m.y - 18;          // a speech bubble
                    R(bx, byy, 8, 5, C_WHITE); pset(bx + (m.face > 0 ? 1 : 6), byy + 5, C_WHITE);
                    for (int d = 0; d < 3; d++) if (((m.t >> 2) % 4) > d) pset(bx + 1 + d * 2, byy + 2, C_INK);
                }
                break;
            }
            default: break;
            }
            blitBC(*s, sx, by, flip, 255, mp);
            manKit(m, sx, by, false);
            break;
        }
        case M_RUN: case M_WALK: blitBC((phase & 4) ? C_MAN_RUN1 : C_MAN_RUN2, sx, m.y, flip, 255, mp); manKit(m, sx, m.y, false); break;
        case M_BEAM: blitBC(*SPIN[(frameNo >> 2) & 7], sx, m.y + 1, ((frameNo >> 4) & 1) != 0, 255, mp); break;   // tumbling and twisting
        case M_DROP: blitBC(*SPIN[(frameNo >> 1) & 7], sx, (m.y >> 2) + 1, ((frameNo >> 3) & 1) != 0, 255, mp); break;
        case M_JET: { const int y = m.y >> 2; blitBC((frameNo & 8) ? C_MAN_CHEER : C_MAN_WAVE1, sx, y, flip, 255, mp); manKit(m, sx, y, true); break; }
        default: break;
        }
    }
}

static void drawEnemies() {
    for (const auto &e : en) {
        if (!e.kind) continue;
        const int sx = (e.x >> 4) - cam, sy = e.y >> 4;
        if (sx < -16 || sx > SW + 16) continue;
        const uint8_t solid = e.hit ? C_WHITE : 255;
        switch (e.kind) {
        case E_UFO:
            if (e.st == 1 && e.target != 255) drawBeam(sx, sy, men[e.target].y + 1);
            if (e.st == 4) {                                            // the order: a paper bag rising in the beam
                const int gy = groundAt(burgerX + 35) - 8, by = gy - e.cargo / 2;
                drawBeam(sx, sy, gy + 2);
                drawBag(sx, by);
            }
            {
                const uint8_t *mp = EMAP; uint8_t rm[14];
                if (angryT && angryE == &e - en && ((angryT > 90 || angryT < 20) ? (frameNo & 4) : 1)) { memcpy(rm, EMAP, 14); rm[3] = C_RED; mp = rm; }   // the dome: flashes red, holds, flashes back
                const int tl = mp == EMAP ? 0 : isin(tiltPh);
                if (tl > 50 || tl < -50) blit(tl > 0 ? C_SAUCER_TR : C_SAUCER_TL, sx - 11, sy - 11, false, solid, mp);   // rocking side to side
                else blit(*SAUCER[(frameNo >> 2) & 3], sx - 11, sy - 5, false, solid, mp);
            }
            for (int k = 0; k < e.cargo && e.st < 3; k++) pset(sx - 4 + k * 2, sy - 3, (frameNo + k * 4) & 8 ? C_WHITE : C_GOLD);
            break;
        case E_STINGER: blit((frameNo & 8) ? C_SNATCH0 : C_SNATCH1, sx - 7, sy - 6, sx > LX() - cam, solid, EMAP); break;
        case E_MINE: blit((frameNo & 4) ? C_MINE0 : C_MINE1, sx - 4, sy - 4, false, solid, EMAP); break;
        case E_WORM: {
            if (e.st >= 4) {                                            // hanging by its head: the rope, the maw, the body swinging below
                const int cy = groundAt(alienX + 20) - COLL_TOP[PLR.coll], hy = e.st == 6 ? cy - 30 + e.t * 3 / 10 : (e.y >> 4);
                if (e.st == 4) rope(sx, hy - 5);
                for (int y = 1; y <= 30; y += 3) {
                    const int x0 = sx - 6 + isin(frameNo * (e.st == 4 && e.cargo < 34 ? 5 : 3) + y * 7) * (y + 6) / (e.st == 4 && e.cargo < 34 ? 400 : 900);   // curving, the tail swinging widest (wildly while it fights)
                    if (e.st != 4 ? hy + y + 3 > cy : hy + y + 3 > groundAt(e.x >> 4)) break;   // (still in the sand, or into the collector)
                    R(x0, hy + y, 13, 3, C_STEEL); R(x0, hy + y, 3, 3, C_SILVER); hl(x0, hy + y, 13, C_INK);
                }
                if (e.st == 6) for (int k = 0; k < 6; k++) R(sx + (int)(hash(k * 97u + e.t / 4) % 61) - 30, cy - 4 - (int)(hash(k * 31u + e.t / 4) % 40), 2 + (k & 1), 2, C_RED);   // blood spattering round the collector
                if (hy < cy) {                                          // the head: a maw, pink inside, teeth top and bottom, opening wide to spit
                    const int op = e.st == 4 && spitT ? (spitT > 10 ? 24 - spitT : spitT) / 2 : 1, top = hy - 5 - op;
                    R(sx - 7, top, 15, 5 + op, C_INK);
                    for (int y = top + 1; y < hy - 1; y++) for (int x = sx - 5; x <= sx + 5; x++) pset(x, y, ((x + y) & 1) ? C_RED : C_WHITE);
                    for (int k = -6; k < 8; k += 2) { pset(sx + k, top, C_WHITE); pset(sx + k, top + 1, C_WHITE); pset(sx + k - 1, hy - 1, C_WHITE); }
                }
                break;
            }
            const int gy = groundAt(e.x >> 4);
            if (hoverT && hoverT < 255 && absi((e.x >> 4) - LX()) < (e.st ? 24 : 16)) { R(sx - 7, gy - (e.st ? 38 : 0) - 12, hoverT / 4, 2, C_RED); const int hy0 = gy - (e.st ? 42 : 4), k = hoverT < 27 ? hoverT : 27; rope(LX() - cam + (sx - LX() + cam) * k / 27, LY() + 9 + (hy0 - LY() - 9) * k / 27); }   // hovering: the bar fills (red: a worm) as the rope lowers
#ifdef SL_TRACE
            if ((frameNo & 15) == 0) fprintf(stderr, "wormdraw f%d sx%d gy%d st%d\n", frameNo, sx, gy, e.st);
#endif
            if (e.st == 0) {                                            // the sand heaves and ripples above it
                for (int k = -9; k <= 9; k++) { const int h = (11 - absi(k)) / 2 + (isin(frameNo * 16 + k * 40) > 60); if (h > 0) { R(sx + k, gy - h, 1, h, C_GNDL); pset(sx + k, gy - h, C_GNDD); } }   // a dark crest you can follow
                if ((frameNo & 3) == 0) part((e.x >> 4) + rnd(9) - 4, gy - 2, rnd(10) - 5, -6 - rnd(8), 12, C_GNDL);
            } else {                                                    // risen: a ringed body and a mouth of teeth
                const int h = e.st == 1 ? e.t * 7 / 4 : e.st == 2 ? 34 : 34 - e.t * 7 / 4;
                int ox = 0;
                for (int y = 0; y < h; y += 3) {                         // steel-grey rings, swaying, lit on the left
                    ox = isin(frameNo * 2 + y * 6) * (y + 4) / 900;                // a gently curving body, swaying
                    const int x0 = sx - 7 + ox;
                    R(x0, gy - y - 3, 15, 3, solid != 255 ? C_WHITE : C_STEEL); R(x0, gy - y - 3, 3, 3, C_SILVER); hl(x0, gy - y - 3, 15, C_INK);
                }
                const int ty = gy - h, mx = sx - 8 + ox;
                R(mx, ty - 4, 17, 5, C_INK);                              // the maw: a red-gold throat, teeth above and below
                R(mx + 3, ty - 3, 11, 3, (frameNo & 4) ? C_RED : C_GOLD);
                for (int k = 1; k < 17; k += 2) pset(mx + k, ty - 4, C_WHITE);
                if (frameNo & 1) part((e.x >> 4) + rnd(17) - 8, gy - 1, rnd(16) - 8, -8 - rnd(10), 14, C_GNDL);
            }
            break;
        }
        case E_MOTHER:
            blit(PK.mother, sx - 22, sy - 10, false, solid, EMAP);
            if (frameNo & 8) pset(sx - 16 + ((frameNo >> 2) % 9) * 4, sy, C_RED);       // a light running round its rim
            R(sx - 12, sy - 13, e.hp * 24 / clampi(40 + stage * 2, 40, 120), 1, C_RED);                 // how much it can still take
            break;
        case E_TURRET: {
            const int dx = clampi((LX() - (e.x >> 4)) / 8, -4, 4);
            for (int k = 0; k < 5; k++) pset(sx + dx * k / 4, sy - 4 - k, C_STEEL);
            blit(C_TURRET, sx - 6, sy - 4, false, solid, EMAP);
            break;
        }
        }
    }
}

static void drawLander() {
    if (lst == 2) return;
    if (invT && (frameNo & 2) && lst != 1) return;
    const int sx = LX() - cam, sy = LY();

    if (thrusting) {                                                // the flame, along the hull's axis
        int nx, ny;
        hullPt(0, 9, nx, ny);
        const int len = 3 + rnd(4) + (seats < 4 ? 1 : 0), dxs = -sinA(ang), dys = cosA(ang);
        for (int k = 0; k <= len; k++) {
            const int px = nx - cam + ((dxs * k) >> 8), py = ny + ((dys * k) >> 8);
            const uint8_t c = k < 1 ? C_WHITE : k < 3 ? C_GOLD : C_RED;
            pset(px, py, c);
            if (k < len - 1) { pset(px + (dys >> 8), py - (dxs >> 8), k < 2 ? C_GOLD : C_RED); pset(px - (dys >> 8), py + (dxs >> 8), k < 2 ? C_GOLD : C_RED); }
        }
    }
    if (thrusting) {                                                // hovering low: dust rolls out along the ground both ways
        const int gx = LX(), alt = groundAt(gx) - sy;
        if (alt < 36) for (int k = 0; k < 4; k++) {
            const int rr = ((frameNo * 3 + k * 9) % 36) * (40 - alt) / 36, y = groundAt(gx + rr) - 1 - (rr > 6 ? (k & 1) : 0);
            const uint8_t c = rr < 12 ? C_GNDL : (k & 1) ? C_GND : C_GNDL;
            for (int d = -1; d <= 1; d += 2) { const int x = sx + d * rr; pset(x, y, c); if (rr > 4) { pset(x - d, y - 1, c); if ((x + frameNo) & 1) pset(x + d, y - 2, C_GND); } }
        }
    }
    if (sy < TOP - 8 && (frameNo & 4)) { const int ax = clampi(sx, 3, SW - 4); pset(ax, TOP + 1, C_GOLD); hl(ax - 1, TOP + 2, 3, C_GOLD); }   // above the screen: an arrow
    const int a = absi(ang);
    if (!gear) blit(*LANDER_UP[a], sx - 11, sy - 11, ang < 0); else if (a < 3) blit(*LANDER[a], sx - 11, sy - 11, ang < 0); else blit(*LANDER_STEEP[a - 3], sx - 11, sy - 11, ang < 0);
    if (lidT) { R(sx - 3, sy - 10, 7, 2, C_INK); R(sx + 4, sy - 17, 2, 8, C_SILVER); }   // the flip-top lid, open
    else if (foodOn && (frameNo & 8)) pset(sx, sy - 10, C_GOLD);      // the takeaway aboard
    if (charge == CHARGED) {                                         // charged: the nose glows
        const int gx = sx + facing * 10;
        gfx_fillCircle(gx, sy, 2 + ((frameNo >> 1) & 1), (frameNo & 4) ? C_ACC : C_GOLD); pset(gx, sy, C_WHITE);
    }
    if (absi(ang) < 3) {                                             // the gun pod
        const int gx = sx + facing * 7, gy = sy + 1;
        pset(gx, gy, C_STEEL); pset(gx + facing, gy, gunCd ? ((frameNo & 1) ? C_WHITE : C_GOLD) : C_SILVER); pset(gx, gy + 1, (frameNo & 2) && gunCd ? C_SILVER : C_STEEL);
    }
}

static NOINL void drawBag(int x, int y) {                                   // a takeaway bag in the world's shop colour
    R(x - 2, y - 4, 5, 5, C_WHITE); R(x - 2, y - 4, 5, 1, C_SILVER); R(x - 2, y - 2, 5, 1, PLR.shopc); pset(x, y - 3, C_GOLD);
}

static void drawDelivery() {
    if (partyT) {                                                       // the feast at the base door, a colonist dancing either side
        const int fx = (doorX + basePadX + 20) / 2, gy = groundAt(fx) - 1, hop = (frameNo >> 3) & 1;   // between the pad and the door
        blitBC(PK.food, fx - cam, gy, false);
        for (int k = -1; k <= 1; k += 2) blitBC(hop ? C_MAN_CHEER : C_MAN_WAVE1, fx - cam + k * 12, gy - hop, k > 0, 255, manMap((uint8_t)(k + 3)));
    }
    if (!dlv) return;
    if (dlv <= 4) drawBag(bagX - cam, bagY);
    if (dlv >= 2) {
        const int x = scX - cam;
        blit(C_SCOOTER, x - 10, scY - 8, scDir < 0);
        pset(x - scDir * 11, scY - 2, (frameNo & 2) ? C_GOLD : C_RED);  // its exhaust
    }
}

static void drawShots() {
    for (const auto &s : shots) {
        if (!s.life) continue;
        const int x = s.x - cam;
        if (s.kind >= 2) {                                                 // a plasma ball: a flickering halo round a white-hot core
            gfx_fillCircle(x, s.y, 4 + (frameNo & 1), (frameNo & 2) ? C_ACC : C_GOLD); gfx_fillCircle(x, s.y, 2, C_WHITE);
            if (frameNo & 1) part(s.x - s.dir * 4 + rnd(5) - 2, s.y - (s.dir ? 0 : 4), -s.dir * 6, s.dir ? 0 : -6, 6, C_GOLD);
            continue;
        }
        if (s.kind) { pset(x, s.y, C_WHITE); pset(x - s.dir, s.y, C_GOLD); pset(x - s.dir * 2, s.y, C_GOLD); pset(x - s.dir * 4, s.y, C_RED); continue; }
        for (int k = 0; k < 14; k++) pset(x - s.dir * k, s.y, k < 2 ? C_WHITE : k < 6 ? C_ACC : (k & 1) ? C_WHITE : C_SKY0);
    }
    for (const auto &s : es) {
        if (!s.life) continue;
        const int x = (s.x >> 4) - cam, y = s.y >> 4;
        const uint8_t c = s.kind ? ((frameNo & 2) ? C_RED : C_GOLD) : ((frameNo & 2) ? C_GREEN : C_WHITE);
        pset(x, y, C_WHITE); pset(x - 1, y, c); pset(x + 1, y, c); pset(x, y - 1, c); pset(x, y + 1, c);
    }
    for (const auto &q : pt) if (q.life) pset((q.x >> 4) - cam, q.y >> 4, q.col);
    for (const auto &b : booms) if (b.t && !b.delay) {
        const int age = 16 - b.t, x = b.x - cam, r = b.r;
        if (age < 2) gfx_fillCircle(x, b.y, 2 + r * (age + 1) / 4, C_WHITE);
        else if (age < 7) {
            const int rr = r * (age + 2) / 9;
            gfx_fillCircle(x, b.y, rr, C_RED); gfx_fillCircle(x - 1, b.y - 1, rr * 2 / 3, C_GOLD); gfx_fillCircle(x - 1, b.y - 1, rr / 3, C_WHITE);
        } else if (age < 12) {
            const int rr = r * (12 - age) / 6;
            if (rr > 0) { gfx_fillCircle(x, b.y - (age - 7), rr, C_RED); if (rr > 2) gfx_fillCircle(x, b.y - (age - 7), rr / 2, C_GOLD); }
        } else {
            const int rr = r / 2 + (age - 10) * 2;
            for (int k = 0; k < 32; k += 2) pset(x + isin(k * 8 + 64) * rr / 128, b.y - (age - 10) + isin(k * 8) * rr / 160, (k & 2) ? C_STEEL : C_SILVER);
        }
    }
}

static void drawHud() {
    R(0, 0, SW, TOP, C_INK);
#ifdef CHGBA
    {   // StarLift Advance: the top row spread across 240: fuel, colonists, seats, shield, stage, score
        text35(2, 1, "FUEL", C_SILVER);
        R(20, 1, 64, 6, C_STEEL);
        const int fw = fuel * 62 / FUEL_MAX;
        R(21, 2, fw, 4, fuel < FUEL_MAX / 3 ? ((frameNo & 8) ? C_RED : C_GOLD) : C_GOLD);
        char q[8];
        text35(92, 1, fmt(q, "", nSaved, "/", quota), nSaved >= quota ? C_GREEN : C_GOLD);
        for (int i = 0; i < SEATS; i++) {                               // a seat each: the figure lit when taken
            const uint8_t c = i < seats ? C_WHITE : C_STEEL;
            R(116 + i * 6, 1, 4, 6, c); pset(117 + i * 6, 2, i < seats ? C_GOLD : C_INK); pset(118 + i * 6, 2, i < seats ? C_GOLD : C_INK);
        }
        for (int i = 0; i < shMax(); i++) R(160 + i * 4, 1, 3, 6, i < shield ? C_GREEN : C_STEEL);
        char s[12];
        text35(197, 1, fmt(s, "STG ", stage + 1), C_SILVER);
        fmt(s, "", score);
        text35(SW - 1 - text35Width(s), 1, s, C_WHITE);
    }
#else
    // fuel
    text35(1, 0, "F", C_SILVER);
    R(7, 2, 26, 4, C_STEEL);
    const int fw = fuel * 24 / FUEL_MAX;
    R(8, 3, fw, 2, fuel < FUEL_MAX / 3 ? ((frameNo & 8) ? C_RED : C_GOLD) : C_GOLD);
    // the seats: a white figure for each colonist aboard
    char q[8];
    text35(35, 1, fmt(q, "", nSaved, "/", quota), nSaved >= quota ? C_GREEN : C_GOLD);
    for (int i = 0; i < SEATS; i++) { const uint8_t c = i < seats ? C_WHITE : C_STEEL; R(57 + i * 4, 2, 3, 4, c); pset(58 + i * 4, 3, i < seats ? C_GOLD : C_INK); }
    // shield and lives
    for (int i = 0; i < shMax(); i++) R(80 + i * 3, 2, 2, 4, i < shield ? C_GREEN : C_STEEL);
    char s[12];
    fmt(s, "", score);
    text35(SW - 1 - text35Width(s), 1, s, C_WHITE);
#endif
    // the scanner: the whole stage across the screen, as tall as the HUD (RH rows from row 9)
    enum { RH = TOP - 10, RW = SW - 8 };
    auto rx = [](int wx) { return 4 + wx * RW / W; };
    auto ry = [](int y) { return 9 + (y > TOP ? (y - TOP) * (RH - 1) / (SH - TOP) : 0); };
    R(3, 8, RW + 2, 1, C_STEEL);
#ifdef CHGBA
    {   // GBA: the ground line's rows kept from stage to stage (no divide on the ARM7: 3 a pixel cost ~3 ms), drawn straight into the frame
        static uint8_t line[RW], gen;
        if (gen != hgtGen) {
            gen = hgtGen;
            for (int i = 0; i < RW; i++) { const int a = i * W / RW, b = a + W / RW / 2, m = hgt[a / 4] < hgt[b / 4] ? hgt[a / 4] : hgt[b / 4]; line[i] = (uint8_t)ry(m); }
        }
        for (int i = 0; i < RW; i++) {
            const int x = 4 + i;
            uint8_t *q = gfx_fb + line[i] * GFX_FB_STRIDE + (x >> 1);
            *q = (x & 1) ? (uint8_t)((*q & 0x0F) | (C_GNDD << 4)) : (uint8_t)((*q & 0xF0) | C_GNDD);
        }
    }
#else
    for (int i = 0; i < RW; i++) { const int a = i * W / RW, b = a + W / RW / 2, m = hgt[a / 4] < hgt[b / 4] ? hgt[a / 4] : hgt[b / 4]; pset(4 + i, ry(m), C_GNDD); }   // the ground line
#endif
    const int c0 = rx(cam);
    R(c0, 9, 1, RH, C_STEEL); R(rx(cam + SW), 9, 1, RH, C_STEEL);
    R(rx(basePadX + 20) - 1, 9 + RH - 3, 3, 2, C_ACC);
    for (int i = 0; i < nMen; i++) {
        const Man &m = men[i];
        if (m.st == M_IDLE || m.st == M_RUN || m.st == M_WALK || m.st == M_JET || m.st == M_BEAM) R(rx(m.x), ry(m.st == M_JET ? m.y >> 2 : m.y) - 2, 1, 2, m.st == M_BEAM ? C_GREEN : C_WHITE);   // where they are, just above the ground line
    }
    for (const auto &it : items) if (it.st == 1 && (frameNo & 8)) pset(rx(it.x), 9 + RH - 2, C_GOLD);
    for (const auto &e : en) if (e.kind) pset(rx(e.x >> 4), ry(e.y >> 4), e.kind == E_UFO ? C_GREEN : C_RED);
    if (frameNo & 4) pset(rx(LX()), ry(LY()), C_GOLD);
}

static void drawWorld() {
    const int saved = cam;
    if (shake) cam += (frameNo & 1) ? (shake >> 2) : -(shake >> 2);
    cam = clampi(cam, 0, W - SW);
    dbg::profStart();
    for (int L = 0; L < 4; L++) layerTops(L);
    dbg::prof(0);
    drawSky();
    dbg::prof(1);
    card::lowPri = true;                                           // the scenery may wait a frame for the card
    drawSkyObjects();
    dbg::prof(2);
#ifndef SL_NOSCENE
#ifdef SL_NOSTRUCT
    for (int L = 0; L < 3; L++) drawLayerCols(L);
#else
    for (int L = 0; L < 3; L++) { drawLayerCols(L); drawStructures(L); }
#endif
    dbg::prof(3);
    drawGroundCols();
#endif
    dbg::prof(4);
    drawGroundThings();
    card::lowPri = false;
    dbg::prof(5);
    { const int c = cam; cam = saved; drawHud(); cam = c; }        // the HUD first: everything up there flies in front of the radar
    drawMen();
    drawEnemies();
    dbg::prof(6);
    drawLander();
    drawDelivery();
#ifdef SL_BEAMTEST
    drawBeam(SW / 2, 40 + WDY, 140 + WDY);                             // a test: a beam on screen every frame
#endif
    drawShots();
#ifndef SL_NOWX
    drawWeather();
#endif
    if (cutAt() > -999) { R(0, TOP, SW, 7, C_INK); R(0, SH - 7, SW, 7, C_INK); }   // a cut-scene's bars
#ifdef SL_TRACE
    if ((frameNo & 15) == 0) fprintf(stderr, "cut f%d dlv%d party%d at%d\n", frameNo, dlv, partyT, cutAt());
#endif
    dbg::prof(7);
    for (auto &q : pops) if (q.t) { char s[8]; fmt(s, "", q.v); text35(q.x - cam - 6, q.y - (30 - q.t) / 3, s, (q.t & 2) ? C_WHITE : C_GOLD); }
    cam = saved;
    dbg::prof(8);
    drawGore();
    drawPointer();
    if (bannerT) {
        char *b = bannerTxt;
        if (bannerNew) { fmt(b, T(banner), bannerNum); bannerNew = 0; }
        int w = text35Width(b);
        if (w * 2 <= SW - 8) { w *= 2; R((SW - w) / 2 - 3, TOP + 3, w + 6, 13, C_INK); text35x2((SW - w) / 2, TOP + 5, b, (frameNo & 4) ? C_WHITE : C_GOLD); }   // a call-out: big
        else { R((SW - w) / 2 - 3, TOP + 3, w + 6, 9, C_INK); textC(TOP + 4 - UY, b, (frameNo & 4) ? C_WHITE : C_GOLD); }
    }
}

// ---- title, menus ------------------------------------------------------------------------------------------------------
static void drawLogo(int y) { blit(C_LOGO_STARLIFT, 7 + UX, y + UY, false); }   // 114 x 29

static void drawBeam(int ux, int uy, int gy);
static uint8_t menuItems(uint8_t *it);
static void drawTitleScene() {
    for (int L = 0; L < 4; L++) layerTops(L);
    drawSky(); drawSkyObjects();
#ifdef SL_NOSTRUCT
    for (int L = 0; L < 3; L++) drawLayerCols(L);
#else
    for (int L = 0; L < 3; L++) { drawLayerCols(L); drawStructures(L); }
#endif
    drawGroundCols();
    drawPlants();
    drawWeather();
    R(0, 0, SW, TOP, C_SKY0);
}

static void drawTitle() {
    drawTitleScene();

#ifndef SL_SLIM
    drawLogo(10);
    textCS(42, T(MS_RESCUE_THE_COLONISTS), C_WHITE);
    char s[24];
    if (hiScore) { fmt(s, T(MS_HI), hiScore); textCS(2, s, C_SILVER); }
    uint8_t it[6];
    const uint8_t n = menuItems(it);
    panel(74, n * 9 + 5);
    menuList(it, n, titleSel, 32, 77);
#ifdef GBA_VERSION
    text35(2, SH - 6, "V" SL_VERSION " ADVANCE " GBA_VERSION, C_SILVER);
#else
    text35(2, SH - 6, "V" SL_VERSION, C_SILVER);
#endif
#endif
}

static NOINL void menuList(const uint8_t *it, int n, int sel, int x, int y) {
    for (int i = 0; i < n; i++) {
        char b[32];
        if (it[i] == MS_STAGE_N) fmt(b, T(MS_STAGE_N), selStage + 1);
        else if (it[i] == MS_DIFF) fmt(b, T(MS_DIFF), -1, T((uint8_t)(MS_EASY + diff))); else fmt(b, T(it[i]));
        text35(x + UX, y + UY + i * 9, b, sel == i ? C_GOLD : C_WHITE);
    }
    text35(x + UX - 7, y + UY + sel * 9, ">", (frameNo & 8) ? C_GOLD : C_RED);
}

static uint8_t menuItems(uint8_t *it) {
    uint8_t n = 0;
    if (hasSave) it[n++] = MS_CONTINUE;
    it[n++] = hasSave ? MS_NEW_GAME : MS_START;
    it[n++] = MS_DIFF;
#ifdef SL_STAGESEL
    it[n++] = MS_STAGE_N;                                            // (testing: a stage select; local builds only)
#endif
    it[n++] = MS_HOW_TO_PLAY;
    it[n++] = MS_OPTIONS;
    return n;
}

// how to play: three pages over the scene
static NOINL void panel(int y, int h) { y += UY; R(4 + UX, y, 120, h, C_INK); R(4 + UX, y, 120, 1, C_GOLD); R(4 + UX, y + h - 1, 120, 1, C_GOLD); }
static void drawHelp() {
#ifndef SL_SLIM                                                    // a board perf build leaves this out so the debug image fits
    drawTitleScene();
    panel(6, 116);
    textCS(10, T((uint8_t)(MS_CONTROLS + helpPage)), C_GOLD);
    for (int i = 0; i < HELP_LINES; i++) {                          // the text, from the card
        const uint8_t *o = card::at(CARD_TXT + i * 2u);
        const uint8_t *r = o ? card::at(CARD_TXT + (uint32_t)(o[0] | o[1] << 8)) : nullptr;
        if (r && r[0] == helpPage) text35(r[1] + UX, r[2] + UY, (const char *)r + 4, r[3]);
    }
    if (frameNo & 16) text35(55 + UX, 114 + UY, T(MS_A_NEXT_B_BACK), C_SILVER);
#endif
}

static void drawOptions() {
#ifndef SL_SLIM                                                    // a board perf build leaves this out so the debug image fits
    drawTitleScene();
    panel(30, 66);
    textCS(34, T(MS_OPTIONS), C_GOLD);
    for (int i = 0; i < 3; i++) {
        text35(22 + UX, 48 + UY + i * 12, T((uint8_t)(MS_MUSIC + i)), optSel == i ? C_GOLD : C_WHITE);
        if (i < 2) for (int k = 0; k < 4; k++) R(76 + UX + k * 8, 49 + UY + i * 12, 6, 6, k < (i ? fxV : musicV) ? (optSel == i ? C_GOLD : C_SILVER) : C_STEEL);
    }
    text35(14 + UX, 48 + UY + optSel * 12, T(MS_X52), C_GOLD);
    text35(10 + UX, 88 + UY, T(MS_LEFT_RIGHT_CHANGE_A_OR_B_BACK), C_SILVER);
#endif
}

static void drawIntro() {
    drawWorld();
#ifndef SL_SLIM
    panel(30, 68);
    char s[28];
    if (timer <= 100) {                                                    // who's who
        panel(18, 106);
        textCS(21, T(MS_WHO), C_GOLD);
        bool has[7] = { true, true, true, true, false, false, false };    // UFO, stinger, mine come anywhere
        for (const auto &e : en) if (e.kind) has[e.kind] = true;
        int y = 32 + UY;
        auto row = [&](uint8_t msg, uint8_t col) { text35(32 + UX, y + 3, T(msg), col); y += 13; };
        blitBC(C_MAN_WAVE1, 17 + UX, y + 11, false); row(MS_R_COLONIST, C_GREEN);
        blit(*SAUCER[(frameNo >> 2) & 3], 6 + UX, y + 2, false, 255, EMAP); row(MS_R_UFO, C_RED);
        blit(C_SNATCH0, 10 + UX, y, false); row(MS_R_STINGER, C_RED);
        blit(C_MINE0, 13 + UX, y + 1, false); row(MS_R_MINE, C_GOLD);
        if (has[E_TURRET]) { blit(C_TURRET, 11 + UX, y + 3, false); row(MS_R_TURRET, C_RED); }
        if (has[E_WORM]) { R(11 + UX, y + 4, 13, 6, C_GND); R(13 + UX, y + 2, 9, 3, C_RED); row(MS_R_WORM, C_RED); }
        return;
    }
    fmt(s, T(MS_STAGE_N), stage + 1);
    textC(34, s, C_SILVER);
    textC(44, PLR.name, C_GOLD);
    textC(53, PLR.place, C_WHITE);
    fmt(s, T(MS_SAVE), quota, T(MS_OF), nMen);
    textC(64, s, C_GREEN);
    fmt(s, T(MS_GRAVITY_0), gHund(), T(MS_G));
    textC(74, s, C_SILVER);
    textC(85, T(baseRight ? MS_BASE_IS_EAST : MS_BASE_IS_WEST), C_ACC);
#endif
}

static void drawPause() {
#ifndef SL_SLIM                                                    // a board perf build leaves this out so the debug image fits
    drawWorld();
    panel(40, 54);
    textC(44, T(MS_PAUSED), C_GOLD);
    static const uint8_t PM[4] = { MS_RESUME, MS_DIFF, MS_SAVE_GAME, MS_QUIT };
    menuList(PM, 4, pauseSel, 30, 54);
    if (pauseNote) textCS(96, T(pauseNote), C_GOLD);
#endif
}

static void draw() {
#ifdef CHGBA
    {                                                                  // a fade in from black when a screen starts
        static uint8_t lastState = 255, fadeT;
        if (state != lastState) { if (state == TITLE || state == INTRO || state == OVER) fadeT = 8; lastState = (uint8_t)state; }
        if (fadeT) { fadeT--; gfx_setFade((uint8_t)(fadeT * 30), 0x0000); }
    }
#endif
    if (!card::ok() || loadReq) {                                      // STARLIFT.DAT holds the worlds, the music and the big pictures
        gfx_clear(C_INK);
        textC(60, "NEED STARLIFT.DAT", C_WHITE);
        return;
    }
    switch (state) {
    case TITLE: drawTitle(); break;
    case INTRO: drawIntro(); break;
    case PAUSED: drawPause(); break;
    case CLEAR: {
        drawWorld();
#ifndef SL_SLIM
        panel(30, 66);
        textC(34, T(missed ? MS_QUOTA_MISSED : MS_STAGE_CLEAR), missed ? C_RED : C_GOLD);
        char s[28];
        if (missed) textC(86, T(MS_TRY_AGAIN), C_SILVER);
        else for (int k = 0; k < 3; k++) {                              // the stars: quota, everyone home, no lander lost
            const int x = 52 + k * 10;
            const uint8_t c = k < stars ? C_GOLD : C_STEEL;
            R(x + 2 + UX, 85 + UY, 1, 7, c); R(x - 1 + UX, 88 + UY, 7, 1, c); R(x + 1 + UX, 86 + UY, 3, 5, c);
        }
        fmt(s, T(MS_RESCUED), nSaved, T(MS_OF), nMen);
        textC(44, s, C_WHITE);
        fmt(s, T(MS_FUEL_BONUS), fuel / 4);
        textC(54, s, C_SILVER);
        textC(64, fmt(s, T(MS_TALLY), feasts, T(MS_COWS), cowsN), C_GOLD);
        if (nSaved == nMen) textC(76, T(MS_ALL_HOME_2000), C_GREEN);
#endif
        break;
    }
#ifndef SL_SLIM
    case OVER: {
        drawWorld();
        for (int y = TOP; y < SH; y += 2) memset(gfx_fb + y * GFX_FB_STRIDE, C_INK << 4 | C_INK, GFX_FB_STRIDE);   // the scene dimmed by scan lines
        blit(C_LOGO_GAMEOVER, 10 + UX, 22 + UY + (overT < 30 ? (30 - overT) : 0), false);
        if (overT > 30) {
            panel(46, 58);
            char s[28];
            textC(50, fmt(s, T(MS_SCORE), score), C_WHITE);
            textC(60, fmt(s, T(MS_BEST), hiScore), C_GOLD);
            textC(70, fmt(s, T(MS_COLONISTS_HOME), gSaved), C_GREEN);
            if (newHi && (frameNo & 8)) textCS(91, T(MS_NEW_HIGH_SCORE), C_GOLD);
            if (overT > 90 && (frameNo & 16)) textCS(110, T(MS_PRESS_A), C_WHITE);
        }
        break;
    }
#endif
    case HELP: drawHelp(); break;
    case OPTIONS: drawOptions(); break;
    default: drawWorld(); break;
    }
}

// ---- update ------------------------------------------------------------------------------------------------------------
static void toTitle() {
    state = TITLE; titleSel = 0; stage = 0; planet = 0;
    loadReq = true;
    cam = 200; synth::music(0); synth::engine(0);
}

static void updateCamera() {
    const int lead = clampi(lvx / 6, -40, 40);
    const int t = clampi(LX() - SW / 2 + lead, 0, W - SW);
    cam += (t - cam) / 6;
    cam = clampi(cam, 0, W - SW);
}

#ifdef SL_BOT
// A test autopilot: to the nearest camp (landing on its free side) or, when full or low on fuel, to the base; cruise above the
// ground ahead, come down slowly, shoot what is in front.
static void botInput(bool &l, bool &r, bool &u, bool &d, bool &a, bool &b) {
    l = r = u = d = a = b = false;
    if (state != PLAY) { a = (frameNo & 15) == 0; if (state == TITLE) titleSel = hasSave ? 1 : 0; return; }
    const int X = LX(), Y = LY();
    const int near = nearestIdle(X, 9999);
    const bool goBase = seats >= SEATS || (seats && near < 0) || fuel < FUEL_MAX / 4 || near < 0;
    int tx = basePadX + 20;
    if (!goBase) tx = zoneX[men[near].home] + (baseRight ? 16 : -16);
    if (!gear) d = (frameNo & 2) != 0;
    if (lst == 1) {
        bool wait = onPad == 2 && (seats || fuel < FUEL_MAX - 20);
        for (auto &m : men) if (m.st == M_RUN) wait = true;
        if (!wait && (absi(tx - X) > 6 || nearestIdle(X, 60) < 0 || seats >= SEATS)) a = true;
        return;
    }
#ifdef SL_BUMP
    for (const auto &e : en) if (e.kind == E_UFO) { tx = e.x >> 4; break; }
#endif
    const int dx = tx - X, alt = groundAt(X) - Y;
    int hmin = 127;
    for (int k = -16; k <= 72; k += 8) { const int g = groundAt(X + (dx < 0 ? -k : k)); if (g < hmin) hmin = g; }
    const int cruise = clampi(hmin - 28, CEIL + 4, 118 + WDY);
    const int dvx = absi(dx) < 3 ? 0 : clampi(dx * 3, -300, 300);
    const int dvy = absi(dx) > 14 ? clampi((cruise - Y) * 8, -200, 200) : alt > 40 ? 140 : alt > 18 ? 80 : 40;
    int want = clampi((dvx - lvx) / 48, -3, 3);
    if (alt < 24 || absi(dx) <= 14) want = clampi(want, -1, 1);
    if ((absi(dx) <= 6 && alt < 20) || alt < 14) want = 0;
    r = ang < want; l = ang > want;
    a = lvy > dvy || (ang && absi(dvx - lvx) > 60 && lvy > dvy - 80);
#ifndef SL_BUMP
    for (const auto &e : en) if (e.kind && absi((e.y >> 4) - Y) < 10 && ((e.x >> 4) - X) * facing > 0 && absi((e.x >> 4) - X) < 90) b = true;
#endif
}
#endif

static void update() {
    frameNo++;
#ifdef SL_TRACE
    { static uint8_t ls = 255; if (state != ls) { fprintf(stderr, "state %d\n", (int)state); ls = state; } }
#endif
    bool l = chgame.pressed(LEFT_BUTTON), r = chgame.pressed(RIGHT_BUTTON), u = chgame.pressed(UP_BUTTON), d = chgame.pressed(DOWN_BUTTON);
    bool a = chgame.pressed(A_BUTTON), b = chgame.pressed(B_BUTTON);
#ifdef CHGBA
    const uint8_t sh = gba_shoulders();
    static uint8_t shPrev;
    const bool lEdge = (sh & 1) && !(shPrev & 1);
    shPrev = sh;
    if (sh & 2) u = true;                                           // R: as UP (hold to charge the shot)
#endif
#ifdef SL_BOT
    botInput(l, r, u, d, a, b);
#endif
    static uint8_t prev;
    const uint8_t cur = (u ? 1 : 0) | (d ? 2 : 0) | (a ? 4 : 0) | (b ? 8 : 0) | (l ? 16 : 0) | (r ? 32 : 0), edge = cur & ~prev;
    prev = cur;
    if (shake) shake--;
    if (bannerT) bannerT--;
    if (killT) killT--;
    if (flashT) { flashT--; gfx_setFade((uint8_t)(flashT * 40), 0xFFFF); }
    for (auto &q : pops) if (q.t) q.t--;
    if (lightT) {                                                   // lightning: the sky goes white for a moment
        lightT--;
        if (lightT == 7) { gfx_setPaletteEntry(C_SKY0, gfx_rgb(220, 220, 255)); gfx_setPaletteEntry(C_SKY1, gfx_rgb(255, 255, 255)); }
        if (lightT == 4) applyPalette();
    }
    if (state == TITLE || state == HELP || state == OPTIONS) {        // the menus: the scene scrolls by behind them
        if (++cam > W - 300) cam = 200;
        updateWeather();
    }
    switch (state) {
    case TITLE: {
        if ((frameNo % 240) == 0) {                                 // show the next world
            planet = (planet + 1) & 7; stage = planet; loadReq = true; cam = 200;
        }
        uint8_t it[6];
        const uint8_t n = menuItems(it);
        if (edge & 1) { titleSel = (uint8_t)((titleSel + n - 1) % n); sfx(S_GEAR); }
        if (edge & 2) { titleSel = (uint8_t)((titleSel + 1) % n); sfx(S_GEAR); }
        if (edge & 4) {
            const uint8_t pick = it[titleSel];
            rng ^= (uint32_t)frameNo * 2654435761u; if (!rng) rng = 1;         // the player's timing: this session's own weather and luck
            if (pick == MS_CONTINUE) {
                stage = sv.stage; score = sv.score; lives = sv.lives; nextLife = (score / 10000 + 1) * 10000; gSaved = 0; newHi = false;
                sfx(S_START); newStage();
            } else if (pick == MS_HOW_TO_PLAY) { state = HELP; helpPage = 0; sfx(S_BOARD); }
            else if (pick == MS_OPTIONS) { state = OPTIONS; optSel = 0; sfx(S_BOARD); }
            else if (pick == MS_STAGE_N) { selStage = (uint8_t)((selStage + 1) & 15); sfx(S_GEAR); }
            else if (pick == MS_DIFF) { diff = (uint8_t)((diff + 1) % 3); sfx(S_GEAR); sv.diff = diff; saveFlag = hasSave; saveDue = true; }
            else newGame();
        }
        break;
    }
#ifndef SL_SLIM
    case HELP:
        if (edge & (4 | 32)) { if (++helpPage >= 3) state = TITLE; sfx(S_BOARD); }
        else if (edge & 16) { if (helpPage) helpPage--; sfx(S_BOARD); }
        else if (edge & 8) { state = TITLE; sfx(S_GEAR); }
        break;
    case OPTIONS: {
        if (edge & 1) optSel = (uint8_t)((optSel + 2) % 3);
        if (edge & 2) optSel = (uint8_t)((optSel + 1) % 3);
        uint8_t *v = optSel == 0 ? &musicV : optSel == 1 ? &fxV : nullptr;
        if (v && (edge & 48)) {
            if ((edge & 16) && *v > 0) (*v)--;
            if ((edge & 32) && *v < 4) (*v)++;
            synth::volume(musicV, fxV);
            sfx(optSel ? S_LASER : S_BOARD);
        }
        if ((edge & 8) || ((edge & 4) && optSel == 2)) {               // back: keep the levels with the save
            state = TITLE;
            sv.musicV = musicV; sv.fxV = fxV; saveFlag = hasSave; saveDue = true;
            sfx(S_GEAR);
        }
        break;
    }
#endif
    case PAUSED:
        if (edge & 1) pauseSel = (uint8_t)((pauseSel + 3) % 4);
        if (edge & 2) pauseSel = (uint8_t)((pauseSel + 1) % 4);
        if ((edge & 12) || (pauseSel == 1 && (edge & 48))) {
            if (pauseSel == 0) state = PLAY;
            else if (pauseSel == 1) { diff = (uint8_t)((diff + ((edge & 16) ? 2 : 1)) % 3); if (shield > shMax()) shield = shMax(); sfx(S_GEAR); }
            else if (pauseSel == 2) { saveProgress(true); pauseNote = save::available() ? MS_GAME_SAVED : MS_SAVING_IS_OFF; }
            else toTitle();
        }
        break;
    case INTRO:
        updateWeather(); updateCritters();
        updateCamera();
        if ((edge & 4) && timer > 100) timer = 100;                       // A: on to who's who, then to play
        else if (--timer <= 0 || ((edge & 4) && timer < 95)) { state = PLAY; say((stage & 3) == 3 ? (uint8_t)(MS_MOTHER + ((stage >> 2) & 3)) : wxLvl == 3 ? MS_STORM : MS_FLY_SAFE, 50); }
        break;
    case PLAY:
#ifdef SL_DLV
        {   // test: a bag on the ground near the start, a speeder on its way, a cow tumbling down; SL_DLV=2: the feast at the base
            static bool once;
            if (!once) {
                once = true;
                if (SL_DLV == 2) { placeLanderOnPad(basePadX); partyT = 200; }
                else if (SL_DLV == 6) { dlv = 2; bagX = LX() + 2; bagY = groundAt(bagX) - 3; scX = LX() + 60; scY = groundAt(scX) - 7; }   // the bag right beside the lander
                else if (SL_DLV == 5) { lx = (int32_t)(alienX + 20 + (baseRight ? -40 : 40)) << 8; ly = (int32_t)56 << 8; lst = 1; gear = 1; shield = 99; ang = baseRight ? -1 : 1; facing = (int8_t)(baseRight ? -1 : 1); }   // near the boss, facing it
                else if (SL_DLV == 4) { if (Enemy *e = spawnEn(E_UFO, LX(), LY() - 26)) { e->st = 4; e->cargo = 10; } }
                else if (SL_DLV == 3) { crit[0] = { (int16_t)(LX() + 40), (int16_t)groundAt(LX() + 40), 1, 4, 100 }; spawnEn(E_UFO, LX() + 40, 40 + WDY); }
                else { dlv = 2; bagX = LX() + 40; bagY = groundAt(bagX) - 3; scX = LX() + 200; scY = groundAt(scX) - 7; crit[0] = { (int16_t)(LX() + 20), 30, 1, 3, 0 }; }
            }
        }
#endif
        if (const int ct = cutAt(); ct > -999) {                          // a cut-scene: everything else holds still
            updateDelivery(); updateWeather(); updateCritters();
            cam = clampi(cam + (ct - SW / 2 - cam) / 8, 0, W - SW);          // the camera goes to it
            break;
        }
        {
            static int lastDown = -99;                                  // the gear: DOWN tapped twice
            bool gearTap = false;
            if (edge & 2) { gearTap = frameNo - lastDown < 10; lastDown = gearTap ? -99 : frameNo; }
#ifdef CHGBA
            if (lEdge) gearTap = true;                              // L: the gear in one press
#endif
            updateLander(l, r, a, gearTap, b, u);
        }
        if (state != PLAY) break;
        updateMen();
        updateEnemies();
        updateShots();
        updateWeather();
        updateCritters();
        updateCamera();
        if ((nSaved >= quota || nSaved + nLost >= nMen || nLost > nMen - quota) && seats == 0 && !walking()) {
            if (++endT > 50) {
                missed = nSaved < quota;
                state = CLEAR; timer = 150; synth::engine(0);
                int grazing = 0;
                for (const auto &c : crit) grazing += c.kind == 4;
                cowsN += grazing;                                               // (and the ones brought home)
                stars = (uint8_t)(!missed + (nSaved == nMen) + !crashN);
                addScore(grazing * 250 + stars * 500 + fuel / 4 + (nSaved == nMen ? 2000 : 0), -1, 0);
                sfx(S_CLEAR);
            }
        }
        break;
    case CLEAR:
        updateShots(); updateWeather();
        if (--timer <= 0) {
            if (missed) {                                               // the quota missed: a lander is lost and the world is tried again
                if (--lives <= 0) { lives = 0; gameOver(); break; }
            } else stage++;
            newStage(); saveProgress(true);
            if (stage && (stage & 15) == 0) say(MS_LOOP, 90, stage / 16 + 1);   // round again, harder
        }
        break;
    case OVER:
        updateShots(); updateWeather();
        if (overT < 2000) overT++;
        if (overT > 90 && (edge & 4)) toTitle();
        break;
    }
#ifdef SL_TRACE
    if (frameNo % 30 == 0) for (auto &e : en) if (e.kind == E_WORM) fprintf(stderr, "worm f%d x%d st%d t%d lander%d,%d lost%d\n", frameNo, e.x >> 4, e.st, e.t, LX(), lst, nLost);
#endif
}

#ifdef CHSIM
void sim_sync();
void sim_advance(uint32_t us);
void sim_waitInput();
#endif

void setup() {
    chgame.boot();
#if CHGAME_DEBUG
    dbg::begin("STARLIFT " SL_VERSION);
#endif
    gfx_begin(GFX_DIV2, GFX_12BPP);
    const uint16_t pal[16] = {
        0, gfx_rgb(255, 255, 255), gfx_rgb(4, 4, 12), 0, gfx_rgb(70, 255, 100), 0, 0, 0, 0, 0, 0, 0,   // (C_SKY0, white, C_INK ...)
        gfx_rgb(184, 190, 208), gfx_rgb(66, 72, 104), gfx_rgb(255, 202, 44), gfx_rgb(232, 32, 44) };
    gfx_setPalette(pal, 16);
    synth::begin();
    chgame.setFrameRate(30);
    SaveData dsv;
    uint8_t flag = 0;
#ifdef SL_SLIM
    if (false) {
#else
    if (save::load(MAGIC, 1, dsv, &flag)) {
#endif
        hiScore = dsv.hi;
        if (flag == 1) hasSave = true;
        sv = dsv;
        musicV = dsv.musicV <= 4 ? dsv.musicV : 4; fxV = dsv.fxV <= 4 ? dsv.fxV : 4; diff = dsv.diff <= 2 ? dsv.diff : 1;
    }
    rng = 0x9E3779B9u ^ (millis() << 1);
    for (int i = 0; i < WXN; i++) { wxX[i] = (uint8_t)rnd(160); wxY[i] = (uint8_t)(TOP + rnd(SH - TOP)); }
    synth::volume(musicV, fxV);
    toTitle();
}

void loop() {
    static bool startPrev;
    const bool st = chgame.pressed(START_BUTTON);
    if (st && !startPrev) {
        if (state == PLAY) { state = PAUSED; pauseSel = 0; pauseNote = 0; synth::engine(0); sfx(S_PAUSE); }
        else if (state == PAUSED) { state = PLAY; sfx(S_RESUME); }
    }
    startPrev = st;
    dbg::markRenderStart();
    update();
    if (state == PLAY && (frameNo % 5) == 0) update();                // play runs a fifth faster: an extra step every fifth frame
    gfx_wait();
    if (saveDue) {
        saveDue = false;
#ifndef SL_SLIM                                                   // (a board perf build does not save)
        save::store(MAGIC, 1, sv, saveFlag ? 1 : 0);
        hasSave = saveFlag && save::available();
#endif
        if (hasSave) titleSel = 0;
    }
    card::frame();
    if (!card::ok() && (frameNo % 60) == 2) card::open();
    if (loadReq && card::ok() && card::copy((uint8_t *)&PLR, CARD_PLANET + (uint32_t)(stage & 7) * PLANET_LEN, PLANET_LEN)
        && card::copy((uint8_t *)&PK, CARD_PACK + (uint32_t)(stage & 15) * sizeof(Pack), sizeof(Pack))) {
        loadReq = false;
#ifdef CHGBA
        {   // the card's colours are in the CHGame order, where 0 is C_INK and 2 C_SKY0: Game.h swaps those two here
            auto fix = [](uint8_t &c) { if (c == 0) c = C_INK; else if (c == 2) c = C_SKY0; };
            fix(PLR.crack); fix(PLR.shopc); fix(PLR.basec);                  // (clouds: 0 is none, and never 2)
            for (auto &a : PLR.alien) for (auto &c : a) fix(c);
        }
#endif
        genStage();
        applyPalette();
        if (state == TITLE) cam = 200;
    }
    if (card::ok() && synth::wanted() >= 0) {                     // the next song, from the card
        const int id = synth::wanted();
        if (card::copy(synth::buffer(), CARD_SONG + SONG_OFF[id], SONG_LEN[id])) synth::loaded();
    }
    draw();
    dbg::markRenderEnd();
    gfx_flushAsync();
#ifdef CHSIM
    uint8_t idle = 0;
    for (dbg::poll(); !chgame.nextFrame(); dbg::poll()) {
        sim_sync();
        sim_advance(1000);
        if (++idle > 2) { sim_waitInput(); idle = 0; }
    }
#else
    do { dbg::poll(); } while (!chgame.nextFrame());
#endif
    chgame.pollButtons();
    dbg::markUpdateStart();
    static uint32_t startDown;
    if (!chgame.pressed(START_BUTTON)) startDown = 0;
    else if (!startDown) startDown = millis() | 1;
    else if (millis() - startDown >= 3000) chgame.exitToMenu();
}
