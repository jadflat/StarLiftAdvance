// STARLIFT's sound (Synth.h): a small software synth mixed in the sample interrupt and sent to the piezo as PWM.
//
// The hardware setup follows chgame-soundtracks (ESPboy-edu/ESPboy_CHCore, LIBS/chgame-soundtracks, BSD 3-Clause: see THIRD-PARTY.md).
// TIM1 CH2 on PB10 is an 8-bit PWM carrier (187.5 kHz), and TIM3's update interrupt writes one sample to it SYN_RATE
// times a second. The music is a step sequencer over Music.h's streams: three 2-operator FM voices (a modulator at a
// whole-number ratio of the carrier, with its own decaying index) and one drum voice (a sine that drops for the kick,
// noise for the snare and hats). An instrument can glide between notes and waver (vibrato): with a low index that is
// a theremin. The engine is low-passed noise; the effects voice is a square, a warbling sine or a noise burst.
#include "Synth.h"
#include "Music.h"
#if defined(__riscv) && !defined(CHSIM)
#include <Arduino.h>
#include <chgame/RamFunc.h>
#define SYN_DEVICE 1
#else
#define SYN_DEVICE 0
#define RAMFUNC(name)
#endif

namespace {

#ifndef SYN_GAIN
#define SYN_GAIN 17                                       // 1.06 x: the theremin holds its notes, so the mix is denser than plucked songs
#endif
constexpr uint16_t ENV_DIV = SYN_RATE / 122;            // samples between envelope ticks (about 8 ms)

struct Voice { uint16_t ph, mph, inc, minc, tinc, binc; uint8_t idx, eff, vol; bool gate; };   // tinc: the note; binc: the glide toward it
Voice v[3];
uint8_t lfo;                                             // the vibrato's phase (about 5 Hz)

struct Drum { uint16_t ph, inc, lfsr; uint8_t vol, kind, decay; };      // kind 0 kick, 1 snare, 2 hat
Drum dr = { 0, 0, 0xACE1, 0, 0, 0 };

uint8_t musV = 4, fxV = 4;                               // the volumes, 0-4
uint8_t engT, engV;                                      // the engine's target and its level now
uint16_t engLfsr = 0x1D3B;
int16_t engLp;

struct Sfx { const audio::Step *st; uint8_t n, i, ticks, tot, prio, amp, amp0, kind, wph; bool soft; uint16_t inc, ph, base; int16_t dinc; int8_t nz; };
Sfx sf = {};                                             // kind 0 square, 1 wobble, 2 noise

alignas(4) uint8_t songBuf[SONG_MAX];                  // the song playing, loaded from the card (Music.h has the layout)
const uint8_t *cur = nullptr;
const Inst *ins = (const Inst *)songBuf;
volatile int8_t needId = -1;                             // a song the game should load into songBuf (the card is its to read)
uint8_t sweepNow = 0;
int8_t curId = -1, pendingId = -2;
uint8_t fade = 255;
int8_t fadeDir = 0;
uint8_t pos = 0;
uint16_t stepCnt = 0, stepLen = 0, envCnt = 0;

inline uint16_t noteInc(uint8_t n) {
    const int o = n / 12 - 5;                           // C4 is octave 0
    const uint32_t b = NOTE_INC16[n % 12];
    return (uint16_t)(o >= 0 ? (b << o) >> 4 : b >> (4 - o));
}

void noteOn(int i, uint8_t n) {
    Voice &c = v[i];
    const Inst &in = ins[i];
    c.tinc = noteInc(n);
    if (!in.glide || !c.vol) c.binc = c.tinc;          // a gliding voice slides from the note it was on
    c.inc = c.binc;
    c.minc = (uint16_t)(c.inc * in.ratio);
    c.mph = 0;
    c.idx = in.idx0;
    c.eff = in.idx0;
    if (!in.glide || !c.vol || !c.gate) c.vol = in.amp; // a legato theremin note does not re-attack
    c.gate = true;
}

void drumOn(uint8_t bits) {
    if (bits & 1) { dr.kind = 0; dr.inc = 1310; dr.vol = 160; dr.decay = 14; }
    else if (bits & 2) { dr.kind = 1; dr.inc = 1556; dr.vol = 130; dr.decay = 18; }
    else if (bits & 8) { dr.kind = 2; dr.vol = 70; dr.decay = 7; }
    else if (bits & 4) { dr.kind = 2; dr.vol = 64; dr.decay = 26; }
}

void silence() {
    for (int i = 0; i < 3; i++) { v[i].vol = 0; v[i].gate = false; }
    dr.vol = 0; sweepNow = 0;
}

void startSong(int song) {                               // songBuf holds it
    silence();
    cur = songBuf;
    curId = (int8_t)song;
    stepLen = (uint16_t)(cur[SB_STEP] * SYN_RATE / 1000);
    stepCnt = 1;
    pos = 0;
}

void nextStep() {
    auto at = [](int stream, uint8_t p) { return cur[SB_BARS + cur[stream + (p >> 4)] * 16 + (p & 15)]; };   // a step of a stream of the song's bars
    for (int i = 0; i < 3; i++) {
        const uint8_t b = i == 2 ? at(SB_LEAD, (uint8_t)(pos - cur[SB_DELAY])) : at(i ? SB_BASS : SB_LEAD, pos);      // the arp is the lead, delayed
        if (b == 0) v[i].gate = false;
        else if (b > 1) noteOn(i, i == 2 ? (uint8_t)(b + (int8_t)cur[SB_SHIFT]) : b);
    }
    if (const uint8_t dm = at(SB_DRUMS, pos)) drumOn(dm);
    const uint8_t t = pos & 63, tri = t < 32 ? t : 63 - t;                   // four bars: the sweep opens and closes
    sweepNow = (uint8_t)((tri * cur[SB_SWEEP]) >> 5);
    pos++;
}

void sfxStep() {
    if (sf.i >= sf.n) { sf.n = 0; sf.amp = 0; sf.prio = 0; return; }
    const audio::Step &s = sf.st[sf.i];
    const uint16_t inc = (uint16_t)(s.hz * 164u);       // 20 Hz units to a phase increment at SYN_RATE
    sf.ticks = sf.tot = (uint8_t)((s.ms * 62u >> 8) + 1);
    sf.dinc = s.endHz ? (int16_t)(((int)s.endHz * 164 - (int)inc) / sf.ticks) : 0;
    sf.inc = sf.base = inc;
    sf.amp0 = s.hz ? (sf.soft ? 44 : sf.kind == 1 ? 120 : 100) : 0;
    sf.amp = sf.amp0;
}

void envTick() {
    if (fadeDir < 0) {
        if (fade > 4) fade -= 4;
        else {
            fade = 0;
            if (pendingId != -2) { silence(); cur = nullptr; curId = -1; needId = pendingId; pendingId = -2; }   // the game loads the next one
            fadeDir = 0;
        }
    } else if (fadeDir > 0) {
        if (fade < 251) fade += 4; else { fade = 255; fadeDir = 0; }
    }
    lfo += 11;
    for (int i = 0; i < 3; i++) {
        Voice &c = v[i];
        const Inst &in = ins[i];
        if (!c.vol) continue;
        if (in.glide) c.binc = (uint16_t)(c.binc + (((int32_t)c.tinc - c.binc) * in.glide >> 5));
        int32_t f = c.binc;
        if (in.vib) f += (f * SIN((uint8_t)(lfo)) * in.vib) >> 14;                     // the waver
        c.inc = (uint16_t)f;
        c.minc = (uint16_t)(c.inc * in.ratio);
        if (c.idx > in.idxFloor + in.idxDecay) c.idx -= in.idxDecay; else c.idx = in.idxFloor;
        c.eff = (i == 1) ? c.idx : (uint8_t)(c.idx + sweepNow > 255 ? 255 : c.idx + sweepNow);
        if (c.gate) {
            if (c.vol > in.volFloor + in.volDecay) c.vol -= in.volDecay; else c.vol = in.volFloor;
        } else {
            c.vol = c.vol > in.release ? c.vol - in.release : 0;
        }
    }
    if (dr.vol) {
        if (dr.kind == 0 && dr.inc > 360) dr.inc -= dr.inc >> 2;
        dr.vol = dr.vol > dr.decay ? dr.vol - dr.decay : 0;
    }
    if (engV < engT) engV = engT - engV > 24 ? engV + 24 : engT;           // the engine spools up fast and down slower
    else if (engV > engT) engV = engV - engT > 10 ? engV - 10 : engT;
    if (sf.n) {
        sf.base += sf.dinc;
        if (sf.kind == 1) { sf.wph += 46; sf.inc = (uint16_t)(sf.base + ((int32_t)sf.base * SIN((uint8_t)(sf.wph)) >> 9)); }   // a fast warble, about 22 Hz
        else sf.inc = sf.base;
        if (sf.kind == 2) sf.amp = (uint8_t)(sf.amp0 * sf.ticks / sf.tot);   // a burst dies away
        if (--sf.ticks == 0) { sf.i++; sfxStep(); }
    }
}

}  // namespace

namespace synth {

uint8_t sample() {                                       // runs from flash: the RAM for a RAM function is not there
    if (++envCnt >= ENV_DIV) { envCnt = 0; envTick(); }
    if (cur && --stepCnt == 0) { stepCnt = stepLen; nextStep(); }
    int mix = 0;
    for (int i = 0; i < 3; i++) {
        Voice &c = v[i];
        if (!c.vol) continue;
        c.ph += c.inc;
        c.mph += c.minc;
        const int m = (SIN((uint8_t)(c.mph >> 8)) * c.eff) >> 5;
        int s = SIN((uint8_t)((uint8_t)((c.ph >> 8) + m)));
        const uint8_t dv = ins[i].drive;
        if (dv > 16) { s = (s * dv) >> 4; if (s > 100) s = 100 + ((s - 100) >> 3); else if (s < -100) s = -100 + ((s + 100) >> 3); }
        mix += (s * c.vol) >> 7;
    }
    if (dr.vol) {
        dr.ph += dr.inc;
        if (dr.kind) {
            uint16_t x = dr.lfsr;
            x ^= x << 7; x ^= x >> 9; x ^= x << 8;
            dr.lfsr = x;
            const int nz = (int)(x >> 8) - 128;
            mix += (nz * dr.vol) >> 7;
            if (dr.kind == 1) mix += (SIN((uint8_t)(dr.ph >> 8)) * dr.vol) >> 7;
        } else {
            mix += (SIN((uint8_t)(dr.ph >> 8)) * dr.vol) >> 8;
        }
    }
    if (fade < 255) mix = (mix * fade) >> 8;
    mix = (mix * musV) >> 2;
    int fx = 0;
    if (engV) {                                                        // the engine: noise through a one-pole low-pass, a rumble
        uint16_t x = engLfsr;
        x ^= x << 7; x ^= x >> 9; x ^= x << 8;
        engLfsr = x;
        engLp += (int16_t)((((int)(x >> 8) - 128) - engLp) >> 3);
        fx += (engLp * engV) >> 7;
    }
    if (sf.amp) {
        const uint16_t was = sf.ph;
        sf.ph += sf.inc;
        if (sf.kind == 1) fx += (SIN((uint8_t)(sf.ph >> 8)) * sf.amp) >> 7;
        else if (sf.kind == 2) {
            if (sf.ph < was) { uint16_t x = dr.lfsr; x ^= x << 7; x ^= x >> 9; x ^= x << 8; dr.lfsr = x; sf.nz = (int8_t)(x >> 8); }   // a new level each cycle: pitched noise
            fx += (sf.nz * sf.amp) >> 7;
        } else fx += (sf.ph & 0x8000) ? sf.amp : -sf.amp;
    }
    mix += (fx * fxV) >> 2;
    mix = (mix * SYN_GAIN) >> 4;
    if (mix > 96) mix = 96 + ((mix - 96) >> 2); else if (mix < -96) mix = -96 + ((mix + 96) >> 2);
    return (uint8_t)(128 + (mix > 127 ? 127 : mix < -128 ? -128 : mix));
}

#if SYN_DEVICE
#define SYN_LOCK() __disable_irq()
#define SYN_UNLOCK() __enable_irq()
#else
#define SYN_LOCK()
#define SYN_UNLOCK()
#endif

void music(int song) {
    const int id = (song < 0 || song >= SONG_COUNT) ? -1 : song;
    SYN_LOCK();
    if (!cur) {                                          // nothing playing: the game loads it, then it fades in
        needId = (int8_t)id;
        pendingId = -2;
    } else if (id == curId) {
        pendingId = -2;
        if (fadeDir < 0) fadeDir = 1;
    } else {
        pendingId = (int8_t)id;
        fadeDir = -1;
    }
    SYN_UNLOCK();
}

void engine(uint8_t level) { engT = level; }
void volume(uint8_t music, uint8_t effects) { musV = music; fxV = effects; }

int wanted() { return needId; }
uint8_t *buffer() { return songBuf; }
void loaded() {
    SYN_LOCK();
    if (needId >= 0) { startSong(needId); fade = 0; fadeDir = 1; }
    needId = -1;
    SYN_UNLOCK();
}

void sfx(const audio::Effect &e) {
    SYN_LOCK();
    const uint8_t prio = e.flags & 15;
    if (!sf.n || prio >= sf.prio) {
        sf.st = e.steps; sf.n = e.n; sf.i = 0; sf.prio = prio; sf.soft = (e.flags & audio::SOFT) != 0;
        sf.kind = (e.flags & NOISE) ? 2 : (e.flags & WOBBLE) ? 1 : 0;
        sf.ph = 0; sf.wph = 0;
        sfxStep();
    }
    SYN_UNLOCK();
}

}  // namespace synth

#if SYN_DEVICE
extern "C" volatile uint32_t CFGHR_tmpB;
extern "C" void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern "C" void TIM3_IRQHandler(void) {
    TIM3->INTFR = ~1u;
    TIM1->CH2CVR = synth::sample();
}

void synth::begin() {
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB | RCC_APB2Periph_TIM1;
    RCC->APB1PCENR |= RCC_APB1Periph_TIM3;
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(7u << 15)) | (1u << 15);          // TIM1 partial remap: CH2 on PB10
    CFGHR_tmpB = (CFGHR_tmpB & ~(15u << 8)) | (11u << 8);            // alternate-function push-pull
    GPIOB->CFGHR = CFGHR_tmpB;
    TIM1->CTLR1 = 0; TIM1->DMAINTENR = 0; TIM1->CCER = 0;
    TIM1->PSC = 0; TIM1->ATRLR = 255; TIM1->CNT = 0;
    TIM1->CHCTLR1 = 0x6800; TIM1->CH2CVR = 128;
    TIM1->RPTCR = 0; TIM1->BDTR = 0x8000; TIM1->CCER = 0x10;
    TIM1->SWEVGR = 1; TIM1->CTLR1 = 0x81;                            // 187.5 kHz carrier
    TIM3->CTLR1 = 0; TIM3->DMAINTENR = 0;
    TIM3->PSC = 0; TIM3->ATRLR = 48000000u / SYN_RATE - 1; TIM3->CNT = 0;
    TIM3->SWEVGR = 1; TIM3->INTFR = 0; TIM3->DMAINTENR = 1;
    NVIC_SetPriority(TIM3_IRQn, 0);
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CTLR1 = 1;
}
#else
void synth::begin() {}
#endif
