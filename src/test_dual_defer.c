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

static bool compile_and_run(zs_arena_t* arena, int test_id, const char* zs_path, int expected_exit, bool expect_trap) {
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

    char asm_path[128];
    snprintf(asm_path, sizeof(asm_path), "build/test_%02d.s", test_id);
    FILE* f = fopen(asm_path, "w");
    if (!f) return false;
    fputs(asm_code, f);
    fclose(f);

    // Escribir runner C auxiliar
    char runner_c_path[128];
    snprintf(runner_c_path, sizeof(runner_c_path), "build/runner_%02d.c", test_id);
    FILE* fr = fopen(runner_c_path, "w");
    if (!fr) return false;
    fprintf(fr, "extern int test_main(void);\nint main(void) { return test_main(); }\n");
    fclose(fr);

    // Compilar y linkear con gcc en aarch64
    char bin_path[128];
    snprintf(bin_path, sizeof(bin_path), "build/bin_test_%02d", test_id);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O0 -pie -o %s %s %s 2> build/gcc_err_%02d.log", bin_path, runner_c_path, asm_path, test_id);
    int res = system(cmd);
    if (res != 0) {
        fprintf(stderr, "Error al ensamblar/linkear test %02d (comando: %s)\n", test_id, cmd);
        return false;
    }

    // Ejecutar binario
    pid_t pid = fork();
    if (pid == 0) {
        execl(bin_path, bin_path, NULL);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (expect_trap) {
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            return (sig == SIGTRAP || sig == SIGILL);
        }
        return false;
    } else {
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            return code == expected_exit;
        }
        return false;
    }
}

static bool run_test_41(zs_arena_t* a) { return compile_and_run(a, 41, "tests/test_41_dual_return_success.Zs", 10, false); }
static bool run_test_42(zs_arena_t* a) { return compile_and_run(a, 42, "tests/test_42_dual_return_fail.Zs", 42, false); }
static bool run_test_43(zs_arena_t* a) { return compile_and_run(a, 43, "tests/test_43_or_propagate.Zs", 55, false); }
static bool run_test_44(zs_arena_t* a) { return compile_and_run(a, 44, "tests/test_44_or_fallback_val.Zs", 77, false); }
static bool run_test_45(zs_arena_t* a) { return compile_and_run(a, 45, "tests/test_45_catch_block.Zs", 20, false); }
static bool run_test_46(zs_arena_t* a) { return compile_and_run(a, 46, "tests/test_46_defer_function_exit.Zs", 50, false); }
static bool run_test_47(zs_arena_t* a) { return compile_and_run(a, 47, "tests/test_47_defer_lifo_order.Zs", 24, false); }
static bool run_test_48(zs_arena_t* a) { return compile_and_run(a, 48, "tests/test_48_defer_on_fail.Zs", 14, false); }
static bool run_test_49(zs_arena_t* a) { return compile_and_run(a, 49, "tests/test_49_defer_in_loop_break.Zs", 18, false); }
static bool run_test_50(zs_arena_t* a) { return compile_and_run(a, 50, "tests/test_50_register_preservation.Zs", 49, false); }

int main(void) {
    printf("=====================================================\n");
    printf("  ZS TEST SUITE - BLOQUE 5: RETORNO DUAL Y DEFER     \n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(512 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {41, "Retorno dual de éxito (Carry = 0, valor retornado en x0)", run_test_41},
        {42, "Retorno dual de fallo (Carry = 1, error retornado en x1)", run_test_42},
        {43, "Propagación de fallos con 'or return fail'", run_test_43},
        {44, "Valor de recuperación fallback con operador 'or'", run_test_44},
        {45, "Captura estructurada de error con 'catch(err) { ... }'", run_test_45},
        {46, "Ejecución garantizada de 'defer' en salida de función", run_test_46},
        {47, "Orden estricto LIFO de sentencias 'defer' encadenadas", run_test_47},
        {48, "Garantía de ejecución de 'defer' en bifurcación de fallo", run_test_48},
        {49, "Ámbito léxico y defers en bloques dentro de bucles", run_test_49},
        {50, "Preservación de registros y estabilidad de ABI compleja", run_test_50}
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
    printf("Resultado Bloque 5: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
