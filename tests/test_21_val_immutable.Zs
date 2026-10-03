module test::val_immutable;

fn probar_inmutabilidad() {
    val x = 10;
    x = 20; // ERROR: no se puede reasignar 'val'
}
