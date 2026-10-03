#include "zs_arena.h"
#include "zs_source.h"
#include "zs_token.h"
#include "zs_lexer.h"

static bool run_test_01(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_01_tokens_basic.Zs");
    if (!file) {
        printf("Fallo al abrir tests/test_01_tokens_basic.Zs\n");
        return false;
    }
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    zs_token_kind_t expected[] = {
        TOK_KW_MODULE, TOK_IDENT, TOK_COLON_COLON, TOK_IDENT, TOK_SEMICOLON,
        TOK_KW_PUB, TOK_KW_FN, TOK_IDENT, TOK_LPAREN, TOK_RPAREN, TOK_ARROW, TOK_KW_I32, TOK_LBRACE,
        TOK_KW_VAL, TOK_IDENT, TOK_EQ, TOK_INT_LIT, TOK_SEMICOLON,
        TOK_KW_VAR, TOK_IDENT, TOK_EQ, TOK_INT_LIT, TOK_SEMICOLON,
        TOK_KW_RETURN, TOK_IDENT, TOK_PLUS, TOK_IDENT, TOK_SEMICOLON,
        TOK_RBRACE, TOK_EOF
    };

    for (usize i = 0; i < ZS_ARRAY_LEN(expected); ++i) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind != expected[i]) {
            printf("Test 01 fallo en token %zu: esperado %s (%d), obtenido %s (%d)\n",
                   i, zs_token_kind_name(expected[i]), expected[i],
                   zs_token_kind_name(tok.kind), tok.kind);
            return false;
        }
    }
    return true;
}

static bool run_test_02(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_02_numeric_literals.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    bool found_dec = false, found_hex = false, found_bin = false, found_oct = false, found_flt = false, found_sci = false;

    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;

        if (tok.kind == TOK_INT_LIT) {
            if (tok.u64_val == 1000000) found_dec = true;
            if (tok.u64_val == 0xDEADBEEF) found_hex = true;
            if (tok.u64_val == 0b10101100) found_bin = true;
            if (tok.u64_val == 0755) found_oct = true;
        } else if (tok.kind == TOK_FLOAT_LIT) {
            if (tok.f64_val >= 3.14 && tok.f64_val <= 3.15) found_flt = true;
            if (tok.f64_val > 0.0 && tok.f64_val < 0.0001) found_sci = true;
        }
    }

    return found_dec && found_hex && found_bin && found_oct && found_flt && found_sci;
}

static bool run_test_03(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_03_typed_suffixes.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    u32 suffixes_found = 0;
    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        if (tok.suffix != NUM_SUFFIX_NONE) {
            suffixes_found++;
        }
    }
    // Debe haber 12 sufijos tipados
    return suffixes_found == 12;
}

static bool run_test_04(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_04_utf8_strings.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    bool found_escaped_str = false;
    bool found_unicode_str = false;
    bool found_char = false;

    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_STRING_LIT) {
            if (strstr(tok.string_val.str, "\n\t") != NULL) found_escaped_str = true;
            if (strstr(tok.string_val.str, "🚀") != NULL) found_unicode_str = true;
        } else if (tok.kind == TOK_CHAR_LIT) {
            if (tok.char_val == 'Z') found_char = true;
        }
    }

    return found_escaped_str && found_unicode_str && found_char;
}

static bool run_test_05(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_05_raw_strings.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    u32 raw_count = 0;
    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_RAW_STRING_LIT) {
            raw_count++;
        }
    }
    return raw_count == 2;
}

static bool run_test_06(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_06_nested_comments.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    u32 idents = 0;
    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_IDENT) {
            idents++;
        }
    }
    // módulo test, nested_comments, x, y -> 4 identificadores
    return idents >= 4 && lexer.error_count == 0;
}

static bool run_test_07(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_07_tad_operators.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    bool fwd = false, bwd = false, safe = false, ring = false, byte = false, dist = false;

    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;

        if (tok.kind == TOK_TAD_FWD) fwd = true;
        if (tok.kind == TOK_TAD_BWD) bwd = true;
        if (tok.kind == TOK_TAD_SAFE) safe = true;
        if (tok.kind == TOK_TAD_RING) ring = true;
        if (tok.kind == TOK_TAD_BYTE) byte = true;
        if (tok.kind == TOK_TAD_DIST) dist = true;
    }

    return fwd && bwd && safe && ring && byte && dist;
}

static bool run_test_08(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_08_delimiters_logic.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    bool found_and = false, found_or = false, found_shl = false, found_shr = false, found_xor = false;

    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;

        if (tok.kind == TOK_AMP_AMP) found_and = true;
        if (tok.kind == TOK_PIPE_PIPE) found_or = true;
        if (tok.kind == TOK_LSHIFT) found_shl = true;
        if (tok.kind == TOK_RSHIFT) found_shr = true;
        if (tok.kind == TOK_CARET) found_xor = true;
    }

    return found_and && found_or && found_shl && found_shr && found_xor;
}

static bool run_test_09(zs_arena_t* arena) {
    // Generar archivo temporal con CRLF en memoria
    const char* crlf_content = "module test::crlf;\r\nval x = 10;\r\nval y = 20;\r\n";
    zs_source_file_t* file = zs_source_file_from_string(arena, "virtual_crlf.Zs", crlf_content, strlen(crlf_content));

    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    u32 token_count = 0;
    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        token_count++;
    }

    // Debe tokenizar sin error y reportar 3 líneas
    return token_count > 0 && lexer.line == 4 && lexer.error_count == 0;
}

static bool run_test_10(zs_arena_t* arena) {
    zs_source_file_t* file = zs_source_file_load(arena, "tests/test_10_lexer_errors.Zs");
    if (!file) return false;
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, file, arena);

    bool error_token_found = false;

    while (true) {
        zs_token_t tok = zs_lexer_next(&lexer);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_ERROR) {
            error_token_found = true;
        }
    }

    return error_token_found && lexer.error_count > 0;
}

int main(void) {
    printf("=====================================================\n");
    printf("   ZS TEST SUITE - BLOQUE 1: LÉXICO Y TOKENIZACIÓN   \n");
    printf("=====================================================\n");

    zs_arena_t* arena = zs_arena_create(256 * 1024);

    struct {
        int id;
        const char* name;
        bool (*func)(zs_arena_t*);
    } tests[] = {
        {1, "Identificadores y estructura básica", run_test_01},
        {2, "Literales numéricos (dec/hex/bin/oct/float)", run_test_02},
        {3, "Sufijos numéricos tipados (_i32, _u8, etc.)", run_test_03},
        {4, "Cadenas UTF-8 con escape y caracteres Unicode", run_test_04},
        {5, "Cadenas crudas r#\"...\"#", run_test_05},
        {6, "Comentarios anidados /* /* */ */ y de línea", run_test_06},
        {7, "Operadores exclusivos del modelo TAD (~>, <~, etc.)", run_test_07},
        {8, "Delimitadores y operadores lógicos / bits", run_test_08},
        {9, "Resiliencia ante saltos de línea CRLF", run_test_09},
        {10, "Detección y reporte controlado de errores léxicos", run_test_10}
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
    printf("Resultado Bloque 1: %d/%d tests pasados con éxito.\n", passed, total);
    printf("=====================================================\n");

    zs_arena_destroy(arena);
    return (passed == total) ? 0 : 1;
}
