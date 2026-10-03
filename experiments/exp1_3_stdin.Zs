foreign "C" {
    fn std_readline(buf: loc<u8>, max_len: i32) -> i32;
    fn std_parse_i64(buf: loc<u8>, len: i32) -> i64;
}

fn main() -> i32 {
    anchor input_buf: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    val p = input_buf.start;
    val n = std_readline(p, 32);
    val x: u64 = std_parse_i64(p, n) as u64;
    val y: i8 = x as i8;
    return y as i32;
}
