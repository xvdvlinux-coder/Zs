    .arch   armv8-a
    .text

// Punto de entrada autónomo para binarios ELF sin libc (_start)
    .globl  _start
    .type   _start, %function
_start:
    // Linux kernel pone argc en [sp], argv[0] en [sp, #8]
    ldr     x0, [sp]         // argc
    add     x1, sp, #8       // argv

    // Alinear stack frame según AAPCS64
    mov     x29, #0
    mov     x30, #0

    // Ejecutar punto de entrada Zs
    bl      test_main

    // sys_exit_group (syscall 94) con el valor retornado en x0
    mov     x8, #94
    svc     #0
