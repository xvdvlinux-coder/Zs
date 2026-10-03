fn main() -> i32 {
    anchor datos: i32[4] = [10, 20, 30, 40];
    var guard: i32 = 77;
    val p = datos.start ~> 4;
    *p = 999;
    return guard;
}
