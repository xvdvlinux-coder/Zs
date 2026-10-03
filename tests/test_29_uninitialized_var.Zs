module test::uninitialized_var;

fn leer_no_inicializada() -> i32 {
    val x: i32;
    val y = x + 10; // ERROR: lectura de variable 'x' no inicializada
    return y;
}
