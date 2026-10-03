module math_mod;

pub fn potencia_dos(n: i32) -> i32 {
    var res = 1;
    var i = 0;
    while (i < n) {
        res = res * 2;
        i = i + 1;
    }
    return res;
}

pub fn sumar(a: i32, b: i32) -> i32 {
    return a + b;
}
