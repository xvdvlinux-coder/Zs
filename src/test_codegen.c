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

static bool run_test_31(zs_arena_t* a) { return compile_and_run(a, 31, "tests/test_31_anchor_alloc_stack.Zs", 10, false); }
static bool run_test_32(zs_arena_t* a) { return compile_and_run(a, 32, "tests/test_32_fwd_displacement.Zs", 40, false); }
static bool run_test_33(zs_arena_t* a) { return compile_and_run(a, 33, "tests/test_33_bwd_displacement.Zs", 40, false); }
static bool run_test_34(zs_arena_t* a) { return compile_and_run(a, 34, "tests/test_34_safe_displacement_fallback.Zs", 100, false); }
static bool run_test_35(zs_arena_t* a) { return compile_and_run(a, 35, "tests/test_35_toroidal_ring_buffer.Zs", 2, false); }
static bool run_test_36(zs_arena_t* a) { return compile_and_run(a, 36, "tests/test_36_aligned_byte_step.Zs", 200, false); }
static bool run_test_37(zs_arena_t* a) { return compile_and_run(a, 37, "tests/test_37_coordinate_metric.Zs", 5, false); }
static bool run_test_38(zs_arena_t* a) { return compile_and_run(a, 38, "tests/test_38_view_slicing.Zs", 40, false); }
static bool run_test_39(zs_arena_t* a) { return compile_and_run(a, 39, "tests/test_39_walk_iteration.Zs", 100, false); }
static bool run_test_40(zs_arena_t* a) { return compile_and_run(a, 40, "tests/test_40_trap_out_of_bounds.Zs", 0, true); }

int main(void) {
    printf("=====================================================\n");
    printf("  ZS TEST SUITE - BLOQUE 4: ENSAMBLADOR ARM64 Y TAD  \n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(512 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {31, "Asignación de anclaje en pila y coordenada .start", run_test_31},
        {32, "Desplazamiento lógico hacia adelante (p ~> k)", run_test_32},
        {33, "Desplazamiento lógico hacia atrás (q <~ k)", run_test_33},
        {34, "Navegación con guardia y fallback (p ?~> k or fallback)", run_test_34},
        {35, "Aritmética toroidal cíclica (%~> k)", run_test_35},
        {36, "Desplazamiento físico alineado en bytes (~># bytes)", run_test_36},
        {37, "Métrica de distancia entre coordenadas (p <=> q)", run_test_37},
        {38, "Subvistas físicas view<T> mediante slice [start .. end]", run_test_38},
        {39, "Recorrido de anclaje con iterador walk .. in", run_test_39},
        {40, "Seguridad de hardware: trampa brk #0x42 en desbordamiento", run_test_40}
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
    printf("Resultado Bloque 4: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
