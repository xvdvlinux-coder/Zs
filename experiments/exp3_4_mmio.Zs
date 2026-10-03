fn test_main() -> i32 {
    val uart: loc<u8> = 0x09000000 as loc<u8>;
    *uart = 65 as u8;
    return 0;
}

fn main() -> i32 {
    return test_main();
}
