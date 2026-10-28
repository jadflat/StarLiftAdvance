// StarLift Advance, patch 04-hills-bg (included by scenery_rows.inc): the far and mid hills on background layers 2 and 3.
//
// A layer's picture depends only on its own x (cam >> 3 for the far hills, cam >> 2 for the mid ones) and the screen
// row: its top (layerH), a lit or shaded rim from the slope, then its body (plain far, the world's texture mid). So, like
// the ground (ground_bg.inc), each is drawn once into tiles 8 columns at a time as they come into view and scrolled by
// the hardware: a ring of 32 tile columns, 256 px. Drawn into the picture they were 7 ms a frame (EWRAM).
//
// The picture is see-through down to the near hills (drawSky clears that far), so the layers show behind it, the far
// hills behind the mid ones (priorities 3, 2), both behind the ground (1) and the picture (0). What the game draws in
// between them, the sky objects behind both and each layer's structures behind the layers in front, is drawn into the
// picture only above the rows where it would be covered (gba_clipY, used by pset, gfx_hline and blitRd).
//
// Tiles (4 bits) at char block 1 (0x06004000) from tile 88: the VRAM between the two picture sets. 88 blank, 89 the
// far body, 90-93 the mid body's texture (16 x 16: 2 x 2 tiles), then HBG_SLOTS tiles a ring column per layer for the
// rows where its top and rim are. Maps: screen blocks 14 (far), 15 (mid), 0x06007000 and 0x06007800.
void gba_hillScroll(int far, int mid);       // gba/src/gfx_gba.cpp
extern "C" { extern const uint8_t *gba_clipY; }
enum { HBG_BASE = 0x06004000, HBG_BLANK = 88, HBG_FAR = 89, HBG_PAT = 90, HBG_SLOT0 = 94, HBG_SLOTS = 4 };
static int16_t hbgCol[2][32];                 // the layer tile column each ring column holds (-1: none)
uint32_t gba_hbgOverflow;                     // (tests) tile columns that needed more than HBG_SLOTS tiles
static inline volatile uint32_t *hbgTile(int idx) { return (volatile uint32_t *)(HBG_BASE + idx * 32); }
static inline volatile uint16_t *hbgMap(int L) { return (volatile uint16_t *)(0x06007000 + L * 0x800); }

__attribute__((noinline)) static void hbgReset() {
    for (int r = 0; r < 8; r++) { hbgTile(HBG_BLANK)[r] = 0; hbgTile(HBG_FAR)[r] = LBODY[0] * 0x11111111u; }
    const uint8_t lut[4] = { LDARK[1], LBODY[1], LBODY[1], LBODY[1] };      // as drawLayerCols (L 1)
    for (int k = 0; k < 4; k++)
        for (int r = 0; r < 8; r++) {
            uint32_t w = 0;
            for (int i = 0; i < 8; i++) w |= (uint32_t)lut[tile[(k & 1) * 8 + i][((k >> 1) * 8 + r) & 15]] << (i * 4);
            hbgTile(HBG_PAT + k)[r] = w;
        }
    for (int L = 0; L < 2; L++) {
        for (int i = 0; i < 32 * 32; i++) hbgMap(L)[i] = HBG_BLANK;
        for (int i = 0; i < 32; i++) hbgCol[L][i] = -1;
    }
}

// layer L's tile column X (its x 8X .. 8X + 7) into ring column X & 31
__attribute__((noinline)) static void hbgColumn(int L, int X) {
    const int slot = X & 31, u0 = X * 8;
    const uint8_t body = LBODY[L], lit = LLIT[L], dark = LDARK[L];
    const uint8_t lut[4] = { dark, body, body, body };
    int t[8], re[8], ymin = SH, ymax = 0;
    uint8_t rc[8];
    int prev = clampi(layerH(L, u0 - 1), TOP, SH - 1);
    for (int i = 0; i < 8; i++) {
        t[i] = clampi(layerH(L, u0 + i), TOP, SH - 1);                         // as layerTops()
        const int slope = prev - t[i];                                          // > 0: rising to the right, lit
        re[i] = t[i] + (slope > 0 ? 2 + slope * 2 : slope < 0 ? 2 - slope : 1);  // where the rim ends
        rc[i] = slope < 0 ? dark : lit;
        prev = t[i];
        if (t[i] < ymin) ymin = t[i];
        if (re[i] > ymax) ymax = re[i];
    }
    volatile uint16_t *map = hbgMap(L) + slot;
    int used = 0;
    for (int ty = 0; ty < SH / 8; ty++) {
        const int yt = ty * 8;
        uint16_t idx;
        if (yt + 7 < ymin) idx = HBG_BLANK;                                     // all above the top
        else if (yt >= ymax || used == HBG_SLOTS) {                              // all body
            if (yt < ymax) gba_hbgOverflow++;
            idx = (uint16_t)(L ? HBG_PAT + (X & 1) + 2 * (ty & 1) : HBG_FAR);
        } else {
            idx = (uint16_t)(HBG_SLOT0 + (L * 32 + slot) * HBG_SLOTS + used++);
            volatile uint32_t *tl = hbgTile(idx);
            for (int r = 0; r < 8; r++) {
                const int y = yt + r;
                uint32_t w = 0;
                for (int i = 0; i < 8; i++) {
                    uint32_t c;
                    if (y < t[i]) continue;                                     // sky: see-through
                    else if (y < re[i]) c = rc[i];
                    else c = L ? lut[tile[(u0 + i) & 15][y & 15]] : body;
                    w |= c << (i * 4);
                }
                tl[r] = w;
            }
        }
        map[ty * 32] = idx;
    }
    hbgCol[L][slot] = (int16_t)X;
}

static uint8_t hbgClipA[3][SW];              // per column: the rows things behind layers 0, 1, 2 stop at (hbgPrep)

// the far and mid layers for this frame: new tile columns, the scroll
__attribute__((noinline)) static void hbgDraw() {
    if (hbgStageNew) { hbgStageNew = false; hbgReset(); }
    for (int L = 0; L < 2; L++) {
        const int lc = cam >> (3 - L);
        for (int X = lc >> 3, X1 = (lc + SW - 1) >> 3; X <= X1; X++) if (hbgCol[L][X & 31] != X) hbgColumn(L, X);
    }
    gba_hillScroll(cam >> 3, cam >> 2);
}

// once a frame, when the sky is cleared (all four layers' tops known): the clip rows, the near hills' columns (patch
// 01's slgCol, as slgColumns made them) and how far down the picture is see-through (the near hills' highest top)
SLG_IWRAM static int hbgPrep() {
    int low = TOP, p2 = topA[2][0];
#pragma clang loop unroll(disable)
    for (int x = 0; x < SW; x++) {
        const int a = topA[0][x], b = topA[1][x], c = topA[2][x], d = topA[3][x];
        const int m2 = c < d ? c : d, m1 = b < m2 ? b : m2, m0 = a < m1 ? a : m1;
        hbgClipA[0][x] = (uint8_t)m0; hbgClipA[1][x] = (uint8_t)m1; hbgClipA[2][x] = (uint8_t)m2;
        if (m2 > low) low = m2;
        slgCol[0][x] = slgWord(c, d, p2 - c);
        p2 = c;
    }
    return low;
}

// clip what is drawn next to the rows above layers from - 3 (from = 0: the sky objects, behind every layer; 1, 2: the far
// and mid layers' structures, behind the layers in front of theirs); -1: no clip
static inline void hbgClipFor(int from) { gba_clipY = from < 0 ? nullptr : hbgClipA[from]; }

// the sky objects, behind every layer (the game's drawSkyObjects, renamed skyObjects_ by this patch)
static void drawSkyObjects() { hbgClipFor(0); skyObjects_(); hbgClipFor(-1); }
