// GBA, patch 05-structure-obj (included into sketch.cpp after blit(const CSpr &)): a card picture as a hardware sprite.
// The same pixels as blit(c, x, y, flip, 255, map) for a picture whose pixels are not clipped (the full-screen clip
// rectangle); false: do that instead. The sprite's priority puts it where the picture's pixels were: in front of the
// background layers behind it, behind the ones in front and the picture (bcObj, gfx_gba.cpp).
static bool blitObj(const CSpr &s, int x, int y, bool flip, const uint8_t *map, int prio) {
    if (!card::ok() || x >= SW || x + s.w <= 0 || y >= SH || y + s.h <= 0) return true;   // (nothing to draw: as blit)
    const GfxClip cl = gfx__clip;
    if (cl.x0 || cl.y0 || cl.x1 != SW || cl.y1 != SH) return false;
    if (!card::at(CARD_PIC + s.off)) return true;
    uint32_t lut4[4];
    uint8_t *const lut = (uint8_t *)lut4;
    for (int k = 0; k < 14; k++) lut[k] = map[k];
    lut[14] = lut[15] = 0;
    Rd rd = { nullptr, CARD_PIC + s.off, 0, true };
    return bcObj(rd, s.w, s.h, x, y, flip, lut4, prio);
}
