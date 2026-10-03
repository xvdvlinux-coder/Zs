module test::foreign_raw;

foreign "C" {
    fn puts(cadena: loc<u8>) -> i32;
    fn exit(status: i32);
}

fn usar_raw() {
    raw {
        val x = 1;
    }
}
