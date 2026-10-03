#include "zs_arena.h"
#include "zs_source.h"
#include "zs_token.h"
#include "zs_lexer.h"
#include "zs_ast.h"
#include "zs_parser.h"

static bool run_test_11(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_11_modules_imports.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0) return false;

    return (strcmp(prog->module_name, "red::socket") == 0) &&
           (prog->import_count == 2) &&
           (prog->decl_count == 1);
}

static bool run_test_12(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_12_record_decl.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 2) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_RECORD) return false;
    return (strcmp(d->record.name, "Cabecera") == 0) &&
           (d->record.alignment == 8) &&
           (d->record.field_count == 4);
}

static bool run_test_13(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_13_choice_adt.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_CHOICE) return false;
    return (strcmp(d->choice.name, "EventoRed") == 0) &&
           (d->choice.variant_count == 3);
}

static bool run_test_14(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_14_pratt_precedence.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_FUNCTION || !d->function.body) return false;

    // Verificar que la función calcular tenga 4 sentencias
    return d->function.body->stmt_count == 4;
}

static bool run_test_15(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_15_if_else_expr.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_FUNCTION || !d->function.body) return false;

    zs_ast_stmt_t* s = d->function.body->stmts[0];
    return s->kind == STMT_IF && s->if_stmt.else_if != NULL;
}

static bool run_test_16(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_16_loop_while_walk.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_FUNCTION || !d->function.body) return false;

    bool has_while = false, has_loop = false, has_walk = false;
    for (u32 i = 0; i < d->function.body->stmt_count; ++i) {
        if (d->function.body->stmts[i]->kind == STMT_WHILE) has_while = true;
        if (d->function.body->stmts[i]->kind == STMT_LOOP) has_loop = true;
        if (d->function.body->stmts[i]->kind == STMT_WALK) has_walk = true;
    }

    return has_while && has_loop && has_walk;
}

static bool run_test_17(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_17_branch_syntax.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_FUNCTION || !d->function.body) return false;

    zs_ast_stmt_t* s = d->function.body->stmts[0];
    return s->kind == STMT_BRANCH && s->branch_stmt.arm_count == 3;
}

static bool run_test_18(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_18_defer_syntax.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    if (d->kind != DECL_FUNCTION || !d->function.body) return false;

    u32 defers = 0;
    for (u32 i = 0; i < d->function.body->stmt_count; ++i) {
        if (d->function.body->stmts[i]->kind == STMT_DEFER) defers++;
    }

    return defers == 2;
}

static bool run_test_19(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_19_contracts.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 1) return false;

    zs_ast_decl_t* d = prog->decls[0];
    return d->kind == DECL_CONTRACT &&
           strcmp(d->contract.name, "Lector") == 0 &&
           d->contract.method_count == 2;
}

static bool run_test_20(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_20_foreign_raw.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);

    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0 || prog->decl_count < 2) return false;

    zs_ast_decl_t* d0 = prog->decls[0];
    zs_ast_decl_t* d1 = prog->decls[1];

    bool foreign_ok = (d0->kind == DECL_FOREIGN && d0->foreign_block.function_count == 2);
    bool raw_ok = (d1->kind == DECL_FUNCTION && d1->function.body && d1->function.body->stmt_count >= 1 &&
                   d1->function.body->stmts[0]->kind == STMT_RAW);

    return foreign_ok && raw_ok;
}

int main(void) {
    printf("=====================================================\n");
    printf("     ZS TEST SUITE - BLOQUE 2: SINTAXIS Y AST        \n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(256 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {11, "Declaraciones de módulos e importaciones (module, use)", run_test_11},
        {12, "Estructuras nominales record con align(8)", run_test_12},
        {13, "Tipos suma algebraicos choice con/sin carga útil", run_test_13},
        {14, "Precedencia de expresiones compleja (Pratt Parser)", run_test_14},
        {15, "Condicionales if/else if/else", run_test_15},
        {16, "Bucles loop, while e iterador walk .. in .. by", run_test_16},
        {17, "Coincidencia de patrones branch con literales, rangos y else", run_test_17},
        {18, "Sentencias defer simples y en bloque", run_test_18},
        {19, "Definición de contratos (contract)", run_test_19},
        {20, "Bloques foreign ABI y bloques de bajo nivel raw", run_test_20}
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
    printf("Resultado Bloque 2: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
