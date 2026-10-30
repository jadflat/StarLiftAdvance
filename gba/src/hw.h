// GBA registers, just the ones this port touches.
#pragma once
#include <stdint.h>
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_DISPSTAT  REG16(0x04000004)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG0CNT    REG16(0x04000008)
#define REG_BG2CNT    REG16(0x0400000C)
#define REG_BG2PA     REG16(0x04000020)
#define REG_BG2PB     REG16(0x04000022)
#define REG_BG2PC     REG16(0x04000024)
#define REG_BG2PD     REG16(0x04000026)
#define REG_BG2X      REG32(0x04000028)
#define REG_BG2Y      REG32(0x0400002C)
#define REG_BG3CNT    REG16(0x0400000E)
#define REG_BG3PA     REG16(0x04000030)
#define REG_BG3PB     REG16(0x04000032)
#define REG_BG3PC     REG16(0x04000034)
#define REG_BG3PD     REG16(0x04000036)
#define REG_BG3X      REG32(0x04000038)
#define REG_BG3Y      REG32(0x0400003C)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_WIN0H     REG16(0x04000040)
#define REG_WIN0V     REG16(0x04000044)
#define REG_WININ     REG16(0x04000048)
#define REG_WINOUT    REG16(0x0400004A)
#define REG_SOUNDCNT_L REG16(0x04000080)
#define REG_SOUNDCNT_H REG16(0x04000082)
#define REG_SOUNDCNT_X REG16(0x04000084)
#define REG_TM0CNT_L  REG16(0x04000100)
#define REG_TM0CNT_H  REG16(0x04000102)
#define REG_TM1CNT_L  REG16(0x04000104)
#define REG_TM1CNT_H  REG16(0x04000106)
#define REG_TM2CNT_L  REG16(0x04000108)
#define REG_TM2CNT_H  REG16(0x0400010A)
#define REG_TM3CNT_L  REG16(0x0400010C)
#define REG_TM3CNT_H  REG16(0x0400010E)
#define REG_DMA0SAD   REG32(0x040000B0)
#define REG_DMA0DAD   REG32(0x040000B4)
#define REG_DMA0CNT_L REG16(0x040000B8)
#define REG_DMA0CNT_H REG16(0x040000BA)
#define REG_MOSAIC    REG16(0x0400004C)
#define REG_BLDY      REG16(0x04000054)
#define REG_DMA1SAD   REG32(0x040000BC)
#define REG_DMA1DAD   REG32(0x040000C0)
#define REG_DMA1CNT_L REG16(0x040000C4)
#define REG_DMA1CNT_H REG16(0x040000C6)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG_IE        REG16(0x04000200)
#define REG_IF        REG16(0x04000202)
#define REG_WAITCNT   REG16(0x04000204)
#define REG_IME       REG16(0x04000208)
#define BIOS_IF       REG16(0x03007FF8)
#define IRQ_VECTOR    REG32(0x03007FFC)
#define PAL_BG        ((volatile uint16_t *)0x05000000)
#define PAL_OBJ       ((volatile uint16_t *)0x05000200)
#define OAM           ((volatile uint16_t *)0x07000000)
#define VRAM_PAGE(p)  ((volatile uint32_t *)(0x06000000 + (p) * 0xA000))
#define SRAM          ((volatile uint8_t *)0x0E000000)
#define IWRAM_TEXT __attribute__((section(".iwram.text"), target("arm"), noinline))

// Halt the CPU until an enabled interrupt (BIOS call 2). ARM and Thumb encode the call number differently.
static inline void gba_halt() {
#ifdef __thumb__
    asm volatile("swi 2" ::: "r0", "r1", "r2", "r3", "memory");
#else
    asm volatile("swi 0x20000" ::: "r0", "r1", "r2", "r3", "memory");
#endif
}
static inline uint32_t irq_off() { uint32_t o = REG_IME; REG_IME = 0; return o; }
static inline void irq_restore(uint32_t o) { REG_IME = (uint16_t)o; }

extern volatile uint8_t gba_flipPending;       // gfx_gba.cpp: a finished frame waits for the next VBlank
void gba_vblank();                             // gfx_gba.cpp: flip the page, load the palette
void gba_psgTick();                            // audio.cpp: the effects on the PSG, called from the 1 kHz tick
void gba_audioVblank();                        // audio.cpp: restart the sample DMA, mix the next buffer
void gba_audioInit();
