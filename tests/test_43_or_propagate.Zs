// Test 43: Propagación de error con 'or return fail'
fn dividir(a: i32, b: i32) -> i32 or i32 {
    if (b == 0) {
        fail 55;
    }
    return a / b;
}

fn paso_intermedio() -> i32 or i32 {
    val x = dividir(10, 0) or return fail;
    return x + 10;
}

fn test_main() -> i32 {
    val res = paso_intermedio() catch(err) {
        return err;
    };
    return 0;
}
