module test::bwd_displacement;

fn test_main(): i32 {
    anchor a: i32[8] = [10, 20, 30, 40, 50, 60, 70, 80];
    val p = a.start ~> 5;
    val q = p <~ 2;
    return *q;
}
