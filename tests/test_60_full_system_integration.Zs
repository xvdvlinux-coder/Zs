// Test 60: Integración completa de sistema: TAD + Retorno Dual + Defer + Syscall I/O
foreign "C" {
    fn sys_write(fd: i32, buf: loc<u8>, count: i32) -> i32;
}

fn calcular_seguro(val_in: i32) -> i32 or i32 {
    if (val_in < 0) {
        fail 99;
    }
    return val_in * 2;
}

fn flujo_con_defer(p: loc<i32>) -> i32 {
    defer {
        *p = *p + 20; // 40 + 20 = 60
    }
    return 40;
}

fn test_main() -> i32 {
    // 1. Asignación y navegación topológica TAD
    anchor datos: i32[4] = [10, 20, 30, 40];
    val start_coord = datos.start;
    val fwd_coord = start_coord ~> 2; // apunta a 30
    val bwd_coord = fwd_coord <~ 1;   // apunta a 20
    val dist = fwd_coord <=> start_coord; // 2

    // 2. Navegación con guardia y fallback
    val safe_coord = start_coord ?~> 50 or start_coord;

    // 3. Retorno dual y manejo estructurado de errores
    val err_fallback = calcular_seguro(-5) or 999;
    val v_ok = calcular_seguro(10) or 0; // 20

    // 4. Ejecución LIFO de defer modificando anclaje
    anchor res_slot: i32[1] = [40];
    val p_res = res_slot.start;
    val ret_code = flujo_con_defer(p_res);

    // 5. Syscall directo Linux ARM64
    anchor log_msg: u8[5] = [90, 115, 54, 48, 10]; // "Zs60\n"
    val p_log = log_msg.start;
    sys_write(1, p_log, 5);

    // Verificación final integral
    if (*bwd_coord == 20 && (dist as i32) == 2 && v_ok == 20 && ret_code == 40 && res_slot[0] == 60 && err_fallback == 999) {
        return res_slot[0]; // 60
    }

    return 0;
}
