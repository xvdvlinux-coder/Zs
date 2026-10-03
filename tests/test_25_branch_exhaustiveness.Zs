module test::branch_exhaustiveness;

choice Estado {
    Activo;
    Inactivo;
    Pendiente;
}

fn verificar_estado(e: Estado) {
    // ERROR: no exhaustivo porque omite 'Pendiente' y no incluye 'else'
    branch e {
        Estado::Activo => return;
        Estado::Inactivo => return;
    }
}
