module test::distance_anchor_origin;

fn distancia_invalida() {
    anchor a1: i32[10];
    anchor a2: i32[10];

    val p1 = a1.start;
    val p2 = a2.start;

    // ERROR: p1 y p2 pertenecen a anclajes distintos
    val dist = p1 <=> p2;
}
