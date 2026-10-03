module test::var_mutable;

fn probar_mutabilidad() -> i32 {
    var contador = 0;
    contador = 5;
    contador = 10;
    return contador;
}
