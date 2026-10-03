module test::view_slicing;

fn test_main(): i32 {
    anchor a: i32[8] = [10, 20, 30, 40, 50, 60, 70, 80];
    val sub = a[2 .. 5];
    return sub[1];
}
