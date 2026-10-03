module test::phase_alignment_check;

fn salto_desalineado(p: loc<i64>) {
    // Para i64, alignof(i64) = 8 bytes
    // 5 bytes no es múltiplo de 8 -> ERROR de congruencia de fase en compilación
    val q = p ~># 5;
}
