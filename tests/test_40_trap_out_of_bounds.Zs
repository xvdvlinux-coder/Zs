module test::trap_out_of_bounds;

fn test_main(): i32 {
    anchor a: i32[4] = [1, 2, 3, 4];
    val p = a.start;
    val q = p ~> 100;
    return *q;
}
