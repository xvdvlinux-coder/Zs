#ifndef ZS_TOKEN_H
#define ZS_TOKEN_H

#include "zs_common.h"
#include "zs_source.h"

typedef enum {
    TOK_EOF = 0,
    TOK_ERROR,

    // Identificadores y Literales
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_RAW_STRING_LIT,
    TOK_CHAR_LIT,

    // Palabras clave de estructura
    TOK_KW_MODULE,
    TOK_KW_USE,
    TOK_KW_PUB,
    TOK_KW_COMPTIME,
    TOK_KW_FOREIGN,
    TOK_KW_RAW,

    // Palabras clave de entidades
    TOK_KW_FN,
    TOK_KW_ANCHOR,
    TOK_KW_RECORD,
    TOK_KW_CHOICE,
    TOK_KW_CONTRACT,
    TOK_KW_CONST,
    TOK_KW_TYPE,
    TOK_KW_ALIGN,

    // Control de flujo
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_LOOP,
    TOK_KW_WHILE,
    TOK_KW_WALK,
    TOK_KW_IN,
    TOK_KW_BY,
    TOK_KW_BRANCH,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_RETURN,
    TOK_KW_DEFER,

    // Errores y bifurcación
    TOK_KW_FAIL,
    TOK_KW_TRAP,
    TOK_KW_OR,
    TOK_KW_CATCH,

    // Variables y mutabilidad
    TOK_KW_VAL,
    TOK_KW_VAR,
    TOK_KW_AS,

    // Constantes booleanas y nulo
    TOK_KW_TRUE,
    TOK_KW_FALSE,
    TOK_KW_NONE,

    // Tipos primitivos
    TOK_KW_I8,
    TOK_KW_I16,
    TOK_KW_I32,
    TOK_KW_I64,
    TOK_KW_ISIZE,
    TOK_KW_U8,
    TOK_KW_U16,
    TOK_KW_U32,
    TOK_KW_U64,
    TOK_KW_USIZE,
    TOK_KW_F32,
    TOK_KW_F64,
    TOK_KW_BOOL,
    TOK_KW_CHAR32,
    TOK_KW_STR,
    TOK_KW_STRING,
    TOK_KW_VOID,
    TOK_KW_LOC,
    TOK_KW_VIEW,

    // Operadores TAD (Topological Anchors & Displacements)
    TOK_TAD_FWD,       // ~>
    TOK_TAD_BWD,       // <~
    TOK_TAD_SAFE,      // ?~>
    TOK_TAD_RING,      // %~>
    TOK_TAD_BYTE,      // ~>#
    TOK_TAD_DIST,      // <=>

    // Operadores aritméticos
    TOK_PLUS,          // +
    TOK_MINUS,         // -
    TOK_STAR,          // *
    TOK_SLASH,         // /
    TOK_PERCENT,       // %

    // Operadores de comparación
    TOK_EQ_EQ,         // ==
    TOK_BANG_EQ,       // !=
    TOK_LESS,          // <
    TOK_LESS_EQ,       // <=
    TOK_GREATER,       // >
    TOK_GREATER_EQ,    // >=

    // Operadores lógicos y de bits
    TOK_AMP_AMP,       // &&
    TOK_PIPE_PIPE,     // ||
    TOK_BANG,          // !
    TOK_AMP,           // &
    TOK_PIPE,          // |
    TOK_CARET,         // ^
    TOK_TILDE,         // ~
    TOK_LSHIFT,        // <<
    TOK_RSHIFT,        // >>

    // Asignación
    TOK_EQ,            // =
    TOK_PLUS_EQ,       // +=
    TOK_MINUS_EQ,      // -=
    TOK_STAR_EQ,       // *=
    TOK_SLASH_EQ,      // /=
    TOK_PERCENT_EQ,    // %=
    TOK_AMP_EQ,        // &=
    TOK_PIPE_EQ,       // |=
    TOK_CARET_EQ,      // ^=
    TOK_LSHIFT_EQ,     // <<=
    TOK_RSHIFT_EQ,     // >>=

    // Puntuación y delimitadores
    TOK_ARROW,         // ->
    TOK_FAT_ARROW,     // =>
    TOK_COLON,         // :
    TOK_COLON_COLON,   // ::
    TOK_SEMICOLON,     // ;
    TOK_COMMA,         // ,
    TOK_DOT,           // .
    TOK_DOT_DOT,       // ..
    TOK_AT,            // @
    TOK_HASH,          // #
    TOK_QUESTION,      // ?

    TOK_LPAREN,        // (
    TOK_RPAREN,        // )
    TOK_LBRACKET,      // [
    TOK_RBRACKET,      // ]
    TOK_LBRACE,        // {
    TOK_RBRACE,        // }

    TOK_COUNT
} zs_token_kind_t;

typedef enum {
    NUM_SUFFIX_NONE = 0,
    NUM_SUFFIX_I8,
    NUM_SUFFIX_I16,
    NUM_SUFFIX_I32,
    NUM_SUFFIX_I64,
    NUM_SUFFIX_ISIZE,
    NUM_SUFFIX_U8,
    NUM_SUFFIX_U16,
    NUM_SUFFIX_U32,
    NUM_SUFFIX_U64,
    NUM_SUFFIX_USIZE,
    NUM_SUFFIX_F32,
    NUM_SUFFIX_F64
} zs_num_suffix_t;

typedef struct {
    zs_token_kind_t kind;
    zs_loc_t loc;
    u32 length;
    const char* start; // Puntero al texto en el buffer de código fuente

    union {
        u64 u64_val;
        i64 i64_val;
        f64 f64_val;
        char32 char_val;
        struct {
            const char* str;
            usize len;
        } string_val;
    };
    zs_num_suffix_t suffix;
} zs_token_t;

const char* zs_token_kind_name(zs_token_kind_t kind);

#endif // ZS_TOKEN_H
