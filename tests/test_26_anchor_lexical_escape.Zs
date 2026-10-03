module test::anchor_lexical_escape;

fn escapar_coordenada() {
    var p_externo: loc<i32> = 0 as loc<i32>;
    {
        anchor buffer_interno: i32[16];
        val p_interno = buffer_interno.start;
        p_externo = p_interno; // ERROR: p_interno proviene de un anclaje con ámbito más profundo
    }
}
