// GBA: shadows chgame/RamFunc.h. RAMFUNC code is hot: Thumb, run from IWRAM (32-bit bus, no wait states).
#pragma once
#define CHGAME_RAMFUNC(name)     __attribute__((section(".iwram.text." #name), noinline))   // (by name: gba.ld keeps some in ROM)
#define CHGAME_APP_RAMFUNC(name) __attribute__((section(".iwram.text"), noinline))
#ifndef RAMFUNC
#define RAMFUNC(name) CHGAME_APP_RAMFUNC(name)
#endif
