module test::pratt_precedence;

fn calcular(a: i32, b: i32, c: i32, p: loc<i32>) -> i32 {
    // a + b * c debe agrupar como a + (b * c)
    // p ~> a + b debe agrupar como p ~> (a + b) o (p ~> a) + b según precedencia TAD
    val res1 = a + b * c;
    val res2 = (a + b) * c;
    val q = p ~> a + b;
    return res1;
}
