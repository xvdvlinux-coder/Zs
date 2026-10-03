// Test 53: Manejo de archivos: sys_openat y sys_close
foreign "C" {
    fn sys_openat(dfd: i32, path: loc<u8>, flags: i32, mode: i32) -> i32;
    fn sys_close(fd: i32) -> i32;
}

fn test_main() -> i32 {
    anchor path: u8[16] = [47, 116, 109, 112, 47, 122, 115, 95, 116, 53, 51, 46, 116, 109, 112, 0];
    val p = path.start;
    val fd = sys_openat(-100, p, 66, 420);
    if (fd > 0) {
        val cl = sys_close(fd);
        if (cl == 0) {
            return 53;
        }
    }
    return 0;
}
