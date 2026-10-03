// Test 47: defer con orden de ejecución LIFO (3 defers)
fn test_main() -> i32 {
    anchor buf: i32[1] = [0];
    val p = buf.start;
    {
        defer { *p = *p + 10; }
        defer { *p = *p * 2; }
        defer { *p = *p + 5; }
        *p = 2;
    }
    // LIFO:
    // 3ro: 2 + 5 = 7
    // 2do: 7 * 2 = 14
    // 1ro: 14 + 10 = 24
    return buf[0];
}
