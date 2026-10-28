// STARLIFT.DAT on the SD card (Card.cpp): the songs and the big set-piece pictures, read through a six-block cache.
// Every call reads the card over SPI1, which the panel shares: call them only between gfx_wait() and gfx_flushAsync().
#pragma once
#include <stdint.h>

namespace card {
bool open();                                       // finds and checks the file; false: no card, no file or the wrong one
bool ok();                                         // the file is open
void frame();                                      // after gfx_wait(), each frame: the scratch slot is stale
extern bool lowPri;                                // while set, at() gives up (nullptr) rather than read a third block this frame
const uint8_t *at(uint32_t off);                   // the byte at off and the rest of its 512 B block (nullptr: the card has gone)
bool copy(uint8_t *dst, uint32_t off, uint16_t n);
}  // namespace card

// A reader of run-length picture data: from flash (fromCard false), or from the card a block at a time
struct Rd {
    const uint8_t *p; uint32_t off; uint16_t left; bool fromCard;
    uint8_t next() {
        if (fromCard && !left) { p = card::at(off); left = (uint16_t)(512 - (off & 511)); if (!p) { left = 0; return 0xFF; } }
        left--; off++;
        return *p++;
    }
};
