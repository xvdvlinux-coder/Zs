module test::if_else_expr;

fn clasificar(x: i32) -> i32 {
    if x > 100 {
        return 1;
    } else if x > 0 {
        return 2;
    } else {
        return 0;
    }
}
