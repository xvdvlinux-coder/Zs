#include "zs_lexer.h"
#include <ctype.h>

const char* zs_token_kind_name(zs_token_kind_t kind) {
    switch (kind) {
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        case TOK_IDENT: return "IDENTIFIER";
        case TOK_INT_LIT: return "INT_LITERAL";
        case TOK_FLOAT_LIT: return "FLOAT_LITERAL";
        case TOK_STRING_LIT: return "STRING_LITERAL";
        case TOK_RAW_STRING_LIT: return "RAW_STRING_LITERAL";
        case TOK_CHAR_LIT: return "CHAR_LITERAL";

        case TOK_KW_MODULE: return "module";
        case TOK_KW_USE: return "use";
        case TOK_KW_PUB: return "pub";
        case TOK_KW_COMPTIME: return "comptime";
        case TOK_KW_FOREIGN: return "foreign";
        case TOK_KW_RAW: return "raw";
        case TOK_KW_FN: return "fn";
        case TOK_KW_ANCHOR: return "anchor";
        case TOK_KW_RECORD: return "record";
        case TOK_KW_CHOICE: return "choice";
        case TOK_KW_CONTRACT: return "contract";
        case TOK_KW_CONST: return "const";
        case TOK_KW_TYPE: return "type";
        case TOK_KW_ALIGN: return "align";

        case TOK_KW_IF: return "if";
        case TOK_KW_ELSE: return "else";
        case TOK_KW_LOOP: return "loop";
        case TOK_KW_WHILE: return "while";
        case TOK_KW_WALK: return "walk";
        case TOK_KW_IN: return "in";
        case TOK_KW_BY: return "by";
        case TOK_KW_BRANCH: return "branch";
        case TOK_KW_BREAK: return "break";
        case TOK_KW_CONTINUE: return "continue";
        case TOK_KW_RETURN: return "return";
        case TOK_KW_DEFER: return "defer";

        case TOK_KW_FAIL: return "fail";
        case TOK_KW_TRAP: return "trap";
        case TOK_KW_OR: return "or";
        case TOK_KW_CATCH: return "catch";

        case TOK_KW_VAL: return "val";
        case TOK_KW_VAR: return "var";
        case TOK_KW_AS: return "as";

        case TOK_KW_TRUE: return "true";
        case TOK_KW_FALSE: return "false";
        case TOK_KW_NONE: return "none";

        case TOK_KW_I8: return "i8";
        case TOK_KW_I16: return "i16";
        case TOK_KW_I32: return "i32";
        case TOK_KW_I64: return "i64";
        case TOK_KW_ISIZE: return "isize";
        case TOK_KW_U8: return "u8";
        case TOK_KW_U16: return "u16";
        case TOK_KW_U32: return "u32";
        case TOK_KW_U64: return "u64";
        case TOK_KW_USIZE: return "usize";
        case TOK_KW_F32: return "f32";
        case TOK_KW_F64: return "f64";
        case TOK_KW_BOOL: return "bool";
        case TOK_KW_CHAR32: return "char32";
        case TOK_KW_STR: return "str";
        case TOK_KW_STRING: return "string";
        case TOK_KW_VOID: return "void";
        case TOK_KW_LOC: return "loc";
        case TOK_KW_VIEW: return "view";

        case TOK_TAD_FWD: return "~>";
        case TOK_TAD_BWD: return "<~";
        case TOK_TAD_SAFE: return "?~>";
        case TOK_TAD_RING: return "%~>";
        case TOK_TAD_BYTE: return "~>#";
        case TOK_TAD_DIST: return "<=>";

        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";

        case TOK_EQ_EQ: return "==";
        case TOK_BANG_EQ: return "!=";
        case TOK_LESS: return "<";
        case TOK_LESS_EQ: return "<=";
        case TOK_GREATER: return ">";
        case TOK_GREATER_EQ: return ">=";

        case TOK_AMP_AMP: return "&&";
        case TOK_PIPE_PIPE: return "||";
        case TOK_BANG: return "!";
        case TOK_AMP: return "&";
        case TOK_PIPE: return "|";
        case TOK_CARET: return "^";
        case TOK_TILDE: return "~";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";

        case TOK_EQ: return "=";
        case TOK_PLUS_EQ: return "+=";
        case TOK_MINUS_EQ: return "-=";
        case TOK_STAR_EQ: return "*=";
        case TOK_SLASH_EQ: return "/=";
        case TOK_PERCENT_EQ: return "%=";
        case TOK_AMP_EQ: return "&=";
        case TOK_PIPE_EQ: return "|=";
        case TOK_CARET_EQ: return "^=";
        case TOK_LSHIFT_EQ: return "<<=";
        case TOK_RSHIFT_EQ: return ">>=";

        case TOK_ARROW: return "->";
        case TOK_FAT_ARROW: return "=>";
        case TOK_COLON: return ":";
        case TOK_COLON_COLON: return "::";
        case TOK_SEMICOLON: return ";";
        case TOK_COMMA: return ",";
        case TOK_DOT: return ".";
        case TOK_DOT_DOT: return "..";
        case TOK_AT: return "@";
        case TOK_HASH: return "#";
        case TOK_QUESTION: return "?";

        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";

        default: return "<desconocido>";
    }
}

void zs_lexer_init(zs_lexer_t* lexer, const zs_source_file_t* file, zs_arena_t* arena) {
    lexer->file = file;
    lexer->arena = arena;
    lexer->cursor = 0;
    lexer->line = 1;
    lexer->col = 1;
    lexer->has_peeked = false;
    lexer->error_count = 0;
}

static inline bool is_at_end(const zs_lexer_t* lexer) {
    return lexer->cursor >= lexer->file->length;
}

static inline char peek_char(const zs_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->file->content[lexer->cursor];
}

static inline char peek_char_n(const zs_lexer_t* lexer, u32 n) {
    if (lexer->cursor + n >= lexer->file->length) return '\0';
    return lexer->file->content[lexer->cursor + n];
}

static inline char advance_char(zs_lexer_t* lexer) {
    if (is_at_end(lexer)) return '\0';
    char c = lexer->file->content[lexer->cursor++];
    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    return c;
}

static inline bool match_char(zs_lexer_t* lexer, char expected) {
    if (is_at_end(lexer) || lexer->file->content[lexer->cursor] != expected) {
        return false;
    }
    advance_char(lexer);
    return true;
}

static void skip_whitespace_and_comments(zs_lexer_t* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek_char(lexer);

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance_char(lexer);
            continue;
        }

        // Comentario de línea //
        if (c == '/' && peek_char_n(lexer, 1) == '/') {
            advance_char(lexer);
            advance_char(lexer);
            while (!is_at_end(lexer) && peek_char(lexer) != '\n') {
                advance_char(lexer);
            }
            continue;
        }

        // Comentario de bloque anidable /* ... */
        if (c == '/' && peek_char_n(lexer, 1) == '*') {
            advance_char(lexer);
            advance_char(lexer);
            u32 depth = 1;

            while (!is_at_end(lexer) && depth > 0) {
                if (peek_char(lexer) == '/' && peek_char_n(lexer, 1) == '*') {
                    advance_char(lexer);
                    advance_char(lexer);
                    depth++;
                } else if (peek_char(lexer) == '*' && peek_char_n(lexer, 1) == '/') {
                    advance_char(lexer);
                    advance_char(lexer);
                    depth--;
                } else {
                    advance_char(lexer);
                }
            }
            continue;
        }

        break;
    }
}

static struct {
    const char* kw;
    zs_token_kind_t kind;
} KEYWORDS[] = {
    {"module", TOK_KW_MODULE},
    {"use", TOK_KW_USE},
    {"pub", TOK_KW_PUB},
    {"comptime", TOK_KW_COMPTIME},
    {"foreign", TOK_KW_FOREIGN},
    {"raw", TOK_KW_RAW},
    {"fn", TOK_KW_FN},
    {"anchor", TOK_KW_ANCHOR},
    {"record", TOK_KW_RECORD},
    {"choice", TOK_KW_CHOICE},
    {"contract", TOK_KW_CONTRACT},
    {"const", TOK_KW_CONST},
    {"type", TOK_KW_TYPE},
    {"align", TOK_KW_ALIGN},
    {"if", TOK_KW_IF},
    {"else", TOK_KW_ELSE},
    {"loop", TOK_KW_LOOP},
    {"while", TOK_KW_WHILE},
    {"walk", TOK_KW_WALK},
    {"in", TOK_KW_IN},
    {"by", TOK_KW_BY},
    {"branch", TOK_KW_BRANCH},
    {"break", TOK_KW_BREAK},
    {"continue", TOK_KW_CONTINUE},
    {"return", TOK_KW_RETURN},
    {"defer", TOK_KW_DEFER},
    {"fail", TOK_KW_FAIL},
    {"trap", TOK_KW_TRAP},
    {"or", TOK_KW_OR},
    {"catch", TOK_KW_CATCH},
    {"val", TOK_KW_VAL},
    {"var", TOK_KW_VAR},
    {"as", TOK_KW_AS},
    {"true", TOK_KW_TRUE},
    {"false", TOK_KW_FALSE},
    {"none", TOK_KW_NONE},
    {"i8", TOK_KW_I8},
    {"i16", TOK_KW_I16},
    {"i32", TOK_KW_I32},
    {"i64", TOK_KW_I64},
    {"isize", TOK_KW_ISIZE},
    {"u8", TOK_KW_U8},
    {"u16", TOK_KW_U16},
    {"u32", TOK_KW_U32},
    {"u64", TOK_KW_U64},
    {"usize", TOK_KW_USIZE},
    {"f32", TOK_KW_F32},
    {"f64", TOK_KW_F64},
    {"bool", TOK_KW_BOOL},
    {"char32", TOK_KW_CHAR32},
    {"str", TOK_KW_STR},
    {"string", TOK_KW_STRING},
    {"void", TOK_KW_VOID},
    {"loc", TOK_KW_LOC},
    {"view", TOK_KW_VIEW}
};

static zs_token_kind_t lookup_keyword(const char* ident, usize len) {
    for (usize i = 0; i < ZS_ARRAY_LEN(KEYWORDS); ++i) {
        if (strlen(KEYWORDS[i].kw) == len && memcmp(KEYWORDS[i].kw, ident, len) == 0) {
            return KEYWORDS[i].kind;
        }
    }
    return TOK_IDENT;
}

static zs_token_t make_token(zs_lexer_t* lexer, zs_token_kind_t kind, u32 start_cursor, zs_loc_t loc) {
    zs_token_t tok;
    tok.kind = kind;
    tok.loc = loc;
    tok.length = lexer->cursor - start_cursor;
    tok.start = lexer->file->content + start_cursor;
    tok.suffix = NUM_SUFFIX_NONE;
    tok.u64_val = 0;
    return tok;
}

static zs_token_t make_error_token(zs_lexer_t* lexer, const char* msg, u32 start_cursor, zs_loc_t loc) {
    lexer->error_count++;
    zs_token_t tok = make_token(lexer, TOK_ERROR, start_cursor, loc);
    tok.string_val.str = msg;
    tok.string_val.len = strlen(msg);
    return tok;
}

static zs_num_suffix_t parse_num_suffix(zs_lexer_t* lexer) {
    if (peek_char(lexer) != '_') return NUM_SUFFIX_NONE;

    // Verificar posibles sufijos
    const char* rest = lexer->file->content + lexer->cursor;
    usize remaining = lexer->file->length - lexer->cursor;

    static const struct {
        const char* name;
        usize len;
        zs_num_suffix_t suf;
    } SUFFIXES[] = {
        {"_isize", 6, NUM_SUFFIX_ISIZE},
        {"_usize", 6, NUM_SUFFIX_USIZE},
        {"_i64", 4, NUM_SUFFIX_I64},
        {"_u64", 4, NUM_SUFFIX_U64},
        {"_i32", 4, NUM_SUFFIX_I32},
        {"_u32", 4, NUM_SUFFIX_U32},
        {"_i16", 4, NUM_SUFFIX_I16},
        {"_u16", 4, NUM_SUFFIX_U16},
        {"_i8", 3, NUM_SUFFIX_I8},
        {"_u8", 3, NUM_SUFFIX_U8},
        {"_f64", 4, NUM_SUFFIX_F64},
        {"_f32", 4, NUM_SUFFIX_F32}
    };

    for (usize i = 0; i < ZS_ARRAY_LEN(SUFFIXES); ++i) {
        if (remaining >= SUFFIXES[i].len && memcmp(rest, SUFFIXES[i].name, SUFFIXES[i].len) == 0) {
            for (usize j = 0; j < SUFFIXES[i].len; ++j) {
                advance_char(lexer);
            }
            return SUFFIXES[i].suf;
        }
    }

    return NUM_SUFFIX_NONE;
}

static inline bool is_suffix_start(const zs_lexer_t* lexer) {
    if (peek_char(lexer) != '_') return false;
    char next = peek_char_n(lexer, 1);
    return (next == 'i' || next == 'u' || next == 'f');
}

static zs_token_t lex_number(zs_lexer_t* lexer, u32 start_cursor, zs_loc_t loc) {
    bool is_float = false;
    int base = 10;

    char first = lexer->file->content[start_cursor];

    if (first == '0') {
        char next = peek_char(lexer);
        if (next == 'x' || next == 'X') {
            base = 16;
            advance_char(lexer);
            while (isxdigit(peek_char(lexer)) || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
                advance_char(lexer);
            }
        } else if (next == 'b' || next == 'B') {
            base = 2;
            advance_char(lexer);
            while (peek_char(lexer) == '0' || peek_char(lexer) == '1' || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
                advance_char(lexer);
            }
        } else if (next == 'o' || next == 'O') {
            base = 8;
            advance_char(lexer);
            while ((peek_char(lexer) >= '0' && peek_char(lexer) <= '7') || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
                advance_char(lexer);
            }
        }
    }

    if (base == 10) {
        while (isdigit(peek_char(lexer)) || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
            advance_char(lexer);
        }

        // Parte decimal de float: debe haber un punto seguido de un dígito (para no colisionar con ..)
        if (peek_char(lexer) == '.' && isdigit(peek_char_n(lexer, 1))) {
            is_float = true;
            advance_char(lexer); // consumir '.'
            while (isdigit(peek_char(lexer)) || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
                advance_char(lexer);
            }
        }

        // Exponente (e/E)
        if (peek_char(lexer) == 'e' || peek_char(lexer) == 'E') {
            is_float = true;
            advance_char(lexer);
            if (peek_char(lexer) == '+' || peek_char(lexer) == '-') {
                advance_char(lexer);
            }
            while (isdigit(peek_char(lexer)) || (peek_char(lexer) == '_' && !is_suffix_start(lexer))) {
                advance_char(lexer);
            }
        }
    }

    u32 num_end_cursor = lexer->cursor;
    zs_num_suffix_t suffix = parse_num_suffix(lexer);
    if (suffix == NUM_SUFFIX_F32 || suffix == NUM_SUFFIX_F64) {
        is_float = true;
    }

    zs_token_t tok = make_token(lexer, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT, start_cursor, loc);
    tok.suffix = suffix;

    usize num_len = num_end_cursor - start_cursor;
    char* clean = (char*)zs_arena_alloc(lexer->arena, num_len + 1, 1);
    usize clean_len = 0;
    for (usize i = start_cursor; i < num_end_cursor; ++i) {
        char ch = lexer->file->content[i];
        if (ch == '_') continue;
        clean[clean_len++] = ch;
    }
    clean[clean_len] = '\0';


    if (is_float) {
        tok.f64_val = strtod(clean, NULL);
    } else {
        if (base == 16) {
            tok.u64_val = strtoull(clean + 2, NULL, 16);
        } else if (base == 2) {
            tok.u64_val = strtoull(clean + 2, NULL, 2);
        } else if (base == 8) {
            tok.u64_val = strtoull(clean + 2, NULL, 8);
        } else {
            tok.u64_val = strtoull(clean, NULL, 10);
        }
        tok.i64_val = (i64)tok.u64_val;
    }

    return tok;
}

static zs_token_t lex_string(zs_lexer_t* lexer, u32 start_cursor, zs_loc_t loc) {
    // Buffer para decodificar caracteres con escape
    usize cap = 64;
    char* decoded = (char*)zs_arena_alloc(lexer->arena, cap, 1);
    usize dlen = 0;

    while (!is_at_end(lexer) && peek_char(lexer) != '"') {
        char c = advance_char(lexer);
        if (c == '\\') {
            char esc = advance_char(lexer);
            switch (esc) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '0': c = '\0'; break;
                case 'x': {
                    char h1 = advance_char(lexer);
                    char h2 = advance_char(lexer);
                    char hex_buf[3] = {h1, h2, '\0'};
                    c = (char)strtoul(hex_buf, NULL, 16);
                    break;
                }
                default:
                    // Dejar caracter tal cual si no se reconoce
                    c = esc;
                    break;
            }
        }

        if (dlen + 1 >= cap) {
            usize new_cap = cap * 2;
            char* new_buf = (char*)zs_arena_alloc(lexer->arena, new_cap, 1);
            memcpy(new_buf, decoded, dlen);
            decoded = new_buf;
            cap = new_cap;
        }
        decoded[dlen++] = c;
    }

    if (is_at_end(lexer)) {
        return make_error_token(lexer, "Cadena de texto sin cerrar al final del archivo", start_cursor, loc);
    }

    advance_char(lexer); // consumir comilla de cierre '"'
    decoded[dlen] = '\0';

    zs_token_t tok = make_token(lexer, TOK_STRING_LIT, start_cursor, loc);
    tok.string_val.str = decoded;
    tok.string_val.len = dlen;
    return tok;
}

static zs_token_t lex_raw_string(zs_lexer_t* lexer, u32 start_cursor, zs_loc_t loc) {
    // Espera formato r#"..."#
    // Ya consumió 'r' y '#' y '"'
    u32 content_start = lexer->cursor;
    while (!is_at_end(lexer)) {
        if (peek_char(lexer) == '"' && peek_char_n(lexer, 1) == '#') {
            break;
        }
        advance_char(lexer);
    }

    if (is_at_end(lexer)) {
        return make_error_token(lexer, "Cadena cruda r#\"...\"# sin cerrar", start_cursor, loc);
    }

    u32 content_end = lexer->cursor;
    advance_char(lexer); // '"'
    advance_char(lexer); // '#'

    usize len = content_end - content_start;
    char* copy = zs_arena_strndup(lexer->arena, lexer->file->content + content_start, len);

    zs_token_t tok = make_token(lexer, TOK_RAW_STRING_LIT, start_cursor, loc);
    tok.string_val.str = copy;
    tok.string_val.len = len;
    return tok;
}

static zs_token_t lex_char(zs_lexer_t* lexer, u32 start_cursor, zs_loc_t loc) {
    char32 val = 0;
    if (is_at_end(lexer) || peek_char(lexer) == '\'') {
        return make_error_token(lexer, "Literal de carácter vacío", start_cursor, loc);
    }

    char c = advance_char(lexer);
    if (c == '\\') {
        char esc = advance_char(lexer);
        switch (esc) {
            case 'n': val = '\n'; break;
            case 'r': val = '\r'; break;
            case 't': val = '\t'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            case '0': val = '\0'; break;
            default: val = (char32)esc; break;
        }
    } else {
        val = (char32)(u8)c;
    }

    if (!match_char(lexer, '\'')) {
        return make_error_token(lexer, "Literal de carácter sin cerrar con comilla simple", start_cursor, loc);
    }

    zs_token_t tok = make_token(lexer, TOK_CHAR_LIT, start_cursor, loc);
    tok.char_val = val;
    return tok;
}

static zs_token_t lex_ident_or_kw(zs_lexer_t* lexer, u32 start_cursor, zs_loc_t loc) {
    while (!is_at_end(lexer) && (isalnum(peek_char(lexer)) || peek_char(lexer) == '_')) {
        advance_char(lexer);
    }

    usize len = lexer->cursor - start_cursor;
    const char* ident_str = lexer->file->content + start_cursor;
    zs_token_kind_t kind = lookup_keyword(ident_str, len);

    zs_token_t tok = make_token(lexer, kind, start_cursor, loc);
    tok.string_val.str = ident_str;
    tok.string_val.len = len;
    return tok;
}

zs_token_t zs_lexer_next(zs_lexer_t* lexer) {
    if (lexer->has_peeked) {
        lexer->has_peeked = false;
        return lexer->peeked_token;
    }

    skip_whitespace_and_comments(lexer);

    u32 start_cursor = lexer->cursor;
    zs_loc_t loc = zs_source_get_loc(lexer->file, start_cursor);

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF, start_cursor, loc);
    }

    char c = advance_char(lexer);

    // Cadenas crudas r#"..."#
    if (c == 'r' && peek_char(lexer) == '#' && peek_char_n(lexer, 1) == '"') {
        advance_char(lexer); // '#'
        advance_char(lexer); // '"'
        return lex_raw_string(lexer, start_cursor, loc);
    }

    // Identificadores y palabras clave
    if (isalpha(c) || c == '_') {
        return lex_ident_or_kw(lexer, start_cursor, loc);
    }

    // Literales numéricos
    if (isdigit(c)) {
        return lex_number(lexer, start_cursor, loc);
    }

    // Literales de cadena
    if (c == '"') {
        return lex_string(lexer, start_cursor, loc);
    }

    // Literales de carácter
    if (c == '\'') {
        return lex_char(lexer, start_cursor, loc);
    }

    // Operadores TAD y signos de puntuación
    switch (c) {
        case '~':
            if (match_char(lexer, '>')) {
                if (match_char(lexer, '#')) {
                    return make_token(lexer, TOK_TAD_BYTE, start_cursor, loc); // ~>#
                }
                return make_token(lexer, TOK_TAD_FWD, start_cursor, loc); // ~>
            }
            return make_token(lexer, TOK_TILDE, start_cursor, loc);

        case '<':
            if (match_char(lexer, '~')) {
                return make_token(lexer, TOK_TAD_BWD, start_cursor, loc); // <~
            }
            if (match_char(lexer, '=')) {
                if (match_char(lexer, '>')) {
                    return make_token(lexer, TOK_TAD_DIST, start_cursor, loc); // <=>
                }
                return make_token(lexer, TOK_LESS_EQ, start_cursor, loc); // <=
            }
            if (match_char(lexer, '<')) {
                if (match_char(lexer, '=')) {
                    return make_token(lexer, TOK_LSHIFT_EQ, start_cursor, loc); // <<=
                }
                return make_token(lexer, TOK_LSHIFT, start_cursor, loc); // <<
            }
            return make_token(lexer, TOK_LESS, start_cursor, loc);

        case '?':
            if (peek_char(lexer) == '~' && peek_char_n(lexer, 1) == '>') {
                advance_char(lexer);
                advance_char(lexer);
                return make_token(lexer, TOK_TAD_SAFE, start_cursor, loc); // ?~>
            }
            return make_token(lexer, TOK_QUESTION, start_cursor, loc);

        case '%':
            if (peek_char(lexer) == '~' && peek_char_n(lexer, 1) == '>') {
                advance_char(lexer);
                advance_char(lexer);
                return make_token(lexer, TOK_TAD_RING, start_cursor, loc); // %~>
            }
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_PERCENT_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_PERCENT, start_cursor, loc);

        case '-':
            if (match_char(lexer, '>')) {
                return make_token(lexer, TOK_ARROW, start_cursor, loc); // ->
            }
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_MINUS_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_MINUS, start_cursor, loc);

        case '=':
            if (match_char(lexer, '>')) {
                return make_token(lexer, TOK_FAT_ARROW, start_cursor, loc); // =>
            }
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_EQ_EQ, start_cursor, loc); // ==
            }
            return make_token(lexer, TOK_EQ, start_cursor, loc);

        case '!':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_BANG_EQ, start_cursor, loc); // !=
            }
            return make_token(lexer, TOK_BANG, start_cursor, loc);

        case '>':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_GREATER_EQ, start_cursor, loc); // >=
            }
            if (match_char(lexer, '>')) {
                if (match_char(lexer, '=')) {
                    return make_token(lexer, TOK_RSHIFT_EQ, start_cursor, loc); // >>=
                }
                return make_token(lexer, TOK_RSHIFT, start_cursor, loc); // >>
            }
            return make_token(lexer, TOK_GREATER, start_cursor, loc);

        case '&':
            if (match_char(lexer, '&')) {
                return make_token(lexer, TOK_AMP_AMP, start_cursor, loc); // &&
            }
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_AMP_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_AMP, start_cursor, loc);

        case '|':
            if (match_char(lexer, '|')) {
                return make_token(lexer, TOK_PIPE_PIPE, start_cursor, loc); // ||
            }
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_PIPE_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_PIPE, start_cursor, loc);

        case '^':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_CARET_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_CARET, start_cursor, loc);

        case '+':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_PLUS_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_PLUS, start_cursor, loc);

        case '*':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_STAR_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_STAR, start_cursor, loc);

        case '/':
            if (match_char(lexer, '=')) {
                return make_token(lexer, TOK_SLASH_EQ, start_cursor, loc);
            }
            return make_token(lexer, TOK_SLASH, start_cursor, loc);

        case '.':
            if (match_char(lexer, '.')) {
                return make_token(lexer, TOK_DOT_DOT, start_cursor, loc); // ..
            }
            return make_token(lexer, TOK_DOT, start_cursor, loc);

        case ':':
            if (match_char(lexer, ':')) {
                return make_token(lexer, TOK_COLON_COLON, start_cursor, loc); // ::
            }
            return make_token(lexer, TOK_COLON, start_cursor, loc);

        case ';': return make_token(lexer, TOK_SEMICOLON, start_cursor, loc);
        case ',': return make_token(lexer, TOK_COMMA, start_cursor, loc);
        case '@': return make_token(lexer, TOK_AT, start_cursor, loc);
        case '#': return make_token(lexer, TOK_HASH, start_cursor, loc);

        case '(': return make_token(lexer, TOK_LPAREN, start_cursor, loc);
        case ')': return make_token(lexer, TOK_RPAREN, start_cursor, loc);
        case '[': return make_token(lexer, TOK_LBRACKET, start_cursor, loc);
        case ']': return make_token(lexer, TOK_RBRACKET, start_cursor, loc);
        case '{': return make_token(lexer, TOK_LBRACE, start_cursor, loc);
        case '}': return make_token(lexer, TOK_RBRACE, start_cursor, loc);

        default: {
            char err_msg[64];
            snprintf(err_msg, sizeof(err_msg), "Carácter inesperado: '%c' (0x%02X)", c, (u8)c);
            return make_error_token(lexer, zs_arena_strdup(lexer->arena, err_msg), start_cursor, loc);
        }
    }
}

zs_token_t zs_lexer_peek(zs_lexer_t* lexer) {
    if (!lexer->has_peeked) {
        lexer->peeked_token = zs_lexer_next(lexer);
        lexer->has_peeked = true;
    }
    return lexer->peeked_token;
}
