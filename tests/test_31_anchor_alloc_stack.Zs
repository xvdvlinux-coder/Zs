module test::anchor_alloc_stack;

fn test_main(): i32 {
    anchor a: i32[4] = [10, 20, 30, 40];
    val p = a.start;
    return *p;
}
