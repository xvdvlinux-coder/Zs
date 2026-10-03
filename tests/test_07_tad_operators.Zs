module test::tad_operators;

// Operadores exclusivos del modelo TAD
// Desplazamiento adelante: ~>
// Desplazamiento atrás: <~
// Navegación segura: ?~>
// Aritmética cíclica: %~>
// Salto en bytes físicos: ~>#
// Distancia métrica: <=>

fn probar_operadores(p: loc<i32>, q: loc<i32>) {
    val fwd = p ~> 2;
    val bwd = q <~ 1;
    val safe = p ?~> 5 or p;
    val ring = p %~> 1;
    val bytes = p ~># 8;
    val dist = q <=> p;
}
