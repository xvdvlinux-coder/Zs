#include "zs_arena.h"
#include "zs_source.h"
#include "zs_token.h"
#include "zs_lexer.h"
#include "zs_ast.h"
#include "zs_parser.h"
#include "zs_types.h"
#include "zs_symtab.h"
#include "zs_checker.h"

static bool check_file(zs_arena_t* arena, const char* path, bool should_pass) {
    zs_source_file_t* file = zs_source_file_load(arena, path);
    if (!file) return false;

    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0) return false;

    zs_checker_t checker;
    zs_checker_init(&checker, file, arena);
    bool ok = zs_checker_check_program(&checker, prog);

    return should_pass ? ok : (!ok && checker.error_count > 0);
}

static bool run_test_21(zs_arena_t* arena) {
    // Negativo: reasignar 'val' debe fallar
    return check_file(arena, "tests/test_21_val_immutable.Zs", false);
}

static bool run_test_22(zs_arena_t* arena) {
    // Positivo: reasignar 'var' con el mismo tipo debe pasar
    return check_file(arena, "tests/test_22_var_mutable.Zs", true);
}

static bool run_test_23(zs_arena_t* arena) {
    // Negativo: u32 + u64 sin cast debe fallar
    return check_file(arena, "tests/test_23_strict_no_coercion.Zs", false);
}

static bool run_test_24(zs_arena_t* arena) {
    // Positivo: función llamada antes de declararse debe pasar
    return check_file(arena, "tests/test_24_two_pass_symbols.Zs", true);
}

static bool run_test_25(zs_arena_t* arena) {
    // Negativo: branch no exhaustivo sin else debe fallar
    return check_file(arena, "tests/test_25_branch_exhaustiveness.Zs", false);
}

static bool run_test_26(zs_arena_t* arena) {
    // Negativo: escape léxico de anclaje debe fallar
    return check_file(arena, "tests/test_26_anchor_lexical_escape.Zs", false);
}

static bool run_test_27(zs_arena_t* arena) {
    // Negativo: salto no alineado en bytes ~># debe fallar
    return check_file(arena, "tests/test_27_phase_alignment_check.Zs", false);
}

static bool run_test_28(zs_arena_t* arena) {
    // Negativo: distancia <=> entre anclajes distintos debe fallar
    return check_file(arena, "tests/test_28_distance_anchor_origin.Zs", false);
}

static bool run_test_29(zs_arena_t* arena) {
    // Negativo: lectura de variable no inicializada debe fallar
    return check_file(arena, "tests/test_29_uninitialized_var.Zs", false);
}

static bool run_test_30(zs_arena_t* arena) {
    // Positivo: declaración de contrato debe pasar
    return check_file(arena, "tests/test_30_contract_conformance.Zs", true);
}

int main(void) {
    printf("=====================================================\n");
    printf("   ZS TEST SUITE - BLOQUE 3: ANÁLISIS SEMÁNTICO Y TIPOS\n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(256 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {21, "Inmutabilidad estricta de val (rechazo de reasignación)", run_test_21},
        {22, "Mutabilidad de var y reasignación válida de tipo", run_test_22},
        {23, "Rechazo de coerción implícita heterogénea (u32 + u64)", run_test_23},
        {24, "Resolución de símbolos no lineal en dos pasadas", run_test_24},
        {25, "Verificación estricta de exhaustividad en branch", run_test_25},
        {26, "Regla de No-Escape Léxico de coordenadas TAD", run_test_26},
        {27, "Congruencia de fase en saltos por bytes (~>#)", run_test_27},
        {28, "Validación de identidad de anclajes en métrica (<=>)", run_test_28},
        {29, "Detección de uso de variables no inicializadas", run_test_29},
        {30, "Conformidad y verificación de contratos (contract)", run_test_30}
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
    printf("Resultado Bloque 3: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
