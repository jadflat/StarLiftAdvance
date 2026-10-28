// CHGame on the Game Boy Advance: just enough Arduino for the CHGame and CHGfx libraries (cf. chsim's host/Arduino.h).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define PROGMEM
#define F(s) (s)
#define pgm_read_byte(p)  (*(const uint8_t *)(p))
#define pgm_read_word(p)  (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
#define pgm_read_ptr(p)   (*(void *const *)(p))
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define F_CPU 16777216u
#define LED_BUILTIN 0

typedef bool    boolean;
typedef uint8_t byte;

uint32_t micros();
uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
static inline void pinMode(uint32_t, uint32_t) {}
static inline void digitalWrite(uint32_t, uint32_t) {}
static inline int digitalRead(uint32_t) { return 1; }
long random(long howbig);
long random(long lo, long hi);
void randomSeed(unsigned long s);

template <class T> static inline T constrain(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

class GbaSerial {
public:
    void begin(unsigned long = 0) {}
    bool waitForPC(uint32_t = 0) { return true; }
    explicit operator bool() const { return true; }
    int available() { return 0; }
    int read() { return -1; }
    void flush() {}
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    size_t print(const char *) { return 0; }
    size_t print(char) { return 0; }
    size_t print(int) { return 0; }
    size_t print(unsigned) { return 0; }
    size_t print(long) { return 0; }
    size_t print(unsigned long) { return 0; }
    template <class T> size_t println(T) { return 0; }
    size_t println() { return 0; }
    int printf(const char *, ...) { return 0; }
};
extern GbaSerial Serial;
extern uint32_t sim_ledState;
