fn main() -> i32 {
    anchor datos: i32[4] = [10, 20, 30, 40];
    val p = datos.start ~> 10;
    val x = *p;
    return x;
}
