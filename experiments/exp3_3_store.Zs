fn main() -> i32 {
    raw {
        val p: loc<u32> = 0x09000000 as loc<u32>;
        @p = 65;
    }
    return 0;
}
