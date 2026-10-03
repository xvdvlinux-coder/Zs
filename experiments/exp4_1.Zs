fn main() -> i32 {
    anchor datos: i32[10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    var sum: i32 = 0;
    var i: i32 = 0;
    while (i < 5) {
        val p = datos.start ~> i;
        sum = sum + *p;
        i = i + 1;
    }
    return sum;
}
