// Test 45: Manejo estructurado con catch(err) { ... }
fn fallar() -> i32 or i32 {
    fail 10;
}

fn test_main() -> i32 {
    val x = fallar() catch(err) {
        return err * 2;
    };
    return x;
}
