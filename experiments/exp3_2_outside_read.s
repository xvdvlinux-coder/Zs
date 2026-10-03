    .arch   armv8-a
    .text
.Lzs_trap:
    brk     #0x42

    .globl  main
    .type   main, %function
main:
    stp     x29, x30, [sp, #-48]!
    mov     x29, sp
    movz    x0, #0
    movk    x0, #2304, lsl #16
    str     x0, [x29, #16]
    str     x1, [x29, #24]
    str     x2, [x29, #32]
    ldr     x0, [x29, #16]
    ldr     x1, [x29, #24]
    ldr     x2, [x29, #32]
    cmp     x0, x2
    b.hs    .Lzs_trap
    cmp     x0, x1
    b.lo    .Lzs_trap
    ldrsw   x0, [x0]
    str     x0, [x29, #40]
    mov     x0, #0
    str     x0, [sp, #-16]!
    ldr     x0, [sp], #16
    b       .Lmain_epilogue
.Lmain_epilogue:
    adds    xzr, xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #48
    ret
.Lmain_fail_epilogue:
    cmp     xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #48
    ret
