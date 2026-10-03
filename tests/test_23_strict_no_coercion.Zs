module test::strict_no_coercion;

fn operacion_invalida(a: u32, b: u64) -> u64 {
    val res = a + b; // ERROR: no existe coerción implícita entre u32 y u64
    return res;
}
