module test::branch_syntax;

fn evaluar_codigo(cod: i32) -> i32 {
    branch cod {
        0 => return 100;
        1 .. 10 => {
            return 200;
        }
        else => return -1;
    }
}
