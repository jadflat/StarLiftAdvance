/* GBA link script: code and constants in the cartridge; hot code (RAMFUNC, the synth, the scenery loops) and all data in IWRAM.
   The card file (STARLIFT.DAT) follows the code in ROM; build.py pads the image to _dat_start and appends it.
   Hot functions are named by their -ffunction-sections names, so the game's source stays as it is. */
ENTRY(_start)
MEMORY {
  rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
  iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 28K
  ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}
SECTIONS {
  .header : { KEEP(*(.header)) KEEP(*(.rodata.savetag)) *(.text.start) } > rom
  /* library RAMFUNCs this game seldom calls (never in the pc samples): left in ROM, IWRAM is full */
  .romfuncs : { *(.iwram.text.sprite4 .iwram.text.spriterot .iwram.text.glyph16) } > rom
  /* patches 12 and 13 put the cloud banks and the rain's particle loops in IWRAM; their room: variables the cartridge-run game code uses (particles, men, enemies, gore, booms, the height table, the HUD's line buffer) live in EWRAM (named below) */
  /* (first: the EWRAM variables, before .bss's wildcard below can take them) */
  .ewram (NOLOAD) : { _ewram_start = .; *(.bss._ZN4cardL3bufE) *(.bss._ZL9s_scratch) *(.bss.gba_*) *(.bss._ZN4saveL6mirrorE) *(.bss._ZN12_GLOBAL__N_17songBufE) *(.bss._ZL2pt .bss._ZZL7drawHudvE4line .bss._ZL3hgt .bss._ZL3men .bss._ZL2en .bss._ZL2es .bss._ZL4gore .bss._ZL5booms) *(.ewram .ewram.*) . = ALIGN(4); _ewram_end = .; } > ewram   /* cleared by crt0.S (power-on EWRAM is not zero on hardware) */
  /* IWRAM: data first, at 0x03000000, so the one 256-aligned array (patch 01's slgEv) costs no padding; code after */
  .bss (NOLOAD) : { _bss_start = .; *(.bss._ZL5slgEv) *(.bss .bss.* COMMON) . = ALIGN(4); _bss_end = .; } > iwram
  .iwram : {
    _iwram_start = .;
    *(.iwram.text .iwram.text.*)
    *Synth.o(.text._ZN6blocks6renderEPsi .text._ZN12_GLOBAL__N_17envTickEv .text._ZN12_GLOBAL__N_18nextStepEv .text._ZN5synth6sampleEv)   /* the sample loop; the calls from the game stay in ROM */
    *Synth.o(.rodata .rodata.*)
    *(.text._ZL7colPairPhiiPKhS1_S1_S1_ .text._ZL7colFillPhhhi .text._ZL6colTexPhhiiiPKhS1_ .text._ZL14drawGroundColsv .text._ZL13drawLayerColsi)
    *CHGfx_draw.o(.text._Z9gfx_hlineiiih .text._Z12gfx_fillRectiiiih .text._Z9gfx_vlineiiih .text._Z9gfx_pixeliih)
    *(.text._ZL4hashj .text._ZL6blitBCRK4CSpriibhPKh .text._ZL8groundAti .text._ZL4psetiih)
    *(.text._ZL2vnjiib .text._ZL9layerTopsi .text._ZL6clampiiii)   /* the scenery's column tops: every column when the camera jumps */
    *(.text._ZL11updateShotsv .text._ZL4flagiih .text._ZL10drawPlantsv .text._ZL12bgGroundDrawv .text._ZL4blitRK4CSpriibhPKh)   /* hot game code (pc samples, FS 0.5) */
    *(.data .data.*)
    . = ALIGN(4); _iwram_end = .;
  } > iwram AT> rom
  _iwram_lma = LOADADDR(.iwram);
  /* code that must run from RAM but need not be fast: EWRAM, copied from ROM by crt0.S (after .ewram's variables) */
  .ewram_text : {
    _ewtext_start = .;
    *save_gba.o(.text .text.*)      /* the cartridge bus is busy with ROM fetches: SRAM is only reachable from code in RAM */
    *(.ewtext .ewtext.*)            /* (main.cpp's testBoot, for the same reason) */
    . = ALIGN(4); _ewtext_end = .;
  } > ewram AT> rom
  _ewtext_lma = LOADADDR(.ewram_text);
  .text : { *(.text .text.*) *(.rodata .rodata.*) *(.glue_7*) *(.eh_frame*) . = ALIGN(4); } > rom
  .init_array : { __init_start = .; KEEP(*(.init_array*)) __init_end = .; } > rom
  _dat_start = ALIGN(ADDR(.init_array) + SIZEOF(.init_array), 512);
  /DISCARD/ : { *(.ARM.exidx*) *(.comment) }
}
