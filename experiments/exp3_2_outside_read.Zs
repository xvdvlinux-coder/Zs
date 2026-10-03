fn main() -> i32 {
    val p: loc<u32> = 0x09000000 as loc<u32>;
    val v = @p;
    return 0;
}
