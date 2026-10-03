// Test 49: defer en bloques anidados / salida de ámbito { ... }
fn test_main() -> i32 {
    anchor buf: i32[1] = [0];
    val p = buf.start;
    var i = 0;
    while (i < 3) {
        {
            defer { *p = *p + 5; }
            *p = *p + 1;
        }
        i = i + 1;
    }
    return buf[0];
}
