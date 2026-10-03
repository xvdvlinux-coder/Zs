module test::delimiters_logic;

fn evaluar(a: bool, b: bool, x: i32, y: i32) -> bool {
    val condicion = (a && !b) || (x <= y && x != 0);
    val bits = (x & y) | (x ^ y) << 2 >> 1;
    return condicion;
}
