@ Integer division for the ARM7TDMI (no divide instruction, no libgcc): the EABI helpers the compiler calls.
@ ARM state, in IWRAM. Thumb callers reach them through bx (the build uses -mlong-calls).
    .section .iwram.text, "ax"
    .arm
    .global __aeabi_uidiv
    .global __aeabi_uidivmod
    .global __aeabi_idiv
    .global __aeabi_idivmod
    .type __aeabi_uidiv, %function
    .type __aeabi_uidivmod, %function
    .type __aeabi_idiv, %function
    .type __aeabi_idivmod, %function

@ r0 / r1 -> r0 quotient, r1 remainder (division by zero gives 0xFFFFFFFF and remainder r0)
__aeabi_uidiv:
__aeabi_uidivmod:
    cmp     r1, #0
    beq     3f
    cmp     r0, r1
    movcc   r1, r0              @ n < d: quotient 0, remainder n
    movcc   r0, #0
    bxcc    lr
@ Four quotient bits a pass (the classic ARMv4 loop, as in libgcc's before clz). Its last pass may take too much off
@ the dividend for bits below bit 0 (their quotient bits shift out), so the remainder is worked out as n - q * d.
@ (tools: the algorithm checked against Python's // and % on 400,000 cases, edge cases included.)
    stmfd   sp!, {r4}
    mov     r12, r0             @ n
    mov     r4, r1              @ d
    mov     r3, #1              @ the bit to set in the quotient
    mov     r2, #0              @ the quotient
1:  cmp     r1, #0x10000000     @ align the divisor under the dividend's top bits: 4 at a time,
    cmpcc   r1, r0
    movcc   r1, r1, lsl #4
    movcc   r3, r3, lsl #4
    bcc     1b
1:  cmp     r1, #0x80000000     @ then 1 at a time
    cmpcc   r1, r0
    movcc   r1, r1, lsl #1
    movcc   r3, r3, lsl #1
    bcc     1b
2:  cmp     r0, r1
    subcs   r0, r0, r1
    orrcs   r2, r2, r3
    cmp     r0, r1, lsr #1
    subcs   r0, r0, r1, lsr #1
    orrcs   r2, r2, r3, lsr #1
    cmp     r0, r1, lsr #2
    subcs   r0, r0, r1, lsr #2
    orrcs   r2, r2, r3, lsr #2
    cmp     r0, r1, lsr #3
    subcs   r0, r0, r1, lsr #3
    orrcs   r2, r2, r3, lsr #3
    cmp     r0, #0              @ nothing left: done
    movsne  r3, r3, lsr #4
    movne   r1, r1, lsr #4
    bne     2b
    mul     r1, r2, r4          @ the remainder: n - q * d
    sub     r1, r12, r1
    mov     r0, r2
    ldmfd   sp!, {r4}
    bx      lr
3:  mov     r1, r0
    mvn     r0, #0
    bx      lr

@ signed: the quotient truncates towards zero, the remainder takes the dividend's sign
__aeabi_idiv:
__aeabi_idivmod:
    stmfd   sp!, {r4, r5, lr}
    mov     r4, r0, asr #31            @ the dividend's sign mask
    mov     r12, r1, asr #31           @ the divisor's
    eor     r5, r4, r12                @ the quotient's
    eor     r0, r0, r4
    sub     r0, r0, r4                 @ |n|
    eor     r1, r1, r12
    sub     r1, r1, r12                @ |d|
    bl      __aeabi_uidivmod
    eor     r0, r0, r5
    sub     r0, r0, r5
    eor     r1, r1, r4
    sub     r1, r1, r4
    ldmfd   sp!, {r4, r5, lr}
    bx      lr
