fn main() -> i32 {
    var x: i32 = 10;
    raw {
        x = 99;
    }
    return x;
}
