module test::walk_iteration;

fn test_main(): i32 {
    anchor a: i32[4] = [10, 20, 30, 40];
    var sum = 0;
    walk x in a {
        sum = sum + x;
    }
    return sum;
}
