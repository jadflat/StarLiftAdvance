// The GBA half of CHGfx: replaces CHGfx.cpp (the SPI/DMA transport of the board's ST7735 panel).
//
// The game draws into gfx_fb (128x128, 4 bits a pixel, a 16-colour palette) exactly as on the board. A flush converts
// it to tiles (mode 0, one 4-bit background of 16x16 tiles: the framebuffer's nibbles are already the GBA's 4bpp pixels,
// so a flush is a word copy into the tile order) in the set that is not on show, and the next VBlank flips to it and loads
// the palette: no tearing, and gfx_wait() is "the last flip has happened".
//
// Star Lift GBA FS: the library is built 240x160 (GFX_W, GFX_H), the GBA's own screen: GBA_NATIVE. The picture is
// BG0 (mode 0, 4 bits a pixel), 30x20 tiles, nothing scaled. The frame (19,200 bytes) lives in EWRAM.
// The 128x128 builds (the CHStarlift port): GBA_SCALE picks the picture size (both cost the same: the flush is the same word copy, only the tile order differs):
//   100: 1:1, 128x128 in the middle of the 240x160 screen on BG0; the border is the backdrop, the game's colour 0.
//   125: 160x160 (1.25x, nearest neighbour), centred, black borders. Scaling needs an affine layer; affine backgrounds
//        want 8 bits a pixel, but affine sprites take 4-bit tiles. So the picture is four 64x64 4bpp sprites (OBJ tiles,
//        2D mapping), each scaled to 80x80 in double-size mode; behind them a BG0 of solid colour 0 (sprite colour 0 is
//        transparent), and window 0 clips both to the 160x160 square.
//   240: the whole 240x160 screen (1.875x across, 1.25x down), without the shimmer a nearest-neighbour stretch has when
//        the scenery scrolls (some game pixels shown 1 screen pixel wide, some 2, the pattern fixed while the picture
//        slides under it). Mode 2: BG2 and BG3 are the same 8-bit tiles, stretched alike, BG3 sampling half a game pixel
//        to the right of BG2, and the hardware blends the two 50/50. So every edge between two game pixels is a
//        half-and-half column wherever it lands, and each game pixel keeps a steadier width as it moves. The flush
//        writes 8-bit tiles (colour + 1: 0 is transparent in a tile), twice the bytes of the 4-bit modes. Not the default:
//        it stretches the picture out of its square, and in mGBA the flush took ~11 ms a frame (5 ms writing to EWRAM
//        instead of VRAM), which pushes every frame past 33 ms.
#include <CHGfx.h>
#include <CHGfx_internal.h>
#include <Arduino.h>
#include "hw.h"

bool gfx__paletteTake(uint16_t out[16]);   // CHGfx_palette.cpp
void gfx__paletteTouch(void);

#if GFX_W == 240 && GFX_H == 160
#define GBA_NATIVE 1
#define GBA_SCALE 0
#else
#define GBA_NATIVE 0
#endif
#ifndef GBA_SCALE
#define GBA_SCALE 125
#endif
#ifndef GBA_FILL_SMOOTH
#define GBA_FILL_SMOOTH 1                   // GBA_SCALE 240: 0 shows BG2 alone (a plain nearest-neighbour stretch, to compare)
#endif

#if GBA_NATIVE
uint8_t gfx_fb[GFX_FB_BYTES] __attribute__((aligned(4), section(".ewram")));   // (IWRAM has no room for 19 KB)
#else
uint8_t gfx_fb[GFX_FB_BYTES] __attribute__((aligned(4)));
#endif
CHGfx Gfx;

static uint8_t s_mode = GFX_16BPP, s_div = GFX_DIV2;
static uint8_t s_scratch[2 * GFX_CHUNK_BYTES] __attribute__((aligned(4)));

volatile uint8_t gba_flipPending;
static uint8_t s_back = 1;                  // the page the next frame is drawn into (the other one is on show)
static uint8_t s_shown = 0;
static uint16_t s_pal[16];                  // BGR555, loaded at the flip
static volatile uint8_t s_palPending;


// ---- VBlank: show the finished set, load its palette ---------------------------------------------------------------
// Tile set A is tiles 0-255 (VRAM 0x06000000) shown by the map in screenblock 28, set B tiles 256-511 by screenblock 29;
// tile 512 stays blank and fills the map around the picture.
// GBA_SCALE 125: the sets are OBJ tiles in the 32-tile-wide 2D grid at 0x06010000, set A in columns 0-15, set B in 16-31;
// the four sprites only change their first tile at the flip.
#define SBLOCK(i) ((volatile uint16_t *)(0x06000000 + (i) * 0x800))
#if GBA_SCALE == 125
static const uint8_t kSpr[4][2] = { {1, 1}, {1, 0}, {0, 1}, {0, 0} };   // (x, y) of each 64x64 quarter; see gfx__begin
#endif
#if GBA_NATIVE
uint16_t gba_grad[2][161];                  // the sky's line colours, per set ([160] is read by the last HBlank): EWRAM
#define s_grad gba_grad
static void skyStart(const uint16_t *g);
static void mosaicStep();
static void beamShow();
static uint8_t s_g1x[2];                    // BG1's scroll per set (see gba_groundScroll)
static uint8_t s_hx[2][2], s_hOn[2];        // the hills' layers (BG2 far, BG3 mid): scroll per set, shown or not (gba_hillScroll)
static uint8_t s_g1xNext, s_hxNext[2], s_hOnNext;   // the frame being drawn's: given to its set by present() (a frame is
                                            // drawn while the one before still waits for its flip, so not straight to s_back)
#endif
extern "C" { const uint8_t *gba_clipY; }      // while set: nothing is drawn at or below row gba_clipY[x] (patch 04-hills-bg)
void gba_vblank() {
#if GBA_NATIVE
    if (!gba_flipPending) { skyStart(s_grad[s_shown]); return; }   // (the HBlank DMA restarts every frame)
#endif
    if (!gba_flipPending) return;
    s_shown = s_back; s_back ^= 1;
#if GBA_NATIVE
    REG_BG0CNT = (uint16_t)((s_shown ? 2 : 0) << 2 | 31 << 8 | 0x40);  // tile set A (char block 0) or B (2), map at 31, mosaic on
    mosaicStep();
    beamShow();
    REG16(0x04000014) = s_g1x[s_shown];                              // BG1HOFS
    REG16(0x04000018) = s_hx[s_shown][0];                           // BG2HOFS: the far hills
    REG16(0x0400001C) = s_hx[s_shown][1];                           // BG3HOFS: the mid hills
    REG_DISPCNT = (uint16_t)((REG_DISPCNT & ~0x0C00) | (s_hOn[s_shown] ? 0x0C00 : 0));   // only under frames that drew them
    if (s_palPending) { for (int i = 1; i < 16; i++) PAL_BG[i] = PAL_OBJ[i] = s_pal[i]; s_palPending = 0; }
    skyStart(s_grad[s_shown]);
#elif GBA_SCALE == 240
    REG_BG2CNT = (uint16_t)(s_shown << 2 | 16 << 8);                 // tile set A or B (char block 0 or 1), map at 16
    REG_BG3CNT = (uint16_t)(s_shown << 2 | 16 << 8 | 1);             // (priority 1: under BG2, the blend's second layer)
    if (s_palPending) { for (int i = 0; i < 16; i++) PAL_BG[1 + i] = s_pal[i]; s_palPending = 0; }
#elif GBA_SCALE == 125
    for (int k = 0; k < 4; k++) OAM[k * 4 + 2] = (uint16_t)(kSpr[k][1] * 8 * 32 + kSpr[k][0] * 8 + s_shown * 16);
    if (s_palPending) { for (int i = 0; i < 16; i++) PAL_OBJ[i] = s_pal[i]; PAL_BG[1] = s_pal[0]; s_palPending = 0; }
#else
    REG_BG0CNT = (uint16_t)((28 + s_shown) << 8);
    if (s_palPending) { for (int i = 0; i < 16; i++) PAL_BG[i] = s_pal[i]; s_palPending = 0; }
#endif
    gba_flipPending = 0;
}

#if GBA_NATIVE
// ---- the sky: colour 0 (C_SKY0) is transparent, and the backdrop behind it changes colour every screen line (DMA 0 at
// each HBlank), a smooth version of the game's dithered gradient: SKY0 down to y0, into SKY1 by about y0 + 25, then a
// little of FAR (a haze) towards y1. The game calls gba_skyGradient() while drawing; frames without it get plain SKY0.
static int s_skyY0 = -1, s_skyY1;
void gba_skyGradient(int y0, int y1) { s_skyY0 = y0; s_skyY1 = y1; }
// the ground's layer (BG1, drawn by patch 01's ground_bg.inc): its scroll for each set, applied with it
void gba_groundScroll(int x) { s_g1xNext = (uint8_t)x; }
// the far and mid hills' layers (patch 04-hills-bg): their scroll for the frame being drawn, which shows them
void gba_hillScroll(int far, int mid) { s_hxNext[0] = (uint8_t)far; s_hxNext[1] = (uint8_t)mid; s_hOnNext = 1; }
// ---- see-through tractor beams: the game's beam pattern drawn into 32x64 sprites (semi-transparent: the hardware blends
// them over the picture). Sprites are per tile set: drawn into the back set's tiles while the frame is drawn, shown with it.
enum { BEAM_SPR = 12, BEAM_TILES = 32 };                         // sprites a set; a 32x64 sprite is 32 tiles (1 KB)
struct BeamSpr { int16_t x, y; };
static BeamSpr s_beam[2][BEAM_SPR];
static uint8_t s_beamN[2];
uint32_t gba_beamBuf[256];                                       // one sprite, drawn here and then copied to VRAM (EWRAM)
#define s_beamBuf gba_beamBuf
static void beamPix(int x, int y, uint8_t c) {                   // x 0-31, y 0-63 in the sprite (1D tile order)
    const int t = (y >> 3) * 4 + (x >> 3), o = t * 8 + (y & 7);
    s_beamBuf[o] |= (uint32_t)c << ((x & 7) * 4);
}
void gba_beam(int ux, int uy, int gy, int frameNo) {             // as the game's drawBeam(), see-through
    const int top = uy + 5, len = gy - top;
    if (len <= 0) return;
    for (int y0 = top; y0 < gy && s_beamN[s_back] < BEAM_SPR; y0 += 64) {
        const int y1 = gy < y0 + 64 ? gy : y0 + 64;
        memset(s_beamBuf, 0, sizeof s_beamBuf);
        for (int y = y0; y < y1; y++) {
            const int hw = 2 + (y - top) * 6 / len;
            for (int dx = -hw; dx <= hw; dx++) {
                const int k = (y * 2 - frameNo * 3 + (dx < 0 ? -dx : dx)) & 7, x = ux + dx;
                uint8_t c = 0;
                if (dx == hw || dx == -hw) { if ((y + frameNo) & 1) c = 4; }        // C_GREEN
                else if (k == 0) c = 1;                                             // C_WHITE
                else if (k < 3 && ((x + y + frameNo) & 1)) c = 4;
                if (c) beamPix(dx + 16, y - y0, c);
            }
        }
        const int n = s_beamN[s_back]++;
        volatile uint32_t *d = (volatile uint32_t *)(0x06010000 + (s_back * BEAM_SPR + n) * BEAM_TILES * 32);
        for (int i = 0; i < 256; i++) d[i] = s_beamBuf[i];
        s_beam[s_back][n] = { (int16_t)(ux - 16), (int16_t)y0 };
    }
}
static void beamShow() {                                         // at the flip: the shown set's sprites
    for (int i = 0; i < BEAM_SPR; i++) {
        volatile uint16_t *o = OAM + i * 4;
        if (i >= s_beamN[s_shown]) { o[0] = 0x0200; continue; }
        const BeamSpr &b = s_beam[s_shown][i];
        if (b.y >= 160 || b.y <= -64 || b.x >= 240 || b.x <= -32) { o[0] = 0x0200; continue; }
        o[0] = (uint16_t)((b.y & 255) | 0x0400 | 0x8000);              // semi-transparent, tall shape
        o[1] = (uint16_t)((b.x & 511) | 0xC000);                         // 32x64
        o[2] = (uint16_t)((s_shown * BEAM_SPR + i) * BEAM_TILES);        // priority 0
    }
    s_beamN[s_back] = 0;                                         // the new back set starts with none
}

// the mosaic: blocks of up to 5 pixels that shrink back to 1 over the given frames (at each shown frame)
static uint8_t s_mosaicT, s_mosaicLen;
void gba_mosaic(uint8_t frames) { s_mosaicT = s_mosaicLen = frames; }
static void mosaicStep() {
    const int m = s_mosaicT ? (s_mosaicT * 4 + s_mosaicLen - 1) / s_mosaicLen : 0;   // 4 .. 1 .. 0 (block size - 1)
    REG_MOSAIC = (uint16_t)(m | m << 4);
    if (s_mosaicT) s_mosaicT--;
}
__attribute__((target("arm"))) static uint16_t mix555(uint16_t a, uint16_t b, int t) {     // a + (b - a) * t / 256, per 5-bit channel
    int r = 0;
    for (int sh = 0; sh < 15; sh += 5) {
        const int ca = (a >> sh) & 31, cb = (b >> sh) & 31;
        r |= ((ca + (((cb - ca) * t) >> 8)) & 31) << sh;
    }
    return (uint16_t)r;
}
__attribute__((target("arm"))) static void skyBuild(uint16_t *g) {
    const uint16_t s0 = s_pal[0], s1 = s_pal[3], far = s_pal[5];      // C_SKY0, C_SKY1, C_FAR (Game.h)
    static uint32_t lastKey[2] = { 1, 1 }, lastKey2[2];                 // per table: rebuilt only when its inputs change
    const int k = g == s_grad[0] ? 0 : 1;
    const uint32_t key = (uint32_t)s0 | (uint32_t)s1 << 16, key2 = (uint32_t)far | (uint32_t)(s_skyY0 & 255) << 16 | (uint32_t)(s_skyY1 & 255) << 24;
    if (key == lastKey[k] && key2 == lastKey2[k]) { s_skyY0 = -1; return; }
    lastKey[k] = key; lastKey2[k] = key2;
    if (s_skyY0 < 0) { for (int y = 0; y <= 160; y++) g[y] = s0; s_skyY0 = -1; return; }
    // (rebuilt every frame of a flash or a fade, when the palette changes: one division, not one a line, on the ARM7)
    const int span = s_skyY1 - s_skyY0;
    const uint32_t recip = (1u << 24) / (uint32_t)span;                 // 1 / span, x 2^24
    for (int y = 0; y <= 160; y++) {
        const int d = y - s_skyY0, n = (d < 0 ? -d : d) * 7 * 256;       // the game's level (0-7) x 256: n / span, as C rounds
        int t = (int)(((uint64_t)(uint32_t)n * recip) >> 24);           // (within 1 of it: corrected below)
        while (t * span > n) t--;
        while ((t + 1) * span <= n) t++;
        if (d < 0) t = -t;
        int a = (t - 384) * 256 / 896;                                  // SKY0 -> SKY1 over levels 1.5 .. 5
        a = a < 0 ? 0 : a > 256 ? 256 : a;
        uint16_t c = mix555(s0, s1, a);
        if (t > 6 * 256) { int h = (t - 6 * 256) / 16; if (h > 32) h = 32; c = mix555(c, far, h); }   // up to 1/8 haze
        g[y] = c;
    }
    s_skyY0 = -1;
}
static void skyStart(const uint16_t *g) {   // at VBlank: line 0's colour now, the rest by HBlank DMA
    REG_DMA0CNT_H = 0;
    PAL_BG[0] = g[0];
    REG_DMA0SAD = (uint32_t)(g + 1); REG_DMA0DAD = 0x05000000; REG_DMA0CNT_L = 1;
    REG_DMA0CNT_H = 0x8000 | 0x2000 | 0x0200 | 0x0040;              // on, HBlank, repeat, dest fixed, 16 bit
}
#endif

// gfx_fb -> the back set: row by row, one word (8 pixels) per tile.
#if GBA_NATIVE
// 30x20 tiles of 8 words; a frame row is 30 words, one per tile. A word costs 4 cycles to read from EWRAM (main.cpp's
// 1 wait state) and 3 to str to VRAM; a single ldr adds 2 more (fetch, I cycle), so each row is read as three 10-word
// ldm and stored with immediate offsets 32 bytes apart: ~7.2 cycles a word against ~9.6 for the C loop (mGBA: 0.6 ms a
// frame less). A transpose in registers to stm whole tile rows costs the same again in pointer adds.
static IWRAM_TEXT __attribute__((naked)) void convertTo(const uint32_t *src, volatile uint32_t *set) {
    asm volatile(
        "push {r4-r11, lr}\n"
        "mov r12, #20\n"                            // bands of 8 rows (a row of tiles)
        "1: mov lr, #8\n"
        "2:\n"
        ".irp g, 0, 320, 640\n"                     // tiles 0-9, 10-19, 20-29 of this row
        "ldmia r0!, {r2-r11}\n"
        "str r2, [r1, #\\g]\n      str r3, [r1, #\\g+32]\n  str r4, [r1, #\\g+64]\n  str r5, [r1, #\\g+96]\n"
        "str r6, [r1, #\\g+128]\n  str r7, [r1, #\\g+160]\n str r8, [r1, #\\g+192]\n str r9, [r1, #\\g+224]\n"
        "str r10, [r1, #\\g+256]\n str r11, [r1, #\\g+288]\n"
        ".endr\n"
        "add r1, r1, #4\n"                          // the next pixel row of the same tiles
        "subs lr, lr, #1\n"
        "bne 2b\n"
        "add r1, r1, #960 - 32\n"                   // the next row of tiles
        "subs r12, r12, #1\n"
        "bne 1b\n"
        "pop {r4-r11, lr}\n"
        "bx lr\n");
}
#ifdef GBA_CONVCHECK
extern volatile uint32_t gba_dbgN;
extern "C" { volatile uint32_t gba_cvChecks, gba_cvBad, gba_cvBadFirst; }   // frames.py's CHECK line: every word, every frame
#endif
static void convert() {
    volatile uint32_t *const set = (volatile uint32_t *)(0x06000000 + s_back * 0x8000);
    convertTo((const uint32_t *)gfx_fb, set);
#ifdef GBA_CONVCHECK
    const uint32_t *src = (const uint32_t *)gfx_fb;
    for (int y = 0; y < 160; y++)
        for (int tx = 0; tx < 30; tx++)
            if (set[(y >> 3) * 240 + tx * 8 + (y & 7)] != *src++ && !gba_cvBad++) gba_cvBadFirst = gba_dbgN << 16 | y << 8 | tx;
    gba_cvChecks++;
#endif
}
#elif GBA_SCALE == 240
// 8-bit tiles (64 bytes, a tile row is 2 words) in char block 0 or 1; each 4-bit pixel p becomes the byte p + 1.
static inline __attribute__((always_inline, target("arm"))) uint32_t spread8(uint32_t h) {       // 4 nibbles -> 4 bytes, + 1 each
    h = (h | h << 8) & 0x00FF00FFu;
    return ((h | h << 4) & 0x0F0F0F0Fu) + 0x01010101u;
}
static IWRAM_TEXT void convert() {
    volatile uint32_t *const set = (volatile uint32_t *)(0x06000000 + s_back * 0x4000);
    const uint32_t *src = (const uint32_t *)gfx_fb;
    for (int y = 0; y < 128; y++) {
        volatile uint32_t *d = set + (y >> 3) * 16 * 16 + (y & 7) * 2;  // tile row y/8, pixel row y%8 of its tiles
        for (int tx = 0; tx < 16; tx++, d += 16) {
            const uint32_t w = *src++;
            d[0] = spread8(w & 0xFFFF);
            d[1] = spread8(w >> 16);
        }
    }
}
#else
#if GBA_SCALE == 125
#define SET_BASE (0x06010000 + s_back * 16 * 32)
#define TILE_ROW (32 * 8)                   // words from one row of tiles to the next
#else
#define SET_BASE (0x06000000 + s_back * 0x2000)
#define TILE_ROW (16 * 8)
#endif
static IWRAM_TEXT void convert() {
    volatile uint32_t *dst = (volatile uint32_t *)SET_BASE;
    const uint32_t *src = (const uint32_t *)gfx_fb;
    for (int ty = 0; ty < 16; ty++) {
        for (int r = 0; r < 8; r++) {
            volatile uint32_t *d = dst + r;
            for (int tx = 0; tx < 16; tx += 4) {
                const uint32_t a = src[0], b = src[1], c = src[2], e = src[3];
                src += 4;
                d[0] = a; d[8] = b; d[16] = c; d[24] = e;
                d += 32;
            }
        }
        dst += TILE_ROW;
    }
}
#endif

// Frame statistics for the emulator harness (tools/gdbdrive.py reads them over the debug stub): per flush, the time
// since the last one and the time the CPU spent halted (idle) in between. busy = period - idle.
volatile uint32_t gba_convUs, gba_drawUs;
static uint32_t s_tWait;
volatile uint32_t gba_dbgN, gba_dbgPeriod[128], gba_dbgIdle[128], gba_idleUs;
static uint32_t s_lastFlush;

static void waitFlip() {
    if (!gba_flipPending) return;
    const uint32_t t0 = micros();
    while (gba_flipPending) gba_halt();
    gba_idleUs += micros() - t0;
}

// The framebuffer is copied into the back tile set at once, so it is free the moment present() returns: nothing waits for the
// flip (a frame the VBlank has not shown yet is simply replaced by the newer one).
static void present() {
    uint16_t out[16];
    gfx__paletteTake(out);                  // the staged palette applies from here
    for (int i = 0; i < 16; i++) {
        const uint16_t c = out[i];          // RGB565 -> BGR555
        s_pal[i] = (uint16_t)(((c >> 11) & 31) | (((c >> 6) & 31) << 5) | ((c & 31) << 10));
    }
    s_palPending = 1;
    const uint32_t c0 = micros();
    if (s_tWait) gba_drawUs += c0 - s_tWait;   // from gfx_wait() to here: the card, the drawing
#if GBA_NATIVE
    skyBuild(s_grad[s_back]);
    s_g1x[s_back] = s_g1xNext;                                      // this frame's layer settings go with its tiles
    s_hx[s_back][0] = s_hxNext[0]; s_hx[s_back][1] = s_hxNext[1]; s_hOn[s_back] = s_hOnNext;
    s_hOnNext = 0;                                                  // (the next frame shows the hills only if it draws them)
#endif
    convert();
    gba_convUs += micros() - c0;
    gba_flipPending = 1;
    const uint32_t now = micros();
    gba_dbgPeriod[gba_dbgN & 127] = now - s_lastFlush;
    gba_dbgIdle[gba_dbgN & 127] = gba_idleUs;
    gba_dbgN++; gba_idleUs = 0; s_lastFlush = now;
}

void gfx_flushRectAsync(int, int, int, int) { present(); }
void gfx_flushRect(int, int, int, int)      { present(); waitFlip(); }
void gfx_flush(void)      { present(); waitFlip(); }
void gfx_flushAsync(void) { present(); }
bool gfx_busy(void)       { return false; }
void gfx_wait(void)       { s_tWait = micros(); }
int  gfx_flushRow(void)   { return GFX_H; }
void gfx_waitRow(int)     {}
void gfx_stream(gfx_streamFn, void *) {}

// ---- configuration and the rest of the API ---------------------------------------------------------------------------
void gfx__begin(uint8_t spiDiv, uint8_t colorMode) {
    s_div = spiDiv; s_mode = colorMode;
    static const uint16_t defpal[16] = {
        0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F,
        0x8410, 0xC618, 0x7800, 0x03E0, 0x000F, 0x8400, 0x0410, 0x4208
    };
    gfx_setPalette(defpal, 16);
    memset(gfx_fb, 0, sizeof gfx_fb);
    for (int i = 0; i < 256; i++) PAL_BG[i] = 0;
#if GBA_NATIVE
    for (volatile uint32_t *v = (volatile uint32_t *)0x06000000; v < (volatile uint32_t *)0x06010000; v++) *v = 0;   // both sets, the map
    volatile uint16_t *map = SBLOCK(31);
    for (int ty = 0; ty < 20; ty++) for (int tx = 0; tx < 30; tx++) map[ty * 32 + tx] = (uint16_t)(ty * 30 + tx);
    REG_BG0CNT = 31 << 8;
    for (int i = 0; i < 32 * 32; i++) SBLOCK(30)[i] = 88;           // the ground's map: its blank tile until it is drawn
    REG16(0x0400000A) = 3 << 2 | 30 << 8 | 0x40 | 1;                 // BG1CNT: tiles at char block 3, map 30, mosaic, priority 1
    // the hills (patch 04-hills-bg): tiles at char block 1 from tile 88 (VRAM between the two picture sets), maps 14, 15;
    // their maps are filled before they are first shown
    REG16(0x0400000C) = 1 << 2 | 14 << 8 | 0x40 | 3;                 // BG2CNT: the far hills, behind everything
    REG16(0x0400000E) = 1 << 2 | 15 << 8 | 0x40 | 2;                 // BG3CNT: the mid hills
    for (int i = 0; i < 128; i++) OAM[i * 4] = 0x0200;              // all sprites off
    REG_BLDCNT = 0x0100 | 0x0200 | 0x0400 | 0x0800 | 0x2000;        // semi-transparent sprites blend over BG0-3, the backdrop
    REG_BLDALPHA = 11 | 8 << 8;                                     // the beams 11/16, what is behind 8/16
    REG_DISPCNT = 0x0100 | 0x0200 | 0x1000 | 0x0040;   // mode 0, BG0 (the picture), BG1 (the ground), sprites (1D mapping)
#elif GBA_SCALE == 240
    for (volatile uint32_t *v = (volatile uint32_t *)0x06000000; v < (volatile uint32_t *)0x06008200; v++) *v = 0;   // 2 tile sets, the map
    volatile uint16_t *map = (volatile uint16_t *)0x06008000;        // screen block 16: a 16x16 map of 1-byte tile numbers 0-255
    for (int i = 0; i < 128; i++) map[i] = (uint16_t)(i * 2 | (i * 2 + 1) << 8);
    // 240x160 from 128x128: a step of 136/256 = 0.531 game pixels across (the last column samples 126.97) and 204/256 down
    REG_BG2PA = 136; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 204; REG_BG2X = 0; REG_BG2Y = 0;
    REG_BG3PA = 136; REG_BG3PB = 0; REG_BG3PC = 0; REG_BG3PD = 204; REG_BG3X = 128; REG_BG3Y = 0;   // half a game pixel right
    REG_BG2CNT = 16 << 8; REG_BG3CNT = 16 << 8 | 1;
    REG_BLDCNT = 0x0004 | 0x0040 | 0x0800;                           // BG2 blended (alpha) over BG3
    REG_BLDALPHA = 8 | 8 << 8;                                       // 8/16 of each
    REG_DISPCNT = 0x0002 | 0x0400 | (GBA_FILL_SMOOTH ? 0x0800 : 0);   // mode 2, BG2, BG3
#elif GBA_SCALE == 125
    for (int i = 0; i < 256; i++) PAL_OBJ[i] = 0;
    for (volatile uint32_t *v = (volatile uint32_t *)0x06000000; v < (volatile uint32_t *)0x06018000; v++) *v = 0;   // BG and OBJ tiles
    for (int i = 0; i < 8; i++) ((volatile uint32_t *)0x06000000)[i] = 0x11111111;   // BG tile 0: solid colour 1 (= the game's 0)
    for (int i = 0; i < 32 * 32; i++) SBLOCK(28)[i] = 0;
    for (int i = 0; i < 128; i++) OAM[i * 4] = 0x0200;              // all sprites off
    // Each quarter: a 64x64 sprite in a 128x128 double-size box, scaled by 256/204 (an 80x80 image from box offset 24).
    // The hardware samples box pixel i at 32 + (i - 64) * 204/256, which leaves an 81st pixel at offset 104 that overlaps
    // the next quarter's first; the right and lower quarters come first in OAM, so theirs (the right sample) is on top,
    // and the window cuts the one past the picture's edge.
    for (int k = 0; k < 4; k++) {
        const int x = 40 + kSpr[k][0] * 80 - 24, y = kSpr[k][1] * 80 - 24;
        OAM[k * 4 + 0] = (uint16_t)((y & 255) | 0x0300);            // affine, double size, 4bpp, square
        OAM[k * 4 + 1] = (uint16_t)((x & 511) | (0 << 9) | (3 << 14));   // affine group 0, 64x64
        OAM[k * 4 + 2] = (uint16_t)(kSpr[k][1] * 8 * 32 + kSpr[k][0] * 8);
    }
    OAM[3] = 204; OAM[7] = 0; OAM[11] = 0; OAM[15] = 204;            // affine group 0: pa, pb, pc, pd
    REG_WIN0H = (40 << 8) | 200; REG_WIN0V = (0 << 8) | 160;
    REG_WININ = 0x11; REG_WINOUT = 0;                                // BG0 and OBJ inside, the black backdrop outside
    REG_BG0CNT = 28 << 8;
    REG_DISPCNT = 0x0100 | 0x1000 | 0x2000; // mode 0, BG0, OBJ (2D mapping), window 0
#else
    for (volatile uint32_t *v = (volatile uint32_t *)0x06000000; v < (volatile uint32_t *)0x06008000; v++) *v = 0;   // tiles (512 is blank)
    for (int m = 0; m < 2; m++) {
        volatile uint16_t *map = SBLOCK(28 + m);
        for (int i = 0; i < 32 * 32; i++) map[i] = 512;
        for (int ty = 0; ty < 16; ty++) for (int tx = 0; tx < 16; tx++) map[(ty + 2) * 32 + tx + 7] = (uint16_t)(m * 256 + ty * 16 + tx);
    }
    REG_BG0CNT = 28 << 8;
    REG_DISPCNT = 0x0100;                   // mode 0, BG0
#endif
    s_back = 1; s_shown = 0; gba_flipPending = 0;
}
void gfx__begin12(uint8_t spiDiv)       { gfx__begin(spiDiv, GFX_12BPP); }
void gfx_setSpiDiv(uint8_t d)           { s_div = d; }
void gfx_setColorMode(uint8_t m)        { s_mode = m; gfx__paletteTouch(); }
uint8_t gfx_colorMode(void)             { return s_mode; }
uint8_t gfx_spiDiv(void)                { return s_div; }
uint32_t gfx_spiHz(void)                { return F_CPU / s_div; }
void gfx_setPanelOffsets(uint8_t, uint8_t, uint8_t) {}
void gfx_setInverted(bool) {}
void gfx_setPanelFrameRate(uint8_t, uint8_t, uint8_t) {}
uint8_t *gfx_chunkScratch(void)         { return s_scratch; }
uint8_t gfx_bytesPerPixel(void)         { return s_mode == GFX_18BPP ? 3 : 2; }
uint32_t gfx_frameBytes(void)           { return GFX_W * GFX_H * 2u; }

// Direct-to-panel paths: the GBA has none.
void gfx_select(void) {}
void gfx_deselect(void) {}
void gfx_setWindow(uint8_t, uint8_t, uint8_t, uint8_t) {}
void gfx_cmd(uint8_t) {}
void gfx_data8(uint8_t) {}
void gfx_writeColorLut(void) {}
void gfx_setWriteColorLut(bool) {}
void gfx_directFillRect(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}
void gfx_directBlit(const void *, uint32_t, bool) {}
void gfx_blockingWrite(const uint8_t *, uint32_t) {}
uint32_t gfx_convertSpan_ram(uint8_t *dst, const uint8_t *src, uint32_t n) {
    uint16_t *d = (uint16_t *)dst;
    for (uint32_t i = 0; i < n; i++) {
        *d++ = gfx_paletteOut(src[i] & 15);
        *d++ = gfx_paletteOut(src[i] >> 4);
    }
    return n * 4;
}
uint32_t gfx_convertRows_ram(uint8_t *dst, uint16_t row, uint16_t rows) {
    return gfx_convertSpan_ram(dst, gfx_fb + row * GFX_FB_STRIDE, (uint32_t)rows * GFX_FB_STRIDE);
}
uint32_t gfx_convertRows_flash(uint8_t *dst, uint16_t row, uint16_t rows) {
    return gfx_convertRows_ram(dst, row, rows);
}
