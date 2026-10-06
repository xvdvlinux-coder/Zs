#include "zs_arena.h"
#include "zs_source.h"
#include "zs_token.h"
#include "zs_lexer.h"
#include "zs_ast.h"
#include "zs_parser.h"
#include "zs_types.h"
#include "zs_symtab.h"
#include "zs_checker.h"
#include "zs_codegen_arm64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

static bool compile_to_asm(zs_arena_t* arena, int test_id, const char* zs_path, char* out_asm_path, size_t out_len) {
    zs_source_file_t* file = zs_source_file_load(arena, zs_path);
    if (!file) return false;

    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);
    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0) return false;

    zs_checker_t checker;
    zs_checker_init(&checker, file, arena);
    if (!zs_checker_check_program(&checker, prog)) return false;

    zs_codegen_arm64_t cg;
    zs_codegen_init(&cg, arena);
    const char* asm_code = zs_codegen_generate(&cg, prog);

    snprintf(out_asm_path, out_len, "build/test_%02d.s", test_id);
    FILE* f = fopen(out_asm_path, "w");
    if (!f) return false;
    fputs(asm_code, f);
    fclose(f);
    return true;
}

// Test 51: Syscall directo Linux AArch64: sys_write (fd 1, stdout)
static bool run_test_51(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 51, "tests/test_51_direct_sys_write.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_51.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_51");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_51.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        // Redirigir stdout a /dev/null para no saturar terminal
        FILE* devnull = freopen("/dev/null", "w", stdout);
        (void)devnull;
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 51);
}

// Test 52: Entrada interactiva de usuario (std_readline + std_parse_i64 desde stdin)
static bool run_test_52(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 52, "tests/test_52_direct_sys_read.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_52.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_52");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_52.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    // Crear tubería para simular entrada de usuario interactiva
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fd[1]); // cerrar extremo de escritura
        dup2(pipe_fd[0], 0); // redirigir pipe a stdin (fd 0)
        close(pipe_fd[0]);
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }

    close(pipe_fd[0]);
    // Escribir entrada simulada de usuario: "52\n"
    const char* user_input = "52\n";
    ssize_t w = write(pipe_fd[1], user_input, strlen(user_input));
    (void)w;
    close(pipe_fd[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 52);
}

// Test 53: Manejo de archivos: sys_openat y sys_close
static bool run_test_53(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 53, "tests/test_53_file_open_close.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_53.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_53");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_53.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    remove("/tmp/zs_t53.tmp");
    return (WIFEXITED(status) && WEXITSTATUS(status) == 53);
}

// Test 54: Entrada y salida formateada: std_fmt_i64
static bool run_test_54(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 54, "tests/test_54_formatted_i64_str.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_54.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_54");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_54.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 54);
}

// Test 55: Rutina de punto de entrada autónomo _start (sin libc ni runtime externo)
static bool run_test_55(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 55, "tests/test_55_freestanding_start.Zs", asm_path, sizeof(asm_path))) return false;

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_55");
    // Enlace puramente freestanding (-nostdlib): la entrada es _start en zs_start.s
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -nostdlib -pie -o %s %s runtime/zs_start.s runtime/zs_runtime.s 2> build/gcc_err_55.log", bin_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 55);
}

// Test 56: Módulo I/O: std_println
static bool run_test_56(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 56, "tests/test_56_high_level_print.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_56.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_56");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_56.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    int pipe_out[2];
    if (pipe(pipe_out) < 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_out[0]);
        dup2(pipe_out[1], 1); // Redirigir stdout al pipe
        close(pipe_out[1]);
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }

    close(pipe_out[1]);
    char captured[32];
    memset(captured, 0, sizeof(captured));
    size_t total = 0;
    ssize_t r;
    while ((r = read(pipe_out[0], captured + total, sizeof(captured) - 1 - total)) > 0) {
        total += (size_t)r;
    }
    close(pipe_out[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    bool output_ok = (strcmp(captured, "Zs-Lang\n") == 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 56 && output_ok);
}

// Test 57: Módulo de archivos: ciclo completo de escritura y lectura
static bool run_test_57(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 57, "tests/test_57_file_read_write.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_57.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_57");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_57.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    remove("/tmp/zs_t57.tmp");
    return (WIFEXITED(status) && WEXITSTATUS(status) == 57);
}

// Test 58: Compilador Driver CLI zsc: generación de ensamblador ARM64 (.s)
static bool run_test_58(zs_arena_t* a) {
    (void)a;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./build/zsc --emit-asm tests/test_58_cli_driver_asm.Zs -o build/cli_test_58.s > /dev/null 2>&1");
    if (system(cmd) != 0) return false;

    // Verificar que el archivo generado existe y contiene código válido
    FILE* f = fopen("build/cli_test_58.s", "r");
    if (!f) return false;
    char buffer[256];
    bool found_main = false;
    while (fgets(buffer, sizeof(buffer), f)) {
        if (strstr(buffer, "test_main:")) {
            found_main = true;
            break;
        }
    }
    fclose(f);
    if (!found_main) return false;

    // Escribir runner para test 58
    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_58.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    // Compilar y ejecutar para validar valor 58
    snprintf(cmd, sizeof(cmd), "gcc -O0 -no-pie -o build/bin_test_58 build/cli_test_58.s %s runtime/zs_runtime.s 2> /dev/null", runner_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        execl("build/bin_test_58", "build/bin_test_58", NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 58);
}

// Test 59: Funcionalidad modular multiplataforma (module + use + flag -c generando .o y enlace)
static bool run_test_59(zs_arena_t* a) {
    (void)a;
    char cmd[512];
    // 1. Compilar módulo math_mod.Zs a build/math_mod.o con la flag -c
    snprintf(cmd, sizeof(cmd), "./build/zsc -c tests/math_mod.Zs -o build/math_mod.o > /dev/null 2>&1");
    if (system(cmd) != 0) return false;

    // 2. Compilar módulo principal test_59_module_multitranslation.Zs a build/test_59.o con -c
    snprintf(cmd, sizeof(cmd), "./build/zsc -c tests/test_59_module_multitranslation.Zs -o build/test_59.o > /dev/null 2>&1");
    if (system(cmd) != 0) return false;

    // 3. Enlazar ambos archivos objeto .o con runtime autónomo
    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_59.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    snprintf(cmd, sizeof(cmd), "gcc -O0 -no-pie -o build/bin_test_59 %s build/test_59.o build/math_mod.o runtime/zs_runtime.s 2> build/gcc_err_59.log", runner_path);
    if (system(cmd) != 0) return false;

    // 4. Ejecutar y verificar resolución entre módulos
    pid_t pid = fork();
    if (pid == 0) {
        execl("build/bin_test_59", "build/bin_test_59", NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 59);
}

// Test 60: Integración completa de sistema: TAD + Retorno Dual + Defer + Syscall I/O
static bool run_test_60(zs_arena_t* a) {
    char asm_path[128];
    if (!compile_to_asm(a, 60, "tests/test_60_full_system_integration.Zs", asm_path, sizeof(asm_path))) return false;

    char runner_path[128];
    snprintf(runner_path, sizeof(runner_path), "build/runner_60.c");
    FILE* fr = fopen(runner_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_60");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -no-pie -o %s %s %s runtime/zs_runtime.s 2> build/gcc_err_60.log", bin_path, runner_path, asm_path);
    if (system(cmd) != 0) return false;

    pid_t pid = fork();
    if (pid == 0) {
        FILE* devnull = freopen("/dev/null", "w", stdout);
        (void)devnull;
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 60);
}

int main(void) {
    printf("=====================================================\n");
    printf("  ZS TEST SUITE - BLOQUE 6: RUNTIME AUTÓNOMO E I/O   \n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(512 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {51, "Syscall directo Linux AArch64: sys_write (stdout)", run_test_51},
        {52, "Entrada interactiva de usuario (std_readline y parse desde stdin)", run_test_52},
        {53, "Manejo de archivos: sys_openat y sys_close", run_test_53},
        {54, "Entrada y salida formateada: std_fmt_i64 entero a ASCII", run_test_54},
        {55, "Punto de entrada autónomo _start sin libc ni runtime externo", run_test_55},
        {56, "Módulo I/O de alto nivel: std_println con captura en tubería", run_test_56},
        {57, "Ciclo completo de persistencia: escritura y lectura de archivo", run_test_57},
        {58, "Compilador Driver CLI zsc: emisión de ensamblador ARM64 (.s)", run_test_58},
        {59, "Funcionalidad modular multiplataforma (flag -c y enlace de módulos .o)", run_test_59},
        {60, "Integración completa de sistema: TAD + Error Dual + Defer + I/O", run_test_60}
    };

    int passed = 0;
    int total = (int)ZS_ARRAY_LEN(tests);

    for (int i = 0; i < total; ++i) {
        bool ok = tests[i].func(arena);
        if (ok) {
            printf("[\033[1;32mPASÓ\033[0m] Test %02d: %s\n", tests[i].id, tests[i].name);
            passed++;
        } else {
            printf("[\033[1;31mFALLÓ\033[0m] Test %02d: %s\n", tests[i].id, tests[i].name);
        }
        zs_arena_reset(arena);
    }

    printf("=====================================================\n");
    printf("Resultado Bloque 6: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
