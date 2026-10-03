// Test 48: defer garantizado en ruta de fallo (fail)
fn operacion_critica(p: loc<i32>) -> i32 or i32 {
    defer {
        *p = 88;
    }
    fail 13;
}

fn test_main() -> i32 {
    anchor buf: i32[1] = [0];
    val p = buf.start;
    val err = operacion_critica(p) catch(e) {
        if (buf[0] == 88) {
            return e + 1;
        }
        return 0;
    };
    return err;
}
