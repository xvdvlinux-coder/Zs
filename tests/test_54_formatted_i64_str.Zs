// Test 54: Entrada y salida formateada: std_fmt_i64
foreign "C" {
    fn std_fmt_i64(num: i32, buf: loc<u8>, max_len: i32) -> i32;
}

fn test_main() -> i32 {
    anchor buf: u8[16] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    val p = buf.start;
    val len = std_fmt_i64(12345, p, 16);
    if (len == 5) {
        if ((buf[0] as i32) == 49 && (buf[1] as i32) == 50 && (buf[2] as i32) == 51 && (buf[3] as i32) == 52 && (buf[4] as i32) == 53) {
            return 54;
        }
    }
    return 0;
}
