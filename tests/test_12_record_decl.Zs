module test::record_decl;

pub record Cabecera align(8) {
    pub magic: u32;
    pub longitud: u16;
    flags: u16;
    timestamp: u64;
}

fn crear_cabecera() -> Cabecera {
    return Cabecera { magic: 0x5A53, longitud: 64, flags: 0, timestamp: 1000 };
}
