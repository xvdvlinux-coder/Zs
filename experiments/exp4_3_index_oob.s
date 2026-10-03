    .arch   armv8-a
    .text
.Lzs_trap:
    brk     #0x42

    .globl  main
    .type   main, %function
main:
    stp     x29, x30, [sp, #-48]!
    mov     x29, sp
    mov     x0, #10
    str     w0, [x29, #16]
    mov     x0, #20
    str     w0, [x29, #20]
    mov     x0, #30
    str     w0, [x29, #24]
    mov     x0, #40
    str     w0, [x29, #28]
    add     x0, x29, #16
    mov     x1, x0
    add     x2, x0, #16
    stp     x0, x1, [sp, #-16]!
    mov     x0, #10
    mov     x3, x0
    ldp     x0, x1, [sp], #16
    cmp     x3, #4
    b.hs    .Lzs_trap
    mov     x4, #4
    mul     x5, x3, x4
    add     x0, x0, x5
    ldrsw   x0, [x0]
    str     x0, [x29, #32]
    ldr     x0, [x29, #32]
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
