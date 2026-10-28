@ GBA start-up: cartridge header, stacks, copy IWRAM code/data from ROM, clear .bss (IWRAM and EWRAM), run constructors, main().
    .section .header, "ax"
    .arm
    .global _start
_start:
    b   start                   @ 0x00: the entry point
    .space 0xBC, 0              @ 0x04..0xBF: logo, title, codes ... filled in by build.py
    .section .text.start, "ax"
    .arm
    .global start
start:
    mov r0, #0x12               @ IRQ mode
    msr cpsr_c, r0
    ldr sp, =0x03007FA0
    mov r0, #0x1F               @ system mode
    msr cpsr_c, r0
    ldr sp, =0x03007C00              @ system stack: 0x03007000-0x03007C00 (data ends by 0x03007000); IRQ stack above
    ldr r0, =0x04000200         @ IME off while we set up
    mov r1, #0
    strh r1, [r0, #8]
    ldr r0, =_iwram_lma
    ldr r1, =_iwram_start
    ldr r2, =_iwram_end
1:  cmp r1, r2
    ldrcc r3, [r0], #4
    strcc r3, [r1], #4
    bcc 1b
    ldr r0, =_ewtext_lma         @ the EWRAM code (save_gba.cpp)
    ldr r1, =_ewtext_start
    ldr r2, =_ewtext_end
6:  cmp r1, r2
    ldrcc r3, [r0], #4
    strcc r3, [r1], #4
    bcc 6b
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    mov r2, #0
2:  cmp r0, r1
    strcc r2, [r0], #4
    bcc 2b
    ldr r0, =_ewram_start       @ EWRAM's .bss too: it is not cleared at power-on on hardware (mGBA clears it)
    ldr r1, =_ewram_end
5:  cmp r0, r1
    strcc r2, [r0], #4
    bcc 5b
    ldr r4, =__init_start
    ldr r5, =__init_end
3:  cmp r4, r5
    bcs 4f
    ldr r0, [r4], #4
    mov lr, pc
    bx  r0
    b   3b
4:  ldr r0, =gba_main
    bx  r0
    .pool
