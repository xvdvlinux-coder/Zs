module test::safe_displacement_fallback;

fn test_main(): i32 {
    anchor a: i32[4] = [100, 200, 300, 400];
    val p = a.start;
    val q = (p ?~> 10) or p;
    return *q;
}
