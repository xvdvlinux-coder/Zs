// Test 52: Entrada interactiva de usuario (std_readline y parse desde stdin)
foreign "C" {
    fn std_readline(buf: loc<u8>, max_len: i32) -> i32;
    fn std_parse_i64(buf: loc<u8>, len: i32) -> i32;
}

fn test_main() -> i32 {
    anchor input_buf: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    val p = input_buf.start;
    val n = std_readline(p, 32);
    if (n > 0) {
        val num = std_parse_i64(p, n);
        if (num == 52) {
            return 52;
        }
    }
    return 0;
}
