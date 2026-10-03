// Test 56: Módulo I/O: std_println
foreign "C" {
    fn std_println(buf: loc<u8>, len: i32);
}

fn test_main() -> i32 {
    anchor msg: u8[7] = [90, 115, 45, 76, 97, 110, 103]; // "Zs-Lang"
    val p = msg.start;
    std_println(p, 7);
    return 56;
}
