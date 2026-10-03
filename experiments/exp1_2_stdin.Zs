foreign "C" {
    fn std_readline(buf: loc<u8>, max_len: i32) -> i32;
    fn std_parse_i64(buf: loc<u8>, len: i32) -> i32;
}

fn main() -> i64 {
    anchor input_buf: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    val p = input_buf.start;
    val n = std_readline(p, 32);
    val x: i32 = std_parse_i64(p, n);
    val y: u32 = x as u32;
    val z: i64 = y as i64;
    return z;
}
