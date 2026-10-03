module test::coordinate_metric;

fn test_main(): i32 {
    anchor a: i32[10] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    val p = a.start ~> 2;
    val q = a.start ~> 7;
    val dist = q <=> p;
    return dist as i32;
}
