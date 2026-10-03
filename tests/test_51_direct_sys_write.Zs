// Test 51: Syscall directo Linux AArch64: sys_write (fd 1, stdout)
foreign "C" {
    fn sys_write(fd: i32, buf: loc<u8>, count: i32) -> i32;
}

fn test_main() -> i32 {
    anchor msg: u8[5] = [72, 101, 108, 108, 111]; // "Hello"
    val p = msg.start;
    val written = sys_write(1, p, 5);
    if (written == 5) {
        return 51;
    }
    return 0;
}
