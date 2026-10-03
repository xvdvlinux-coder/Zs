    .arch   armv8-a
    .text
.Lzs_trap:
    brk     #0x42

    .globl  main
    .type   main, %function
main:
    stp     x29, x30, [sp, #-112]!
    mov     x29, sp
    mov     x0, #10
    str     w0, [x29, #16]
    mov     x0, #20
    str     w0, [x29, #20]
    mov     x0, #30
    str     w0, [x29, #24]
    mov     x0, #40
    str     w0, [x29, #28]
    mov     x0, #50
    str     w0, [x29, #32]
    mov     x0, #60
    str     w0, [x29, #36]
    mov     x0, #70
    str     w0, [x29, #40]
    mov     x0, #80
    str     w0, [x29, #44]
    mov     x0, #90
    str     w0, [x29, #48]
    mov     x0, #100
    str     w0, [x29, #52]
    add     x0, x29, #16
    mov     x1, x0
    add     x2, x0, #40
    str     x0, [x29, #56]
    str     x1, [x29, #64]
    str     x2, [x29, #72]
    ldr     x0, [x29, #56]
    ldr     x1, [x29, #64]
    ldr     x2, [x29, #72]
    str     x0, [sp, #-16]!
    mov     x0, #8
    ldr     x1, [sp], #16
    add     x0, x1, x0
    str     x0, [x29, #80]
    str     x1, [x29, #88]
    str     x2, [x29, #96]
    ldr     x0, [x29, #80]
    ldr     x1, [x29, #88]
    ldr     x2, [x29, #96]
    cmp     x0, x2
    b.hs    .Lzs_trap
    cmp     x0, x1
    b.lo    .Lzs_trap
    ldrsw   x0, [x0]
    str     x0, [x29, #104]
    ldr     x0, [x29, #104]
    str     x0, [sp, #-16]!
    ldr     x0, [sp], #16
    b       .Lmain_epilogue
.Lmain_epilogue:
    adds    xzr, xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #112
    ret
.Lmain_fail_epilogue:
    cmp     xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #112
    ret
