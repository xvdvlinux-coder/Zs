    .arch   armv8-a
    .text
.Lzs_trap:
    brk     #0x42

    .globl  main
    .type   main, %function
main:
    stp     x29, x30, [sp, #-32]!
    mov     x29, sp
    mov     x0, #0
    str     x0, [x29, #16]
.Lwhile_start_1:
    ldr     x0, [x29, #16]
    str     x0, [sp, #-16]!
    mov     x0, #5
    ldr     x1, [sp], #16
    cmp     x1, x0
    cset    x0, lt
    cbz     x0, .Lwhile_end_2
    ldr     x0, [x29, #16]
    str     x0, [sp, #-16]!
    mov     x0, #1
    ldr     x1, [sp], #16
    add     x0, x1, x0
    str     x0, [x29, #16]
    b       .Lwhile_start_1
.Lwhile_end_2:
    ldr     x0, [x29, #16]
    str     x0, [sp, #-16]!
    ldr     x0, [sp], #16
    b       .Lmain_epilogue
.Lmain_epilogue:
    adds    xzr, xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #32
    ret
.Lmain_fail_epilogue:
    cmp     xzr, xzr
    mov     sp, x29
    ldp     x29, x30, [sp], #32
    ret
