module test::choice_adt;

pub choice EventoRed {
    Conectado(u32);
    DatosRecibidos(view<u8>);
    Desconectado;
}

fn manejar_evento(e: EventoRed) -> i32 {
    return 0;
}
