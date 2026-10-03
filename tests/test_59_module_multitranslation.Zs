// Test 59: Funcionalidad modular multiplataforma (module + use + cross-linking de .o)
module main_mod;
use math_mod;

foreign "C" {
    fn potencia_dos(n: i32) -> i32;
    fn sumar(a: i32, b: i32) -> i32;
}

fn test_main() -> i32 {
    val p = potencia_dos(5); // 32
    val s = sumar(p, 27);    // 32 + 27 = 59
    return s;
}
