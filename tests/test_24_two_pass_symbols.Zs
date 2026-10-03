module test::two_pass_symbols;

fn primera_funcion() -> i32 {
    return segunda_funcion(); // Legal: segunda_funcion declarada más abajo
}

fn segunda_funcion() -> i32 {
    return 42;
}
