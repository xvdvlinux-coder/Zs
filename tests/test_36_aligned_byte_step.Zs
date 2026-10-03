module test::aligned_byte_step;

fn test_main(): i32 {
    anchor a: i64[4] = [100, 200, 300, 400];
    val p = a.start;
    val q = p ~># 8;
    return *q as i32;
}
