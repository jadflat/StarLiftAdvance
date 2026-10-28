// STARLIFT.DAT (Card.h): block 0 holds "STARLIFT" and the build's hash (CardIndex.h, written with the file by
// tools/make_card.py), so a card from another build is refused; then the picture section, then the song section.
// Six 512 B slots (and CHGfx's scratch buffer while a frame is drawn) keep the blocks used last: the pictures on screen are read once and drawn from RAM after that.
#include <CHGame.h>
#include <Fat.h>
#include <SdSpi.h>
#include "Card.h"
#include "CardIndex.h"
#ifdef SL_TRACE
#include <stdio.h>
#endif

namespace card {

enum { SLOTS = 6, ALL = SLOTS + 1 };                // the last slot is CHGfx's scratch buffer, free while a frame is drawn
alignas(4) static uint8_t buf[SLOTS][512];
static uint16_t tag[ALL] = { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };
static uint8_t age[ALL], tick;                    // the least recently used slot is the one to replace
static uint8_t used[ALL], frameNo;               // the frame each slot was last used in
static uint8_t misses;                            // blocks read from the card this frame
bool lowPri;                                      // scenery: it may wait a frame rather than read more than two blocks
static uint8_t *slotBuf(uint8_t i) { return i < SLOTS ? buf[i] : gfx_chunkScratch(); }
static fat::Run runs[4];
static uint8_t nRuns;
static bool open_;

// STARLIFT.DAT's place on the card, found without CHSd's general FAT code (only its block reads): the first FAT16 or FAT32
// volume (at LBA 0, or the MBR's first partition), the file in its root, in up to four runs. A card that is not like
// that simply finds no file; the header check below catches the rest. About half the size of fat::open().
static uint32_t fatLba, dataLba;
static uint8_t shift;
static bool f32;
static uint32_t rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }
static uint32_t clus(uint32_t c) { return dataLba + ((c - 2) << shift); }
static uint32_t nextC(uint32_t c, uint8_t *b) {                 // the cluster after c, 0 at the end (or on a read error)
    const uint32_t off = c << (f32 ? 2 : 1);
    if (!sd::read(fatLba + (off >> 9), b)) return 0;
    const uint32_t n = f32 ? rd32(b + (off & 511)) & 0x0FFFFFFFu : rd16(b + (off & 511));
    return n >= (f32 ? 0x0FFFFFF8u : 0xFFF8u) ? 0 : n;
}
static uint8_t findDat(uint8_t *b) {
    uint32_t base = 0;
    if (!sd::init() || !sd::read(0, b)) return 0;
    if (b[0] != 0xEB && b[0] != 0xE9 && !sd::read(base = rd32(b + 454), b)) return 0;   // an MBR: its first partition
    uint32_t fsz = rd16(b + 22);
    f32 = !fsz;
    if (f32) fsz = rd32(b + 36);
    for (shift = 0; (1u << shift) < b[13]; shift++) {}
    fatLba = base + rd16(b + 14);
    const uint32_t rootSecs = (rd16(b + 17) + 15) >> 4, rootLba = fatLba + b[16] * fsz;
    dataLba = rootLba + rootSecs;
    uint32_t dir = rd32(b + 44), sec = 0;
    for (uint32_t guard = 4096; guard--;) {
        uint32_t lba;
        if (!f32) { if (sec == rootSecs) return 0; lba = rootLba + sec; }
        else { if (sec >> shift) { if ((dir = nextC(dir, b)) < 2) return 0; sec = 0; } lba = clus(dir) + sec; }
        sec++;
        if (!sd::read(lba, b)) return 0;
        for (const uint8_t *d = b; d < b + 512; d += 32) {
            if (!d[0]) return 0;
            if (d[0] == 0xE5 || (d[11] & 0x1A) || memcmp(d, "STARLIFTDAT", 11)) continue;
            uint32_t c = rd16(d + 26) | (f32 ? rd16(d + 20) << 16 : 0), left = (rd32(d + 28) + 511) >> 9;
            uint8_t n = 0;
            while (left) {
                if (c < 2) return 0;
                const uint32_t at = clus(c), k = left < (1u << shift) ? left : (1u << shift);
                if (n && runs[n - 1].lba + runs[n - 1].blocks == at) runs[n - 1].blocks += k;
                else { if (n == 4) return 0; runs[n].lba = at; runs[n].blocks = k; n++; }
                left -= k;
                if (left) c = nextC(c, b);
            }
            return n;
        }
    }
    return 0;
}
static bool readBlk(uint32_t k, uint8_t *dst) {
    for (uint8_t i = 0; i < nRuns; i++) { if (k < runs[i].blocks) return sd::read(runs[i].lba + k, dst); k -= runs[i].blocks; }
    return false;
}

#ifdef CHGBA
extern "C" const uint8_t _dat_start[];          // Star Lift GBA FS: STARLIFT.DAT is in the cartridge ROM (gba/src/fat_rom.cpp)
extern "C" const uint32_t gba_datBlocks;
bool open() {                                   // the GBA: no card to search, only the file's header to check
    open_ = gba_datBlocks && !memcmp(_dat_start, "STARLIFT", 8) && !memcmp(_dat_start + 8, &CARD_HASH, 4);
    return open_;
}
#else
bool open() {
    for (uint8_t i = 0; i < ALL; i++) tag[i] = 0xffff;
    nRuns = findDat(buf[0]);
    open_ = nRuns && readBlk(0, buf[0]) && !memcmp(buf[0], "STARLIFT", 8) && !memcmp(buf[0] + 8, &CARD_HASH, 4);
    return open_;
}
#endif

bool ok() { return open_; }

void frame() { tag[SLOTS] = 0xffff; frameNo++; misses = 0; }              // the scratch buffer went out with the last flush

const uint8_t *at(uint32_t off) {
    if (!open_) return nullptr;
#ifdef CHGBA
    // on the GBA every block is in ROM already: no cache to fill and no reads to ration, so nothing waits a frame (with
    // the wider screen's extra pictures the 5-slot cache and lowPri budget made scenery blink in and out)
    if ((off >> 9) >= gba_datBlocks) return nullptr;
    return _dat_start + off;
#endif
    const uint16_t blk = (uint16_t)(off >> 9);
#ifdef SL_TRACE
    if ((frameNo & 63) == 7) fprintf(stderr, "use f%u b%u lp%d\n", (unsigned)frameNo, (unsigned)blk, (int)lowPri);
#endif
    for (uint8_t i = 0; i < ALL; i++) if (tag[i] == blk) { age[i] = ++tick; used[i] = frameNo; return slotBuf(i) + (off & 511); }
    // the slot to replace: the least recently used of those not used this frame (a frame that needs more blocks than there
    // are slots reads the extra ones through the scratch slot, so the others stay put and only the extra ones miss)
    uint8_t v = SLOTS;
    for (uint8_t i = 0; i < SLOTS; i++)
        if (used[i] != frameNo && (v == SLOTS || (uint8_t)(tick - age[i]) > (uint8_t)(tick - age[v]))) v = i;
    if (lowPri && misses >= 2) return nullptr;    // the scenery can show up a frame later; a frame cannot be late
    misses++;
#ifdef SL_TRACE
    fprintf(stderr, "miss f%u b%u lp%d\n", (unsigned)frameNo, (unsigned)blk, (int)lowPri);
#endif
    tag[v] = 0xffff;
    if (!readBlk(blk, slotBuf(v))) { open_ = false; return nullptr; }
    tag[v] = blk;
    age[v] = ++tick;
    used[v] = frameNo;
    return slotBuf(v) + (off & 511);
}

bool copy(uint8_t *dst, uint32_t off, uint16_t n) {
    while (n) {
        const uint8_t *s = at(off);
        if (!s) return false;
        uint16_t k = (uint16_t)(512 - (off & 511));
        if (k > n) k = n;
        memcpy(dst, s, k);
        dst += k; off += k; n -= k;
    }
    return true;
}

}  // namespace card
