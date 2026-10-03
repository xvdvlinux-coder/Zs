module test::contract_conformance;

pub contract Procesador {
    fn procesar(dato: i32) -> i32;
}

pub fn procesar(dato: i32) -> i32 {
    return dato * 2;
}
