foreign "C" {
    fn sys_write(fd: i32, buf: loc<u8>, count: i32) -> i32;
}

fn main() -> i32 {
    anchor buf: i32[10] = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
    val start = buf.start;
    val addr = (start as i64) + (8 as i64);
    
    // Llamar a una función foreign que clobberea registros x1 y x2
    anchor dummy: u8[4] = [0, 0, 0, 0];
    val _ = sys_write(999, dummy.start, 0); // fallo seguro, altera x1 y x2

    val p: loc<i32> = addr as loc<i32>;
    val v = @p;
    return v;
}
