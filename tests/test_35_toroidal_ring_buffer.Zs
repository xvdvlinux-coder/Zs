module test::toroidal_ring_buffer;

fn test_main(): i32 {
    anchor a: i32[4] = [1, 2, 3, 4];
    val p = a.start;
    val q = p %~> 5;
    return *q;
}
