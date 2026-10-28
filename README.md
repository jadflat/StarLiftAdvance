# Star Lift GBA FS

STARLIFT, the lander-rescue game made for the CHGame handheld, redrawn for the whole Game Boy Advance screen:
240x160, pixel for pixel, nothing stretched. Version **FS 0.8**, from the CHGame version 0.24 (merged in with its bosses,
chrome title, cut-scenes and balance changes).

The CHGame version (128x128) stays in its own repository; this one has its own copy of the game in `game/` and the GBA
back end in `gba/`.

## What is different on the GBA

- **The whole screen.** The game draws 240x160 itself: a wider view of each world, the world lower on the screen, a
  taller HUD with a full-width radar, and more sky. Menus and panels keep their layout, centred.
- **A smooth sky.** The sky's dithered bands are replaced by a gradient the GBA paints itself, a colour each screen line
  (colour 0, the sky's top colour, is the one the hardware shows through; an HBlank DMA changes the backdrop per line).
- **See-through tractor beams.** The beam is drawn into hardware sprites that the GBA blends over the scene.
- **Fades** in from black on the title, each stage card and the game over; the picture **breaks into blocks** for a
  moment when the lander is lost (the GBA's mosaic).
- **Shoulder buttons.** L puts the landing gear up or down in one press; R works as UP (hold it to charge the shot).
  The original controls all still work.
- **More sparks**: twice the particles, bigger explosions.
- **A soft echo** on the music and effects (125 ms, about a quarter level). `GBA_ECHO=0` builds without it.
- Scenery pictures come straight from the cartridge (the "SD card" file is in the ROM), so none ever waits a frame.
- **The ground on its own hardware layer**: drawn once as it scrolls into view (the hardware scrolls it), with detail the
  picture never had time for: pebbles in the soil, lit on top with a shadow under.

## Controls

D-pad left/right: tilt. A: main engine. B: cannon and laser. Hold UP (or R): charge shot. DOWN twice (or L): landing
gear. START: pause menu (hold 3 s: start over).

Remapping: OPTIONS > CONTROLS on the title screen, or hold L + R + SELECT at any time, for the CONTROLS screen. Any control (the eight buttons, the gear and the
charge shot) can go on any GBA key: UP/DOWN to choose, A then the new key, B or DONE to leave. Kept in the save RAM.

## Building

    python gba/build.py          ->  gba/out/STARLIFT.gba

zig is the only compiler needed (the game, the vendored CHGame library and the GBA back end are compiled with zig c++
for the ARM7TDMI). `build.py` copies `game/`, regenerates its card file (`game/tools/make_card.py`), applies
`gba/patches/*.patch` (GBA-only speed-ups, each checked to apply exactly once) and links the ROM; STARLIFT.DAT is
appended to it. See `gba/README.md` for the back end and its build notes.

Switches (environment): `GBA_CFLAGS="-DSL_START=n"` starts new games at stage n; `GBA_SKETCH_THUMB=1` builds the game
code as Thumb; `GBA_SCENECHECK=1` draws the scenery and every picture both the game's way and the patched way each
frame and counts differences (`gba/tools/frames.py` reports them).

## Tested

In mGBA (nightly), on a fixed 3000-frame scripted run (`gba/tools/frames.py`: title, stages, crashes, game over and back)
and a 1500-frame run starting on each of the 8 worlds: no crash. A check build compared the game's own scenery and
picture drawing with the patched versions (no difference in 12,000 scenery and 44,000 picture draws, FS 0.2), and the
ground layer with the picture's own ground pixel for pixel (no difference in 3000 frames, FS 0.3; the pebbles are new
since).

**Speed: not 30 fps yet.** The median frame takes about 38 ms (about 26 fps); the game counts frames, so it plays a
little slower than on the CHGame board. A stage's first frame (the game builds the stage) takes about 230 ms. What
costs: a 240x160 picture is 2.3 times the board's pixels, and it has to live in the GBA's slower 16-bit memory (EWRAM:
the fast 32 KB is full) and be copied to video memory every frame (3.7 ms). Moving the ground to a hardware layer saved
about 2 ms; the near hills on a layer of their own saved nothing (clearing the picture under them cost as much) and
video memory has no room for more layers next to a double-buffered full-screen picture. Getting to 30 fps needs a
different design: the scenery and sprites drawn by the GBA's own layers and sprites instead of a software picture.

Not tried: a real GBA. The MiSTer GBA core: FS builds up to 0.1 were played there; 0.2-0.4 have not been tried yet.
The sound (including the echo) has not been listened to; its levels were checked (no clipping).

## Credits and licences

Game, art and music by Jadflat (MIT, see LICENSE). Built on CHGame by Kevin Bates (Apache-2.0, `game/chgame/`,
see its LICENSE and NOTICE). See `game/THIRD-PARTY.md` for the sound output's credit.
