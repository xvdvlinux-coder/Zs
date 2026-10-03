#ifndef ZS_PARSER_H
#define ZS_PARSER_H

#include "zs_ast.h"
#include "zs_lexer.h"

typedef struct {
    zs_lexer_t* lexer;
    zs_arena_t* arena;
    zs_token_t current;
    zs_token_t previous;
    u32 error_count;
    bool panic_mode;
} zs_parser_t;

void zs_parser_init(zs_parser_t* parser, zs_lexer_t* lexer, zs_arena_t* arena);
zs_ast_program_t* zs_parser_parse_program(zs_parser_t* parser);

// Funciones para análisis unitario de nodos
zs_ast_decl_t* zs_parser_parse_decl(zs_parser_t* parser);
zs_ast_stmt_t* zs_parser_parse_stmt(zs_parser_t* parser);
zs_ast_expr_t* zs_parser_parse_expr(zs_parser_t* parser);
zs_ast_type_t* zs_parser_parse_type(zs_parser_t* parser);

#endif // ZS_PARSER_H
