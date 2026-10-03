    .arch   armv8-a
    .text
.Lzs_trap:
    brk     #0x42

    .globl  main
    .type   main, %function
main:
    stp     x29, x30, [sp, #-96]!
    mov     x29, sp
    mov     x0, #0
    strb    w0, [x29, #16]
    mov     x0, #0
    strb    w0, [x29, #17]
    mov     x0, #0
    strb    w0, [x29, #18]
    mov     x0, #0
    strb    w0, [x29, #19]
    mov     x0, #0
    strb    w0, [x29, #20]
    mov     x0, #0
    strb    w0, [x29, #21]
    mov     x0, #0
    strb    w0, [x29, #22]
    mov     x0, #0
    strb    w0, [x29, #23]
    mov     x0, #0
    strb    w0, [x29, #24]
    mov     x0, #0
    strb    w0, [x29, #25]
    mov     x0, #0
    strb    w0, [x29, #26]
    mov     x0, #0
    strb    w0, [x29, #27]
    mov     x0, #0
    strb    w0, [x29, #28]
    mov     x0, #0
    strb    w0, [x29, #29]
    mov     x0, #0
    strb    w0, [x29, #30]
    mov     x0, #0
    strb    w0, [x29, #31]
    mov     x0, #0
    strb    w0, [x29, #32]
    mov     x0, #0
    strb    w0, [x29, #33]
    mov     x0, #0
    strb    w0, [x29, #34]
    mov     x0, #0
    strb    w0, [x29, #35]
    mov     x0, #0
    strb    w0, [x29, #36]
    mov     x0, #0
    strb    w0, [x29, #37]
    mov     x0, #0
    strb    w0, [x29, #38]
    mov     x0, #0
    strb    w0, [x29, #39]
    mov     x0, #0
    strb    w0, [x29, #40]
    mov     x0, #0
    strb    w0, [x29, #41]
    mov     x0, #0
    strb    w0, [x29, #42]
    mov     x0, #0
    strb    w0, [x29, #43]
    mov     x0, #0
    strb    w0, [x29, #44]
    mov     x0, #0
    strb    w0, [x29, #45]
    mov     x0, #0
    strb    w0, [x29, #46]
    mov     x0, #0
    strb    w0, [x29, #47]
    add     x0, x29, #16
    mov     x1, x0
    add     x2, x0, #32
    str     x0, [x29, #48]
    str     x1, [x29, #56]
    str     x2, [x29, #64]
    ldr     x0, [x29, #48]
    ldr     x1, [x29, #56]
    ldr     x2, [x29, #64]
    str     x0, [sp, #-16]!
    mov     x0, #32
    str     x0, [sp, #-16]!
    ldr     x1, [sp], #16
    ldr     x0, [sp], #16
    bl      std_readline
    str     x0, [x29, #72]
    ldr     x0, [x29, #48]
    ldr     x1, [x29, #56]
    ldr     x2, [x29, #64]
    str     x0, [sp, #-16]!
    ldr     x0, [x29, #72]
    str     x0, [sp, #-16]!
    ldr     x1, [sp], #16
    ldr     x0, [sp], #16
    bl      std_parse_i64
    str     x0, [x29, #80]
    ldr     x0, [x29, #80]
    str     x0, [x29, #88]
    ldr     x0, [x29, #88]
    str     x0, [sp, #-16]!
    ldr     x0, [sp], #16
    b       .Lmain_epilogue
.Lmain_epilogue:
    adds    xzr, xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #96
    ret
.Lmain_fail_epilogue:
    cmp     xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #96
    ret
