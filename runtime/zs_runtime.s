    .arch   armv8-a
    .text

// Syscall directo Linux ARM64: sys_write (syscall 64)
// x0 = fd, x1 = buf, x2 = count
// Retorno: x0 = bytes escritos (Carry = 0) o x1 = errno, x0 = -1 (Carry = 1)
    .globl  sys_write
    .type   sys_write, %function
sys_write:
    mov     x8, #64
    svc     #0
    cmp     x0, #0
    b.ge    .Lwrite_ok
    neg     x1, x0
    mov     x0, #-1
    cmp     xzr, xzr
    ret
.Lwrite_ok:
    adds    xzr, xzr, xzr
    ret

// Syscall directo Linux ARM64: sys_read (syscall 63)
// x0 = fd, x1 = buf, x2 = count
// Retorno: x0 = bytes leídos (Carry = 0) o x1 = errno, x0 = -1 (Carry = 1)
    .globl  sys_read
    .type   sys_read, %function
sys_read:
    mov     x8, #63
    svc     #0
    cmp     x0, #0
    b.ge    .Lread_ok
    neg     x1, x0
    mov     x0, #-1
    cmp     xzr, xzr
    ret
.Lread_ok:
    adds    xzr, xzr, xzr
    ret

// Syscall directo Linux ARM64: sys_openat (syscall 56)
// x0 = dirfd (-100 para AT_FDCWD), x1 = pathname, x2 = flags, x3 = mode
// Retorno: x0 = fd (Carry = 0) o x1 = errno, x0 = -1 (Carry = 1)
    .globl  sys_openat
    .type   sys_openat, %function
sys_openat:
    mov     x8, #56
    svc     #0
    cmp     x0, #0
    b.ge    .Lopenat_ok
    neg     x1, x0
    mov     x0, #-1
    cmp     xzr, xzr
    ret
.Lopenat_ok:
    adds    xzr, xzr, xzr
    ret

// Syscall directo Linux ARM64: sys_close (syscall 57)
// x0 = fd
// Retorno: x0 = 0 (Carry = 0) o x1 = errno, x0 = -1 (Carry = 1)
    .globl  sys_close
    .type   sys_close, %function
sys_close:
    mov     x8, #57
    svc     #0
    cmp     x0, #0
    b.ge    .Lclose_ok
    neg     x1, x0
    mov     x0, #-1
    cmp     xzr, xzr
    ret
.Lclose_ok:
    adds    xzr, xzr, xzr
    ret

// std_print: Escribe una cadena a stdout (fd = 1)
// x0 = buffer pointer, x1 = length
    .globl  std_print
    .type   std_print, %function
std_print:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    mov     x2, x1           // count
    mov     x1, x0           // buf
    mov     x0, #1           // fd 1
    bl      sys_write
    ldp     x29, x30, [sp], #16
    ret

// std_println: Escribe una cadena seguida de salto de línea '\n'
// x0 = buffer pointer, x1 = length
    .globl  std_println
    .type   std_println, %function
std_println:
    stp     x29, x30, [sp, #-32]!
    mov     x29, sp
    bl      std_print
    mov     w2, #10          // '\n'
    strb    w2, [sp, #-16]!
    mov     x0, #1
    mov     x1, sp
    mov     x2, #1
    bl      sys_write
    add     sp, sp, #16
    ldp     x29, x30, [sp], #32
    ret

// std_fmt_i64: Formatea un entero de 64 bits a ASCII decimal en buf
// x0 = valor (i64), x1 = buf (u8*), x2 = max_len
// Retorno: x0 = longitud generada en bytes
    .globl  std_fmt_i64
    .type   std_fmt_i64, %function
std_fmt_i64:
    stp     x29, x30, [sp, #-80]!
    mov     x29, sp
    str     x19, [sp, #16]
    str     x20, [sp, #24]
    str     x21, [sp, #32]
    str     x22, [sp, #40]

    mov     x19, x1          // out_buf
    mov     x20, #0          // out_len = 0

    // Si x0 == 0
    cmp     x0, #0
    b.ne    .Lfmt_nonzero
    mov     w3, #'0'
    strb    w3, [x19]
    mov     x0, #1
    b       .Lfmt_done

.Lfmt_nonzero:
    // Manejo de signo negativo si < 0
    cmp     x0, #0
    b.ge    .Lfmt_positive
    mov     w3, #'-'
    strb    w3, [x19], #1
    add     x20, x20, #1
    neg     x0, x0

.Lfmt_positive:
    // Extraer dígitos en buffer local en stack (offset 48 a 79)
    add     x21, x29, #48    // base del buffer de dígitos
    mov     x22, x21         // puntero de escritura
    mov     x4, #10
.Lfmt_digit_loop:
    udiv    x5, x0, x4       // quotient
    msub    x6, x5, x4, x0   // remainder = x0 - x5*10
    add     w6, w6, #'0'
    strb    w6, [x22], #1
    mov     x0, x5
    cbnz    x0, .Lfmt_digit_loop

    // Copiar dígitos invertidos hacia out_buf
.Lfmt_copy_loop:
    sub     x22, x22, #1
    ldrb    w7, [x22]
    strb    w7, [x19], #1
    add     x20, x20, #1
    cmp     x22, x21
    b.ne    .Lfmt_copy_loop

    mov     x0, x20          // Retornar longitud total escrita

.Lfmt_done:
    strb    wzr, [x19]       // Null terminate
    ldr     x19, [sp, #16]
    ldr     x20, [sp, #24]
    ldr     x21, [sp, #32]
    ldr     x22, [sp, #40]
    ldp     x29, x30, [sp], #80
    ret

// std_readline: Lee una línea de stdin (fd 0) hasta '\n' o EOF
// x0 = buffer pointer (u8*), x1 = max_len
// Retorno: x0 = bytes leídos (sin incluir '\n', null-terminated en buf[n])
    .globl  std_readline
    .type   std_readline, %function
std_readline:
    stp     x29, x30, [sp, #-64]!
    mov     x29, sp
    str     x19, [sp, #16]
    str     x20, [sp, #24]
    str     x21, [sp, #32]
    str     x22, [sp, #40]

    mov     x19, x0          // buf
    mov     x20, x1          // max_len
    mov     x21, #0          // count = 0

.Lreadline_loop:
    sub     x22, x20, #1
    cmp     x21, x22
    b.ge    .Lreadline_done

    // Leer 1 byte de stdin (fd 0)
    mov     x0, #0
    sub     sp, sp, #16
    mov     x1, sp
    mov     x2, #1
    bl      sys_read
    ldrb    w22, [sp], #16

    cmp     x0, #1
    b.ne    .Lreadline_done

    cmp     w22, #10         // '\n'
    b.eq    .Lreadline_done
    cmp     w22, #13         // '\r'
    b.eq    .Lreadline_loop

    strb    w22, [x19, x21]
    add     x21, x21, #1
    b       .Lreadline_loop

.Lreadline_done:
    strb    wzr, [x19, x21]  // Null-terminate
    mov     x0, x21          // Retornar longitud

    ldr     x19, [sp, #16]
    ldr     x20, [sp, #24]
    ldr     x21, [sp, #32]
    ldr     x22, [sp, #40]
    ldp     x29, x30, [sp], #64
    ret

// std_input: Muestra un prompt interactivo y lee entrada de usuario desde stdin
// x0 = prompt_ptr, x1 = prompt_len, x2 = out_buf, x3 = max_len
// Retorno: x0 = bytes leídos
    .globl  std_input
    .type   std_input, %function
std_input:
    stp     x29, x30, [sp, #-48]!
    mov     x29, sp
    str     x19, [sp, #16]
    str     x20, [sp, #24]

    mov     x19, x2          // out_buf
    mov     x20, x3          // max_len

    // Mostrar prompt si prompt_len > 0
    cmp     x1, #0
    b.le    .Linput_read
    bl      std_print

.Linput_read:
    mov     x0, x19
    mov     x1, x20
    bl      std_readline

    ldr     x19, [sp, #16]
    ldr     x20, [sp, #24]
    ldp     x29, x30, [sp], #48
    ret

// std_parse_i64: Convierte cadena ASCII decimal a entero de 64 bits con signo
// x0 = buf, x1 = len
// Retorno: x0 = entero convertido
    .globl  std_parse_i64
    .type   std_parse_i64, %function
std_parse_i64:
    mov     x2, #0           // result = 0
    mov     x3, #0           // is_neg = 0
    mov     x4, #0           // index = 0

    cmp     x1, #0
    b.eq    .Lparse_done

    ldrb    w5, [x0, x4]
    cmp     w5, #'-'
    b.ne    .Lparse_loop
    mov     x3, #1
    add     x4, x4, #1

.Lparse_loop:
    cmp     x4, x1
    b.ge    .Lparse_done
    ldrb    w5, [x0, x4]
    sub     w6, w5, #'0'
    cmp     w6, #9
    b.hi    .Lparse_done     // Carácter no numérico
    mov     x7, #10
    mul     x2, x2, x7
    add     x2, x2, x6
    add     x4, x4, #1
    b       .Lparse_loop

.Lparse_done:
    cmp     x3, #1
    b.ne    .Lparse_ret
    neg     x2, x2

.Lparse_ret:
    mov     x0, x2
    ret

