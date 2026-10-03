// Test 44: Valor por defecto de error con fallback 'or <expr>'
fn dividir(a: i32, b: i32) -> i32 or i32 {
    if (b == 0) {
        fail 99;
    }
    return a / b;
}

fn test_main() -> i32 {
    val x = dividir(10, 0) or 77;
    return x;
}
