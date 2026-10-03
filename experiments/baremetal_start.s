    .arch   armv8-a
    .text
    .globl  _start
_start:
    // 1. Inicializar SP en DRAM (0x40080000)
    ldr     x0, =0x40080000
    mov     sp, x0
    mov     x29, #0
    mov     x30, #0

    // 2. Llamar a test_main compilado desde Zs
    bl      test_main

    // 3. Apagar QEMU limpiamente vía PSCI system_off (0x84000008)
    ldr     x0, =0x84000008
    hvc     #0

.Lhang:
    wfe
    b       .Lhang
