// Test 41: Dual return ABI - éxito (Carry = 0, valor en x0)
fn dividir(a: i32, b: i32) -> i32 or i32 {
    if (b == 0) {
        fail 99;
    }
    return a / b;
}

fn test_main() -> i32 {
    val res = dividir(40, 4) or 0;
    return res;
}
