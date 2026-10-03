#ifndef ZS_LEXER_H
#define ZS_LEXER_H

#include "zs_token.h"

typedef struct {
    const zs_source_file_t* file;
    zs_arena_t* arena;
    u32 cursor;
    u32 line;
    u32 col;
    bool has_peeked;
    zs_token_t peeked_token;
    u32 error_count;
} zs_lexer_t;

void zs_lexer_init(zs_lexer_t* lexer, const zs_source_file_t* file, zs_arena_t* arena);
zs_token_t zs_lexer_next(zs_lexer_t* lexer);
zs_token_t zs_lexer_peek(zs_lexer_t* lexer);

#endif // ZS_LEXER_H
