// Test 55: Rutina de punto de entrada autónomo _start (sin libc ni runtime externo)
fn test_main() -> i32 {
    // Retorno directo que alimentará sys_exit_group (syscall 94)
    return 55;
}
