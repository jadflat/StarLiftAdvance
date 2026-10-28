// Saving on the GBA: CHGame's save:: API (Save.h) with the two 256-byte pages in the cartridge's 32 KB battery SRAM.
// The record is the library's (Save.cpp): u32 magic; u8 version, flag; u16 seq; data; (pad to 4) u32 crc.
// SRAM is an 8-bit bus: pages are copied through byte reads and writes.
#include <chgame/Save.h>
#include <CHGfx.h>
#include <string.h>
#include "hw.h"

namespace save {

static const uint32_t PAGE = 256;
struct Header { uint32_t magic; uint8_t version, flag; uint16_t seq; };

static uint16_t lastSeq = 0;
static bool broken = false;
alignas(4) static uint8_t mirror[2][PAGE];     // the pages as last read

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
static uint32_t crcAt(uint16_t size) { return (uint32_t)(sizeof(Header) + size + 3) & ~3u; }

bool available() { return !broken; }

static bool valid(const uint8_t *p, uint32_t magic, uint8_t version, uint16_t size) {
    const Header *h = (const Header *)p;
    const uint32_t at = crcAt(size);
    return h->magic == magic && h->version == version && *(const uint32_t *)(p + at) == crc32(p, at);
}

const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag) {
    if (!available() || size > MAX_DATA) return nullptr;
    for (int pg = 0; pg < 2; pg++) for (uint32_t i = 0; i < PAGE; i++) mirror[pg][i] = SRAM[pg * PAGE + i];
    const uint8_t *a = mirror[0], *b = mirror[1];
    const bool va = valid(a, magic, version, size), vb = valid(b, magic, version, size);
    const uint8_t *r = nullptr;
    if (va && vb) r = (int16_t)(((const Header *)a)->seq - ((const Header *)b)->seq) > 0 ? a : b;
    else r = va ? a : (vb ? b : nullptr);
    if (!r) return nullptr;
    const Header *h = (const Header *)r;
    lastSeq = h->seq;
    if (flag) *flag = h->flag;
    return r + sizeof(Header);
}

void *buffer() {
    uint8_t *buf = gfx_chunkScratch();          // idle between gfx_wait() and the next flush
    memset(buf, 0, PAGE);
    return buf + sizeof(Header);
}

bool write(uint32_t magic, uint8_t version, uint16_t size, uint8_t flag) {
    if (!available() || size > MAX_DATA) return false;
    uint8_t *buf = gfx_chunkScratch();
    Header *h = (Header *)buf;
    h->magic = magic;
    h->version = version;
    h->flag = flag;
    h->seq = (uint16_t)(lastSeq + 1);
    const uint32_t at = crcAt(size);
    *(uint32_t *)(buf + at) = crc32(buf, at);
    const uint32_t pg = h->seq & 1;             // take turns
    for (uint32_t i = 0; i < PAGE; i++) SRAM[pg * PAGE + i] = buf[i];
    for (uint32_t i = 0; i < PAGE; i++) if (SRAM[pg * PAGE + i] != buf[i]) { broken = true; return false; }
    lastSeq = h->seq;
    return true;
}

}  // namespace save
