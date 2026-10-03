fn main() -> i32 {
    anchor buf: i32[10] = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
    val start = buf.start;
    val p: loc<i32> = ((start as i64) + (8 as i64)) as loc<i32>;
    val v = @p;
    return v;
}
