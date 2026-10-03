foreign "C" {
    fn pthread_create(thread: loc<u8>, attr: loc<u8>, start_routine: loc<u8>, arg: loc<u8>) -> i32;
}

fn main() -> i32 {
    anchor th: u8[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    val p_th = th.start;
    val res = pthread_create(p_th, p_th, p_th, p_th);
    return res;
}
