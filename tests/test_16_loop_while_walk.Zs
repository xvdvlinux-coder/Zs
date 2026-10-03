module test::loop_while_walk;

fn iteraciones(arr: view<i32>) {
    var i = 0;
    while i < 10 {
        i += 1;
    }

    loop {
        if i >= 20 {
            break;
        }
        i += 1;
    }

    walk item in arr by 2 {
        val x = item;
    }
}
