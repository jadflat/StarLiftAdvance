// STARLIFT's sound: a small software synth. The music is three FM voices (a lead, a bass and an arpeggio) and a
// noise-and-sine drum voice, read from step sequences (Music.h); a lead can glide and waver like a theremin. The
// lander's engine is a rumble of filtered noise, and the sound effects are one more voice: a square, a wobbling sine
// (ray guns, saucers) or a burst of noise (blasts), playing the game's step tables. Everything is mixed in a timer
// interrupt and sent to the piezo as PWM (see Synth.cpp), so music, engine and effects sound together.
#pragma once
#include <CHGame.h>

namespace synth {

enum : uint8_t { WOBBLE = 0x40, NOISE = 0x80 };   // effect flags (with the priority and SOFT): a warbling sine, a noise burst

void begin();                           // device: starts the sample interrupt (the simulator is silent)
void music(int song);                   // see Music.h for the numbers; -1 stops
void engine(uint8_t level);             // the engine rumble, 0 (off) to 255
void volume(uint8_t music, uint8_t effects);   // 0 (off) to 4 (full) each
// The songs are on the SD card: when wanted() >= 0, the game reads that song (Music.h: SONG_OFF, SONG_LEN) into
// buffer() after gfx_wait() and calls loaded(); the song then fades in.
int wanted();
uint8_t *buffer();
void loaded();
void sfx(const audio::Effect &e);       // an effect from the game's table; a higher or equal priority one replaces it

}  // namespace synth
