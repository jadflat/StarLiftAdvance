# Changelog

## 0.24 (2026-10-28)

- A new title: tall slanted chrome letters (white and silver over a horizon line, gold to red below) with a steel edge,
  an orbit ring in the world's accent colour sweeping behind, and sparkles.
- The cut-scenes are unhurried: the speeder and the lob a quarter slower again, and when the bag is in (or the feast is
  over) the camera rests on the lander for a moment and play comes back with three seconds of safety (the lander blinks).
- New moments: a colonist running for the lander now and then trips flat on its face, then gets up and runs on; a cow
  being beamed up now and then kicks itself free and tumbles down; saucers kept waiting at the drive-through lose their
  patience and honk; a stinger shot down now and then drops a fuel cell.
- Board, autopilot: Verda in a storm 25-29 ms a frame on average, Zorg 24-27 and 23-25 (worst frames 31-34 ms).
- Room for it: the spark bursts share one helper (216 B).

## 0.23 (2026-10-28)

- A gentler start, on every difficulty: stage 1 needs 2, 3 or 4 of its 8 colonists (Easy, Normal, Hard; was 2, 4, 5),
  has no stingers (only a saucer and slow mines), one enemy at a time, the first after 20 s (was 15) and the next ones a
  quarter further apart.
- A little easier throughout: quotas 5% lower on every stage, one enemy fewer at once on Normal and Hard as the stages go
  on, and new enemies spaced out a little more until the last stages (the same by stage 16).

## 0.22 (2026-10-28)

- Fixed: the game could freeze in the delivery cut-scene, the speeder hovering over the lander and turning left and
  right for ever. It chose which side of the lander to come to every frame, from a target that itself depended on that
  side; picked up close to the lander it never settled. It now chooses once, when it has the bag.

## 0.21 (2026-10-28)

- Brighter menus: the title and pause menus' items in white, the chosen one in gold (they were steel blue, hard to read);
  the options screen the same.
- Board, autopilot: stage 15 (Prism) 22-26 ms a frame on average, stage 16 (Zorg, the Dreadnought) 23-25 ms, worst
  31 ms (one 34 ms frame on 15). The new card reader works on a real FAT32 card.

## 0.20 (2026-10-28)

- The four bosses redrawn bigger (45 x 20, from 37 x 14) and in far more detail: shaded from the top left and outlined
  (tools/bosses.py builds the lit shapes, then the detail is added by hand). The Mothership: a glass dome with a glint,
  a band of lit windows, running lights, hull seams, a glowing tractor emitter. The Hive: a wet, veined dome, four gold
  eyes, fangs, tendrils. The Prism warship: a faceted crystal blade with an accent spine, wing lights, fins of light and a
  burning gem of a core. The Dreadnought: a skull-fronted battleship with burning eye sockets, teeth, side guns and
  glowing gun ports. Their hit area and health bar follow the new size; each still fits its stage's card block.

## 0.19 (2026-10-28)

- Four bosses instead of one mothership, one on each boss stage, each with its own picture, call-out and ways: the
  Mothership (Verda, saucers), the Hive (Zorg, stingers), the Prism warship (Verda again, mines and three-way fire) and
  the Dreadnought (Zorg again, the last stage: faster fire, saucers and stingers in turn).
- Boss music: SHOWDOWN (E phrygian, 132 bpm, the theremin over a driving bass) on every boss stage.
- Stage ratings: up to three stars on the stage-clear screen (quota made, everyone home, no lander lost), 500 each.
- Cows aboard: once the milk run has filled the tank, the cow climbs in (it takes a seat); bring it to the base for 1,000.
  The stage tally counts the cows brought home and the ones still grazing.
- Room for all this: STARLIFT finds its file on the card with its own small reader (FAT16/FAT32, the root folder)
  instead of CHSd's general one (268 B), and the sound effects are one packed table (164 B).
- Not done: a high-score table with initials (about 540 B; it does not fit without dropping something else).

## 0.18 (2026-10-27)

- Longer cut-scenes: the speeder rides at half its 0.17 speed (a delivery runs about twice as long), and the feast lasts
  about 10 seconds (its clock at half speed), fireworks still every half second.

## 0.17 (2026-10-27)

- The takeaway delivery is a little cut-scene: once the bag lands, everything else holds still, black bars close in and
  the camera follows the speeder, now half as fast, out to the bag, up to the lander and the lob into the lid.
- The feast too: bars, the camera on the party, longer (about 7 seconds), fireworks every half second, a slower dance.
- The help page's last tip is the takeaway (bump or shoot a saucer as its order comes up).
- To make room: the ledge camps (pillars of rock with double points) are gone.

## 0.16 (2026-10-27)

- Smoother late stages: each stage now draws three of its world's six buildings (the other stage the other three), packed
  into two SD-card blocks, and the mothership comes from the stage's own pictures; a frame now needs about as many card
  blocks as the cache holds. Simulator, autopilot, play only: card reads per frame on stage 15 (Prism) ~0.3 and stage
  16 (Zorg) ~0.5, from about one or more.
- The takeaway: a saucer's order now takes four times as long to come up, and a single bump knocks it loose (the
  speeder comes for it, as when the saucer is shot down).
- Bumps are gentler: a bump from below pushes the lander sideways only (no more being slammed into the ground), from
  above it bounces up.
- To make room: the who's-who card's mothership row (the stage opens with its call-out), two flashing texts steady.

## 0.15 (2026-10-27)

- Smoother on busy worlds: the pictures drawn every frame now share three SD-card blocks, the card cache has a seventh
  slot, and the banner text is read from the card once instead of every frame. Board, autopilot: Zorg's slow stretch
  went from 31-33 ms a frame to 28-30, Vulcan's 77 ms hitch is gone (worst 32 ms), Verda's storm 25-27 ms.
- A milk run: land beside a grazing cow and the tank tops up.
- The feast: two colonists dance either side of the food, between the pad and the base.
- Mothership saucers capped at five at a time.
- To make room: the on-screen wind arrow (the windsocks and the weather show the wind), the who's-who card's wildlife
  row, and fewer sparks and weather drops at once (32 and 48).

## 0.14 (2026-10-27)

- Saucers beam grazing cows back up; shoot the saucer before the cow reaches it to save it (300).
- Stingers go for the takeaway speeder while it carries the bag: a hit brings it down, the bag falls and another
  speeder sets out.
- Each world's food does something of its own at the feast: an extra lander, an engine upgrade or a mended hull.
- The stage-clear screen counts the feasts and the cows; every cow still grazing is worth 250.
- Colonists who come round a camp a second time stand on its other side (they no longer stand on each other).
- The README's GIF and text brought up to date; the trailer linked.
- To make room: one save page instead of two (a power cut in the middle of a save loses that save); the colonists'
  dashes between camps and the camp-chain bonus are gone.

## 0.13 (2026-10-27)

- The aliens dress differently on every world and in each of its two sectors: the saucers' domes, hulls and lights, and
  the stingers', mines', turrets' and mothership's colours (two schemes a world, in its record on the SD card).
- Colonists camp in groups of 2, 3 and 4 (pairs on the high ledges), 8 px apart.
- To make room: the bullseye bonus is gone (it also went off when hopping on the start pad), a saucer's shoot-down has
  one blast fewer, and the palette maps share one helper.

## 0.12 (2026-10-27)

- Shoot down a saucer while its takeaway order is coming up and you get a bonus (500 a sector), and the bag drops: a
  speeder bike from the takeaway races out, its rider grabs the bag, flies up and tosses it into the lander's flip-top
  lid ("ORDER UP!").
- Bring the food home to the base pad for a FEAST FIESTA: 2,000 a sector, fireworks over the pad, the feast at the door.
- Bump a saucer and now and then (1 in 5) a cow tumbles out ("HOLY COW!"). It lands, moos, and wanders the stage
  grazing for the rest of it.
- To make room: every sprite now comes from the SD card; explosions lose their dotted shock rings; a few small trims.

## 0.11 (2026-10-27)

- On the ground, LEFT and RIGHT turn the guns round (they kept the way you last flew).

## 0.10 (2026-10-27)

- The charge shot moves to UP (B is the guns alone again) and charges in about half a second.
- Let go and three plasma balls fly: forwards, backwards and straight down.
- The help pages list HOLD UP for the charge shot and the hull for each difficulty.

## 0.9 (2026-10-27)

- Heavier: gravity 2.75 times each world's (was 2.5).
- Less sideways push: tilting adds a quarter (was a half) again to the sideways thrust.
- Weaker engine, now matched to each world: 104 + 1.25 x the world's gravity (was 142 everywhere), so it is weaker
  against gravity on every world but a full load can still climb on the heaviest; passengers weigh a little less.

## 0.8 (2026-10-27)

- A charge shot: hold B (the guns keep firing) and sparks gather at the nose until it glows; let go for a plasma ball that
  goes through everything in its path, four laser hits to each.
- Better guns: the laser fires nearly twice as often and reaches further; every cannon round now counts.
- Easier landings: more speed, tilt and slope allowed; a heavy (not wild) landing costs a pip of hull instead of the lander.
- The gear folds up by itself once you are clear of the ground after take-off; with the gear down the lander turns more
  finely and the wind no longer drags it more.
- Longer safety after a hit, the low-fuel alarm at a third of a tank, an extra lander every 10,000 points, slower enemy
  fire on Easy (half speed) and Normal (three quarters), and on Easy the hull mends itself (a pip about every 20 s).
- A new, gentler title tune, MOONGLOW (G major, 84 bpm, a mellow FM lead over a held bass).
- To make room: the saucers and some colonist poses are drawn from the SD card; explosion rings are dotted.

## 0.7 (2026-10-27)

- More hull: 8 hits on Easy, 5 on Normal, 3 on Hard (was 5/3/2).
- Bumping a UFO no longer costs hull: the two ships clang and bounce apart.
- Easy: three extra shield-repair kits along every stage.

## 0.6 (2026-10-27)

- The sandworm redrawn so it can be seen: steel-grey swaying rings, a red-gold maw with teeth, a taller mound with a dark crest.

## 0.5 (2026-10-27)

- Difficulty: Easy, Normal or Hard, on the title menu and the pause menu (enemies, fire, hull, fuel, landing, quota).
- A who's-who card before each stage: what it holds and what to do about each (rescue, shoot, avoid, lasers only, harmless).
- To make room: the rocket-borne signs and the Moon's meteors are gone; a simpler windsock.

## 0.4 (2026-10-27)

- Heavier: two and a half times the worlds' gravity, a faster fall, a stronger engine to match.
- Above the screen the engine fades out within a third of a screen: you pop up and drop back, no floating.
- Play runs a fifth faster (an extra game step every fifth frame).

## 0.3 (2026-10-27)

- Stronger sideways thrust; enemies arrive further apart (time for a rescue); slower early UFOs.
- The version shows on the title screen (each build from here gets the next number).

## 0.2 (2026-10-27)

- A heavier lander (twice the gravity, a stronger, quicker engine), and no ceiling: fly up and out of the screen.
- Easier: fewer enemies early, gentler landings, a lower quota, more fuel.
- New: a bullseye bonus for landing dead centre on a pad; chains of camps in one trip; colonists who dash between
  camps (escort them); camps on high ledges worth double; a mothership every fourth stage (only lasers hurt it;
  shoot it open to free everyone it took); after stage 16 the worlds come round again, harder.
- Runs at 30 fps on the board (22-28 ms a frame in storms); a pleasant FM title tune; one small font throughout.

## 0.1 (2026-10-27)

- First version: a Newtonian rocket lander (spooling engine, landing gear, rotary cannon and laser), colonists to land
  beside and carry to the base pad, which refuels and repairs; a quota a stage, pick-ups (repairs, fuel, supplies,
  engine parts).
- Eight worlds over sixteen stages, each with its own palette, three parallax layers of scenery (ruins, bases or alien
  technology), sky, weather and wind, wildlife, plants, adverts and a takeaway; the weather changes from game to game.
- UFOs with tractor beams that carry colonists to their collector; jet packs when one is shot down; stingers, mines,
  turrets, sandworms on the desert world.
- An FM synth: seven songs (a loud title march, theremin and vibraphone songs) and 1950s science-fiction effects.
- Title menu, how to play, options (music and effects volume), pause menu and save, game over screen.
- Music, pictures, worlds and words on the SD card in STARLIFT.DAT.
