module test::contracts;

pub contract Lector {
    fn leer(buf: view<u8>) -> usize;
    fn cerrar();
}
