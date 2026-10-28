// Saving: see Save.h. The record is
//   u32 magic; u8 version, flag; u16 seq; data; (pad to 4) u32 crc
// with the CRC (CRC-32, reflected) over everything before it.
#include "Save.h"
#include <Arduino.h>
#include <CHGfx.h>
#include <string.h>
#include "RamFunc.h"

namespace save {

static const uint32_t PAGE = 256;
static const uint32_t PAGE_A = 0xF500, PAGE_B = 0xF600;   // the metadata page is 0xF700

struct Header { uint32_t magic; uint8_t version, flag; uint16_t seq; };

static uint16_t lastSeq = 0;
static bool broken = false;

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static uint32_t crcAt(uint16_t size) { return (uint32_t)(sizeof(Header) + size + 3) & ~3u; }

#ifndef CHSIM
extern "C" uint32_t _data_lma, _data_vma, _edata;

static uint32_t imageEnd() {
    return (uint32_t)&_data_lma + ((uint32_t)&_edata - (uint32_t)&_data_vma);
}

// The flash controller, as the bootloader drives it (platform/bootloader).
// Must run from SRAM, with interrupts off (the vector table is in flash).
#define CR_STRT 0x00000040u
#define CR_FLOCK 0x00008000u
#define CR_PAGE_PG 0x00010000u
#define CR_PAGE_ER 0x00020000u
#define CR_BUF_LOAD 0x00040000u
#define CR_BUF_RST 0x00080000u
#define SR_BSY 0x00000001u
#define PROG(a) ((a) + 0x08000000u)

CHGAME_RAMFUNC(save) static void pageWrite(uint32_t addr, const uint32_t *w) {
    uint32_t irq;
    __asm volatile("csrr %0, 0x800" : "=r"(irq));
    __asm volatile("csrw 0x800, %0" : : "r"(irq & ~0x88u));
    FLASH->KEYR = 0x45670123u; FLASH->KEYR = 0xCDEF89ABu;
    FLASH->MODEKEYR = 0x45670123u; FLASH->MODEKEYR = 0xCDEF89ABu;
    FLASH->CTLR |= CR_PAGE_ER;
    FLASH->ADDR = PROG(addr);
    FLASH->CTLR |= CR_STRT;
    while (FLASH->STATR & SR_BSY) {}
    FLASH->CTLR &= ~CR_PAGE_ER;
    FLASH->CTLR |= CR_PAGE_PG;
    FLASH->CTLR |= CR_BUF_RST;
    while (FLASH->STATR & SR_BSY) {}
    FLASH->CTLR &= ~CR_PAGE_PG;
    for (uint32_t i = 0; i < PAGE / 4; i++) {
        FLASH->CTLR |= CR_PAGE_PG;
        *(volatile uint32_t *)(PROG(addr) + i * 4) = w[i];
        FLASH->CTLR |= CR_BUF_LOAD;
        while (FLASH->STATR & SR_BSY) {}
        FLASH->CTLR &= ~CR_PAGE_PG;
    }
    FLASH->CTLR |= CR_PAGE_PG;
    FLASH->ADDR = PROG(addr);
    FLASH->CTLR |= CR_STRT;
    while (FLASH->STATR & SR_BSY) {}
    FLASH->CTLR &= ~CR_PAGE_PG;
    FLASH->CTLR |= CR_FLOCK;
    __asm volatile("csrw 0x800, %0" : : "r"(irq));
}

// Two pages when the image leaves room for them, else just the last one.
static bool twoPages() { return imageEnd() <= PAGE_A; }
bool available() { return !broken && imageEnd() <= PAGE_B; }

static const uint8_t *page(uint32_t a) { return (const uint8_t *)a; }

static bool writePage(uint32_t addr, const uint8_t *buf) {
    pageWrite(addr, (const uint32_t *)buf);
    return memcmp((const void *)addr, buf, PAGE) == 0;
}
#else
// The simulator: in-memory pages, so save/continue flows can be scripted.
static uint8_t simFlash[2][PAGE];
bool available() { return !broken; }
static bool twoPages() { return true; }
static const uint8_t *page(uint32_t a) { return simFlash[a == PAGE_B]; }
static bool writePage(uint32_t addr, const uint8_t *buf) {
    memcpy(simFlash[addr == PAGE_B], buf, PAGE);
    return true;
}
#endif

static bool valid(const uint8_t *p, uint32_t magic, uint8_t version, uint16_t size) {
    const Header *h = (const Header *)p;
    uint32_t at = crcAt(size);                  // (word-aligned: pages are)
    return h->magic == magic && h->version == version && *(const uint32_t *)(p + at) == crc32(p, at);
}

const void *read(uint32_t magic, uint8_t version, uint16_t size, uint8_t *flag) {
    if (!available() || size > MAX_DATA) return nullptr;
    const uint8_t *a = page(PAGE_A), *b = page(PAGE_B);
    bool va = twoPages() && valid(a, magic, version, size), vb = valid(b, magic, version, size);
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
    uint32_t at = crcAt(size);
    *(uint32_t *)(buf + at) = crc32(buf, at);
    uint32_t addr = ((h->seq & 1) || !twoPages()) ? PAGE_B : PAGE_A;   // take turns
    if (!writePage(addr, buf)) { broken = true; return false; }
    lastSeq = h->seq;
    return true;
}

}  // namespace save
