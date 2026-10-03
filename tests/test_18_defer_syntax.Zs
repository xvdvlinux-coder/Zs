module test::defer_syntax;

fn ejecutar_limpieza() -> i32 {
    defer val clean1 = 1;
    defer {
        val clean2 = 2;
        val clean3 = 3;
    }
    return 42;
}
