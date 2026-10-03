// Test 57: Módulo de archivos: ciclo completo de escritura y lectura
foreign "C" {
    fn sys_openat(dfd: i32, path: loc<u8>, flags: i32, mode: i32) -> i32;
    fn sys_write(fd: i32, buf: loc<u8>, count: i32) -> i32;
    fn sys_read(fd: i32, buf: loc<u8>, count: i32) -> i32;
    fn sys_close(fd: i32) -> i32;
}

fn test_main() -> i32 {
    anchor path: u8[16] = [47, 116, 109, 112, 47, 122, 115, 95, 116, 53, 55, 46, 116, 109, 112, 0];
    val p_path = path.start;

    // 1. Abrir para escribir: O_CREAT (64) | O_TRUNC (512) | O_WRONLY (1) = 577, mode 0644 (420)
    val fd_w = sys_openat(-100, p_path, 577, 420);
    if (fd_w <= 0) {
        return 0;
    }

    anchor data_out: u8[4] = [65, 66, 67, 68]; // "ABCD"
    val p_out = data_out.start;
    val w_bytes = sys_write(fd_w, p_out, 4);
    sys_close(fd_w);
    if (w_bytes != 4) {
        return 0;
    }

    // 2. Reabrir para leer: O_RDONLY (0)
    val fd_r = sys_openat(-100, p_path, 0, 0);
    if (fd_r <= 0) {
        return 0;
    }

    anchor data_in: u8[4] = [0, 0, 0, 0];
    val p_in = data_in.start;
    val r_bytes = sys_read(fd_r, p_in, 4);
    sys_close(fd_r);

    if (r_bytes == 4) {
        if ((data_in[0] as i32) == 65 && (data_in[1] as i32) == 66 && (data_in[2] as i32) == 67 && (data_in[3] as i32) == 68) {
            return 57;
        }
    }
    return 0;
}
