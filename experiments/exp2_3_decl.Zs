foreign "C" {
    fn pthread_create(thread: loc<u8>, attr: loc<u8>, start_routine: loc<u8>, arg: loc<u8>) -> i32;
}

fn main() -> i32 {
    return 0;
}
