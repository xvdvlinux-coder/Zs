// Test 46: defer en salida de función (modificación de estado)
fn modificar_estado(coord: loc<i32>) -> i32 {
    defer {
        *coord = 50;
    }
    return 100;
}

fn test_main() -> i32 {
    anchor buf: i32[1] = [0];
    val p = buf.start;
    val ret_val = modificar_estado(p);
    if (ret_val == 100) {
        return buf[0];
    }
    return 0;
}
