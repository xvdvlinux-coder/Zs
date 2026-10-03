// Test 42: Dual return ABI - fallo (Carry = 1, error en x1)
fn fallar() -> i32 or i32 {
    fail 42;
}

fn test_main() -> i32 {
    val err = fallar() catch(e) {
        return e;
    };
    return 0;
}
