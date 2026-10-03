foreign "C" {
    fn std_readline(buf: loc<u8>, max_len: i32) -> i32;
    fn std_parse_i64(buf: loc<u8>, len: i32) -> i32;
}

fn main() -> i64 {
    anchor input_buf: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    val p = input_buf.start;
    val n = std_readline(p, 32);
    val x: i8 = std_parse_i64(p, n) as i8;
    val y: u64 = x as u64;
    return y as i64;
}
