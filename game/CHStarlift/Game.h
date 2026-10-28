// STARLIFT: the game's types (a header, so the Arduino build's generated prototypes can see them).
#pragma once
#include <stdint.h>

// The screen's sixteen colours: nine follow the planet (sky, the three background layers, the ground, an accent), seven are fixed.
// Star Lift GBA FS: C_SKY0 is colour 0 (the CHGame version has C_INK there), the colour the GBA shows through: the sky is
// left as colour 0 and the hardware paints a smooth gradient behind the picture, a colour a screen line
enum Col : uint8_t { C_SKY0, C_WHITE, C_INK, C_SKY1, C_GREEN, C_FAR, C_MID, C_NEAR, C_GNDD, C_GND, C_GNDL, C_ACC, C_SILVER, C_STEEL, C_GOLD, C_RED };
enum Theme : uint8_t { TH_MOON, TH_RUINS, TH_BASE, TH_ALIEN };
enum Weather : uint8_t { WX_METEOR, WX_DUST, WX_SNOW, WX_RAIN, WX_ASH, WX_DRIZZLE, WX_GLINT, WX_ACID };
enum Style : uint8_t { ST_SMOOTH, ST_JAGGED, ST_MESA, ST_SPIKES, ST_CONES };
enum SkyObj : uint8_t { SK_EARTH, SK_SUN, SK_AURORA, SK_GIANT, SK_REDSUN, SK_RINGED, SK_TWINSUN, SK_MOONS };
enum Tile : uint8_t { TL_STONES, TL_BRICKS, TL_STRATA, TL_CRYSTAL };
// pal: SKY0, SKY1, FAR, MID, NEAR, GNDD, GND, GNDL, ACC
// a world, as STARLIFT.DAT holds it (tools/make_card.py writes it, byte for byte)
struct Planet {
    uint8_t pal[9][3];                                  // SKY0, SKY1, FAR, MID, NEAR, GNDD, GND, GNDL, ACC
    uint8_t grav, theme, weather, sky, style[3], tile, air, clouds, crack;   // grav: 1/1024 px a frame squared (64 = 1 G)
    uint8_t food, shopc, basec, fly, crawl, plants[3], wind, base, coll, worms;
    uint8_t alien[2][4];                                // each sector's alien colours: the saucers' dome, hull and shade, and the creatures' bodies     // the takeaway's food and colour, the base's colour, the wildlife, wind 0-3, the base's and the aliens' designs
    char name[12], place[20], shop[12], ads[3][14];
};
enum State : uint8_t { TITLE, INTRO, PLAY, CLEAR, OVER, WIN, PAUSED, HELP, OPTIONS };
enum ManSt : uint8_t { M_NONE, M_IDLE, M_RUN, M_ABOARD, M_BEAM, M_JET, M_WALK, M_SAVED, M_LOST, M_HELD, M_DROP };   // HELD: in a UFO; DROP: falling into its bucket; JET: jet-packing down
enum ManDo : uint8_t { D_STAND, D_LOOK, D_GAZE, D_CHAT, D_WANDER, D_WAVE, D_CHEER, D_TREK, D_TRIP };   // TREK: (gone); TRIP: flat on its face for a moment
struct Man { int16_t x, y; uint8_t st, act, t, home; int8_t face, vy; uint8_t by, look; };   // y in px (1/4 px when JET or DROP); by: the UFO holding it, + 1; look: suit colour and kit
enum EnKind : uint8_t { E_NONE, E_UFO, E_STINGER, E_MINE, E_TURRET, E_WORM, E_MOTHER };
struct Enemy { int16_t x, y; int8_t vx, vy; uint8_t kind, hp, st, t, target, hit, cargo; };   // x, y in 1/16 px; cargo: colonists aboard a UFO
struct Shot { int16_t x, y; int8_t dir; uint8_t life, kind; };                         // px; kind 0 the laser, 1 the rotary cannon
struct EShot { int16_t x, y; int8_t vx, vy; uint8_t life, kind; };                     // 1/16 px
struct Part { int16_t x, y; int8_t vx, vy; uint8_t life, col; };                       // 1/16 px
struct SaveData { int32_t hi, score; uint8_t stage, lives, musicV, fxV, diff; };   // volumes 0-4; difficulty 0 easy, 1 normal, 2 hard
