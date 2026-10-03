// Test 50: Preservación de registros y estabilidad de ABI en llamadas complejas
fn sub_calculo(a: i32, b: i32) -> i32 or i32 {
    if (b == 0) {
        fail 9;
    }
    return a * b + 3;
}

fn calcular_complejo(x: i32, y: i32) -> i32 {
    var acc = 0;
    {
        defer { acc = acc + 10; }
        val v1 = sub_calculo(x, y) or 100;
        val v2 = sub_calculo(x, 0) or 7;
        acc = v1 + v2;
    }
    val v3 = sub_calculo(2, 3) catch(err) { return 0; };
    return acc + v3;
}

fn test_main() -> i32 {
    return calcular_complejo(4, 5);
}
