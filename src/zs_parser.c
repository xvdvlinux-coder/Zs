#include "zs_parser.h"
#include <ctype.h>

// Niveles de precedencia para Pratt Parser
typedef enum {
    PREC_NONE = 0,
    PREC_ASSIGN,       // =, +=, -=, etc.
    PREC_OR_FALLBACK,  // or, catch
    PREC_LOGICAL_OR,   // ||
    PREC_LOGICAL_AND,  // &&
    PREC_BIT_OR,       // |
    PREC_BIT_XOR,      // ^
    PREC_BIT_AND,      // &
    PREC_EQUALITY,     // ==, !=
    PREC_RELATIONAL,   // <, <=, >, >=, <=>
    PREC_SHIFT,        // <<, >>
    PREC_TAD_NAV,      // ~>, <~, ?~>, %~>, ~>#
    PREC_ADDITIVE,     // +, -
    PREC_MULTIPLICATIVE, // *, /, %
    PREC_CAST,         // as
    PREC_UNARY,        // -, !, ~, @
    PREC_CALL,         // ., (), [], [..]
    PREC_PRIMARY
} zs_precedence_t;

static void advance(zs_parser_t* p) {
    p->previous = p->current;
    p->current = zs_lexer_next(p->lexer);
}

static bool check(zs_parser_t* p, zs_token_kind_t kind) {
    return p->current.kind == kind;
}

static bool match(zs_parser_t* p, zs_token_kind_t kind) {
    if (check(p, kind)) {
        advance(p);
        return true;
    }
    return false;
}

static void error_at_current(zs_parser_t* p, const char* msg) {
    p->error_count++;
    if (p->panic_mode) return;
    p->panic_mode = true;
    zs_source_print_error(p->lexer->file, p->current.loc, "%s", msg);
}

static bool consume(zs_parser_t* p, zs_token_kind_t kind, const char* msg) {
    if (check(p, kind)) {
        advance(p);
        return true;
    }
    error_at_current(p, msg);
    return false;
}

static void synchronize(zs_parser_t* p) {
    p->panic_mode = false;
    advance(p);
    while (!check(p, TOK_EOF)) {
        if (p->previous.kind == TOK_SEMICOLON) return;
        switch (p->current.kind) {
            case TOK_KW_FN:
            case TOK_KW_VAL:
            case TOK_KW_VAR:
            case TOK_KW_ANCHOR:
            case TOK_KW_RECORD:
            case TOK_KW_CHOICE:
            case TOK_KW_IF:
            case TOK_KW_WHILE:
            case TOK_KW_LOOP:
            case TOK_KW_RETURN:
            case TOK_KW_DEFER:
            case TOK_KW_MODULE:
                return;
            default:
                advance(p);
                break;
        }
    }
}


void zs_parser_init(zs_parser_t* parser, zs_lexer_t* lexer, zs_arena_t* arena) {
    parser->lexer = lexer;
    parser->arena = arena;
    parser->error_count = 0;
    parser->panic_mode = false;
    advance(parser); // Cargar primer token
}

// Declaraciones adelantadas
static zs_ast_expr_t* parse_expr_prec(zs_parser_t* p, zs_precedence_t prec);
static zs_ast_block_t* parse_block(zs_parser_t* p);

// --- PARSER DE TIPOS ---
zs_ast_type_t* zs_parser_parse_type(zs_parser_t* p) {
    zs_loc_t loc = p->current.loc;

    // Primitivos
    switch (p->current.kind) {
        case TOK_KW_I8: case TOK_KW_I16: case TOK_KW_I32: case TOK_KW_I64: case TOK_KW_ISIZE:
        case TOK_KW_U8: case TOK_KW_U16: case TOK_KW_U32: case TOK_KW_U64: case TOK_KW_USIZE:
        case TOK_KW_F32: case TOK_KW_F64: case TOK_KW_BOOL: case TOK_KW_CHAR32:
        case TOK_KW_STR: case TOK_KW_STRING: case TOK_KW_VOID: {
            zs_token_kind_t k = p->current.kind;
            advance(p);
            return zs_ast_type_primitive(p->arena, loc, k);
        }
        case TOK_KW_LOC: {
            advance(p);
            consume(p, TOK_LESS, "Se esperaba '<' después de 'loc'");
            zs_ast_type_t* sub = zs_parser_parse_type(p);
            consume(p, TOK_GREATER, "Se esperaba '>' después de loc<...>");
            return zs_ast_type_loc(p->arena, loc, sub);
        }
        case TOK_KW_VIEW: {
            advance(p);
            consume(p, TOK_LESS, "Se esperaba '<' después de 'view'");
            zs_ast_type_t* sub = zs_parser_parse_type(p);
            consume(p, TOK_GREATER, "Se esperaba '>' después de view<...>");
            return zs_ast_type_view(p->arena, loc, sub);
        }
        case TOK_IDENT: {
            const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            advance(p);
            return zs_ast_type_named(p->arena, loc, name);
        }
        case TOK_LPAREN: {
            advance(p);
            // Tupla
            u32 cap = 4, count = 0;
            zs_ast_type_t** types = (zs_ast_type_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
            while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                if (count >= cap) {
                    cap *= 2;
                    zs_ast_type_t** new_t = (zs_ast_type_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                    memcpy(new_t, types, count * sizeof(void*));
                    types = new_t;
                }
                types[count++] = zs_parser_parse_type(p);
                if (!match(p, TOK_COMMA)) break;
            }
            consume(p, TOK_RPAREN, "Se esperaba ')' tras tipos de tupla");
            zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(p->arena, sizeof(zs_ast_type_t), sizeof(void*));
            t->kind = TYPE_KIND_TUPLE;
            t->loc = loc;
            t->tuple.types = types;
            t->tuple.count = count;
            return t;
        }
        default:
            error_at_current(p, "Se esperaba un nombre de tipo válido");
            advance(p);
            return zs_ast_type_primitive(p->arena, loc, TOK_KW_VOID);
    }
}

// --- PARSER DE EXPRESIONES (PRATT PARSER) ---
static zs_precedence_t get_infix_precedence(zs_token_kind_t op) {
    switch (op) {
        case TOK_KW_OR:
        case TOK_KW_CATCH:
            return PREC_OR_FALLBACK;

        case TOK_PIPE_PIPE: return PREC_LOGICAL_OR;
        case TOK_AMP_AMP: return PREC_LOGICAL_AND;
        case TOK_PIPE: return PREC_BIT_OR;
        case TOK_CARET: return PREC_BIT_XOR;
        case TOK_AMP: return PREC_BIT_AND;

        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
            return PREC_EQUALITY;

        case TOK_LESS: case TOK_LESS_EQ:
        case TOK_GREATER: case TOK_GREATER_EQ:
        case TOK_TAD_DIST:
            return PREC_RELATIONAL;

        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return PREC_SHIFT;

        case TOK_TAD_FWD:
        case TOK_TAD_BWD:
        case TOK_TAD_SAFE:
        case TOK_TAD_RING:
        case TOK_TAD_BYTE:
            return PREC_TAD_NAV;

        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_ADDITIVE;

        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
            return PREC_MULTIPLICATIVE;

        case TOK_KW_AS:
            return PREC_CAST;

        case TOK_DOT:
        case TOK_LPAREN:
        case TOK_LBRACKET:
            return PREC_CALL;

        default:
            return PREC_NONE;
    }
}

static bool is_record_init_ahead(zs_parser_t* p, const char* name) {
    if (!isupper((unsigned char)name[0])) return false;
    if (!check(p, TOK_LBRACE)) return false;

    u32 cur = p->lexer->cursor;
    const char* content = p->lexer->file->content;
    usize len = p->lexer->file->length;

    while (cur < len && (content[cur] == ' ' || content[cur] == '\t' || content[cur] == '\r' || content[cur] == '\n' || content[cur] == '/')) {
        if (content[cur] == '/' && cur + 1 < len && content[cur+1] == '/') {
            while (cur < len && content[cur] != '\n') cur++;
        } else if (content[cur] == '/' && cur + 1 < len && content[cur+1] == '*') {
            cur += 2;
            while (cur + 1 < len && !(content[cur] == '*' && content[cur+1] == '/')) cur++;
            if (cur + 1 < len) cur += 2;
        } else {
            cur++;
        }
    }

    if (cur < len && content[cur] == '}') return true;

    if (cur < len && (isalpha((unsigned char)content[cur]) || content[cur] == '_')) {
        while (cur < len && (isalnum((unsigned char)content[cur]) || content[cur] == '_')) {
            cur++;
        }
        while (cur < len && (content[cur] == ' ' || content[cur] == '\t' || content[cur] == '\r' || content[cur] == '\n')) cur++;
        if (cur < len && content[cur] == ':') {
            return true;
        }
    }

    return false;
}


static zs_ast_expr_t* parse_primary(zs_parser_t* p) {
    zs_loc_t loc = p->current.loc;

    switch (p->current.kind) {
        case TOK_INT_LIT: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_INT_LIT, loc);
            e->int_val = p->current.u64_val;
            advance(p);
            return e;
        }
        case TOK_FLOAT_LIT: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_FLOAT_LIT, loc);
            e->float_val = p->current.f64_val;
            advance(p);
            return e;
        }
        case TOK_STRING_LIT:
        case TOK_RAW_STRING_LIT: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_STRING_LIT, loc);
            e->string_val.str = p->current.string_val.str;
            e->string_val.len = p->current.string_val.len;
            advance(p);
            return e;
        }
        case TOK_CHAR_LIT: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_CHAR_LIT, loc);
            e->char_val = p->current.char_val;
            advance(p);
            return e;
        }
        case TOK_KW_TRUE: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_BOOL_LIT, loc);
            e->bool_val = true;
            advance(p);
            return e;
        }
        case TOK_KW_FALSE: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_BOOL_LIT, loc);
            e->bool_val = false;
            advance(p);
            return e;
        }
        case TOK_KW_NONE: {
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_NONE_LIT, loc);
            advance(p);
            return e;
        }
        case TOK_KW_RETURN: {
            advance(p);
            if (match(p, TOK_KW_FAIL)) {
                return zs_ast_expr_create(p->arena, EXPR_RETURN_FAIL, loc);
            }
            error_at_current(p, "Se esperaba 'fail' tras 'return' en expresión de propagación");
            return zs_ast_expr_create(p->arena, EXPR_INT_LIT, loc);
        }
        case TOK_IDENT: {
            const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            advance(p);

            while (match(p, TOK_COLON_COLON)) {
                if (!check(p, TOK_IDENT)) {
                    error_at_current(p, "Se esperaba identificador tras '::'");
                    break;
                }
                const char* sub = zs_arena_strndup(p->arena, p->current.start, p->current.length);
                advance(p);
                char buf[512];
                snprintf(buf, sizeof(buf), "%s::%s", name, sub);
                name = zs_arena_strdup(p->arena, buf);
            }

            // Verificar si es inicializador de record: Nombre { campo: valor, ... }
            if (is_record_init_ahead(p, name)) {
                advance(p); // consumir '{'
                u32 cap = 4, count = 0;
                const char** f_names = (const char**)zs_arena_alloc(p->arena, cap * sizeof(char*), sizeof(void*));
                zs_ast_expr_t** f_vals = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));

                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    if (count >= cap) {
                        cap *= 2;
                        const char** nf = (const char**)zs_arena_alloc(p->arena, cap * sizeof(char*), sizeof(void*));
                        zs_ast_expr_t** nv = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                        memcpy(nf, f_names, count * sizeof(char*));
                        memcpy(nv, f_vals, count * sizeof(void*));
                        f_names = nf;
                        f_vals = nv;
                    }
                    if (!check(p, TOK_IDENT)) {
                        error_at_current(p, "Se esperaba nombre de campo en inicializador de record");
                        break;
                    }
                    f_names[count] = zs_arena_strndup(p->arena, p->current.start, p->current.length);
                    advance(p);
                    consume(p, TOK_COLON, "Se esperaba ':' tras nombre de campo");
                    f_vals[count] = zs_parser_parse_expr(p);
                    count++;
                    if (!match(p, TOK_COMMA)) break;
                }
                consume(p, TOK_RBRACE, "Se esperaba '}' al cerrar inicializador de record");

                zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_RECORD_INIT, loc);
                e->record_init.type_name = name;
                e->record_init.field_names = f_names;
                e->record_init.field_values = f_vals;
                e->record_init.field_count = count;
                return e;
            }

            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_IDENT, loc);
            e->ident = name;
            return e;
        }

        case TOK_STAR:
        case TOK_MINUS:
        case TOK_BANG:
        case TOK_TILDE:
        case TOK_AT: {
            zs_token_kind_t op = p->current.kind;
            advance(p);
            zs_ast_expr_t* sub = parse_expr_prec(p, PREC_UNARY);
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_UNARY, loc);
            e->unary.op = op;
            e->unary.expr = sub;
            return e;
        }
        case TOK_LPAREN: {
            advance(p);
            zs_ast_expr_t* expr = zs_parser_parse_expr(p);
            // Tupla o expresión agrupada
            if (match(p, TOK_COMMA)) {
                u32 cap = 4, count = 0;
                zs_ast_expr_t** elems = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                elems[count++] = expr;

                while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                    if (count >= cap) {
                        cap *= 2;
                        zs_ast_expr_t** ne = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                        memcpy(ne, elems, count * sizeof(void*));
                        elems = ne;
                    }
                    elems[count++] = zs_parser_parse_expr(p);
                    if (!match(p, TOK_COMMA)) break;
                }
                consume(p, TOK_RPAREN, "Se esperaba ')' tras tupla");
                zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_TUPLE_LIT, loc);
                e->tuple_lit.elems = elems;
                e->tuple_lit.count = count;
                return e;
            }
            consume(p, TOK_RPAREN, "Se esperaba ')' tras expresión agrupada");
            return expr;
        }
        case TOK_LBRACKET: {
            advance(p);
            u32 cap = 8, count = 0;
            zs_ast_expr_t** elems = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                if (count >= cap) {
                    cap *= 2;
                    zs_ast_expr_t** ne = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                    memcpy(ne, elems, count * sizeof(void*));
                    elems = ne;
                }
                elems[count++] = zs_parser_parse_expr(p);
                if (!match(p, TOK_COMMA)) break;
            }
            consume(p, TOK_RBRACKET, "Se esperaba ']' al cerrar literal de array");
            zs_ast_expr_t* e = zs_ast_expr_create(p->arena, EXPR_ARRAY_LIT, loc);
            e->array_lit.elems = elems;
            e->array_lit.count = count;
            return e;
        }
        default:
            error_at_current(p, "Expresión esperada");
            advance(p);
            return zs_ast_expr_create(p->arena, EXPR_INT_LIT, loc);
    }
}

static zs_ast_expr_t* parse_expr_prec(zs_parser_t* p, zs_precedence_t prec) {
    zs_ast_expr_t* left = parse_primary(p);

    while (prec <= get_infix_precedence(p->current.kind)) {
        zs_token_t op_tok = p->current;
        zs_loc_t loc = op_tok.loc;
        advance(p);

        // Llamada a función: expr(arg1, arg2)
        if (op_tok.kind == TOK_LPAREN) {
            u32 cap = 4, count = 0;
            zs_ast_expr_t** args = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
            while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                if (count >= cap) {
                    cap *= 2;
                    zs_ast_expr_t** na = (zs_ast_expr_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
                    memcpy(na, args, count * sizeof(void*));
                    args = na;
                }
                args[count++] = zs_parser_parse_expr(p);
                if (!match(p, TOK_COMMA)) break;
            }
            consume(p, TOK_RPAREN, "Se esperaba ')' tras argumentos de llamada");
            zs_ast_expr_t* call_e = zs_ast_expr_create(p->arena, EXPR_CALL, loc);
            call_e->call.callee = left;
            call_e->call.args = args;
            call_e->call.arg_count = count;
            left = call_e;
            continue;
        }

        // Acceso por índice o slice: expr[index] o expr[start .. end]
        if (op_tok.kind == TOK_LBRACKET) {
            zs_ast_expr_t* first = NULL;
            if (!check(p, TOK_DOT_DOT) && !check(p, TOK_RBRACKET)) {
                first = zs_parser_parse_expr(p);
            }
            if (match(p, TOK_DOT_DOT)) {
                zs_ast_expr_t* end = NULL;
                if (!check(p, TOK_RBRACKET)) {
                    end = zs_parser_parse_expr(p);
                }
                consume(p, TOK_RBRACKET, "Se esperaba ']' tras slice [start .. end]");
                zs_ast_expr_t* sl = zs_ast_expr_create(p->arena, EXPR_SLICE, loc);
                sl->slice.target = left;
                sl->slice.start = first;
                sl->slice.end = end;
                left = sl;
            } else {
                consume(p, TOK_RBRACKET, "Se esperaba ']' tras índice");
                zs_ast_expr_t* idx = zs_ast_expr_create(p->arena, EXPR_INDEX, loc);
                idx->index.target = left;
                idx->index.index = first;
                left = idx;
            }
            continue;
        }

        // Acceso a campo: expr.campo
        if (op_tok.kind == TOK_DOT) {
            if (!check(p, TOK_IDENT)) {
                error_at_current(p, "Se esperaba nombre de campo tras '.'");
                break;
            }
            const char* f_name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            advance(p);
            zs_ast_expr_t* f = zs_ast_expr_create(p->arena, EXPR_FIELD, loc);
            f->field.target = left;
            f->field.field_name = f_name;
            left = f;
            continue;
        }

        // Cast: expr as Type
        if (op_tok.kind == TOK_KW_AS) {
            zs_ast_type_t* target = zs_parser_parse_type(p);
            zs_ast_expr_t* c = zs_ast_expr_create(p->arena, EXPR_CAST, loc);
            c->cast.expr = left;
            c->cast.target_type = target;
            left = c;
            continue;
        }

        // Manejo de errores: expr or expr
        if (op_tok.kind == TOK_KW_OR) {
            zs_ast_expr_t* right = parse_expr_prec(p, PREC_OR_FALLBACK + 1);
            zs_ast_expr_t* o = zs_ast_expr_create(p->arena, EXPR_OR_FALLBACK, loc);
            o->or_fallback.primary = left;
            o->or_fallback.fallback = right;
            left = o;
            continue;
        }

        // Manejo de errores: expr catch (err) { ... }
        if (op_tok.kind == TOK_KW_CATCH) {
            consume(p, TOK_LPAREN, "Se esperaba '(' tras 'catch'");
            const char* err_var = "err";
            if (check(p, TOK_IDENT)) {
                err_var = zs_arena_strndup(p->arena, p->current.start, p->current.length);
                advance(p);
            }
            consume(p, TOK_RPAREN, "Se esperaba ')' tras variable de error");
            zs_ast_block_t* blk = parse_block(p);
            zs_ast_expr_t* c = zs_ast_expr_create(p->arena, EXPR_CATCH, loc);
            c->catch_expr.primary = left;
            c->catch_expr.err_var = err_var;
            c->catch_expr.catch_block = blk;
            left = c;
            continue;
        }

        // Operador binario convencional o TAD
        zs_precedence_t next_prec = get_infix_precedence(op_tok.kind) + 1;
        zs_ast_expr_t* right = parse_expr_prec(p, next_prec);

        zs_ast_expr_t* bin = zs_ast_expr_create(p->arena, EXPR_BINARY, loc);
        bin->binary.op = op_tok.kind;
        bin->binary.left = left;
        bin->binary.right = right;
        left = bin;
    }

    return left;
}

zs_ast_expr_t* zs_parser_parse_expr(zs_parser_t* p) {
    return parse_expr_prec(p, PREC_ASSIGN);
}

// --- PARSER DE BLOQUES Y SENTENCIAS ---
static zs_ast_block_t* parse_block(zs_parser_t* p) {
    consume(p, TOK_LBRACE, "Se esperaba '{' al inicio del bloque");
    u32 cap = 8, count = 0;
    zs_ast_stmt_t** stmts = (zs_ast_stmt_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        if (count >= cap) {
            cap *= 2;
            zs_ast_stmt_t** ns = (zs_ast_stmt_t**)zs_arena_alloc(p->arena, cap * sizeof(void*), sizeof(void*));
            memcpy(ns, stmts, count * sizeof(void*));
            stmts = ns;
        }
        zs_ast_stmt_t* s = zs_parser_parse_stmt(p);
        if (s) {
            stmts[count++] = s;
        } else {
            if (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                advance(p);
            }
        }
    }
    consume(p, TOK_RBRACE, "Se esperaba '}' al final del bloque");

    zs_ast_block_t* block = (zs_ast_block_t*)zs_arena_alloc(p->arena, sizeof(zs_ast_block_t), sizeof(void*));
    block->stmts = stmts;
    block->stmt_count = count;
    return block;
}

zs_ast_stmt_t* zs_parser_parse_stmt(zs_parser_t* p) {
    zs_loc_t loc = p->current.loc;

    // Bloque autónomo { ... }
    if (check(p, TOK_LBRACE)) {
        zs_ast_block_t* blk = parse_block(p);
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_BLOCK, loc);
        s->block_stmt = blk;
        return s;
    }

    // val / var
    if (match(p, TOK_KW_VAL) || match(p, TOK_KW_VAR)) {
        bool is_var = (p->previous.kind == TOK_KW_VAR);
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba un nombre de variable");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);

        zs_ast_type_t* type = NULL;
        if (match(p, TOK_COLON)) {
            type = zs_parser_parse_type(p);
        }

        zs_ast_expr_t* init = NULL;
        if (match(p, TOK_EQ)) {
            init = zs_parser_parse_expr(p);
        }
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras declaración de variable");

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, is_var ? STMT_VAR_DECL : STMT_VAL_DECL, loc);
        s->var_decl.name = name;
        s->var_decl.type = type;
        s->var_decl.init = init;
        return s;
    }

    // anchor nombre: T[N] [= init];
    if (match(p, TOK_KW_ANCHOR)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de anclaje");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);
        consume(p, TOK_COLON, "Se esperaba ':' tras nombre de anclaje");
        zs_ast_type_t* elem_type = zs_parser_parse_type(p);
        consume(p, TOK_LBRACKET, "Se esperaba '[' tras tipo de elemento de anclaje");
        zs_ast_expr_t* size_e = zs_parser_parse_expr(p);
        consume(p, TOK_RBRACKET, "Se esperaba ']' tras tamaño del anclaje");

        zs_ast_expr_t* init = NULL;
        if (match(p, TOK_EQ)) {
            init = zs_parser_parse_expr(p);
        }
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras anclaje");

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_ANCHOR_DECL, loc);
        s->anchor_decl.name = name;
        s->anchor_decl.elem_type = elem_type;
        s->anchor_decl.size_expr = size_e;
        s->anchor_decl.init_expr = init;
        return s;
    }

    // defer
    if (match(p, TOK_KW_DEFER)) {
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_DEFER, loc);
        if (check(p, TOK_LBRACE)) {
            s->defer_stmt.block = parse_block(p);
        } else {
            s->defer_stmt.single_stmt = zs_parser_parse_stmt(p);
        }
        return s;
    }

    // if
    if (match(p, TOK_KW_IF)) {
        zs_ast_expr_t* cond = zs_parser_parse_expr(p);
        zs_ast_block_t* then_b = parse_block(p);
        zs_ast_block_t* else_b = NULL;
        zs_ast_stmt_t* else_if = NULL;

        if (match(p, TOK_KW_ELSE)) {
            if (check(p, TOK_KW_IF)) {
                else_if = zs_parser_parse_stmt(p);
            } else {
                else_b = parse_block(p);
            }
        }

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_IF, loc);
        s->if_stmt.cond = cond;
        s->if_stmt.then_block = then_b;
        s->if_stmt.else_block = else_b;
        s->if_stmt.else_if = else_if;
        return s;
    }

    // loop
    if (match(p, TOK_KW_LOOP)) {
        zs_ast_block_t* body = parse_block(p);
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_LOOP, loc);
        s->loop_stmt.body = body;
        return s;
    }

    // while
    if (match(p, TOK_KW_WHILE)) {
        zs_ast_expr_t* cond = zs_parser_parse_expr(p);
        zs_ast_block_t* body = parse_block(p);
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_WHILE, loc);
        s->while_stmt.cond = cond;
        s->while_stmt.body = body;
        return s;
    }

    // walk p in buffer [by step]
    if (match(p, TOK_KW_WALK)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba variable de iteración tras 'walk'");
            return NULL;
        }
        const char* var_name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);
        consume(p, TOK_KW_IN, "Se esperaba 'in' tras variable de walk");
        zs_ast_expr_t* iter = zs_parser_parse_expr(p);
        zs_ast_expr_t* step = NULL;
        if (match(p, TOK_KW_BY)) {
            step = zs_parser_parse_expr(p);
        }
        zs_ast_block_t* body = parse_block(p);

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_WALK, loc);
        s->walk_stmt.var_name = var_name;
        s->walk_stmt.iterable = iter;
        s->walk_stmt.step = step;
        s->walk_stmt.body = body;
        return s;
    }

    // branch expr { ... }
    if (match(p, TOK_KW_BRANCH)) {
        zs_ast_expr_t* target = zs_parser_parse_expr(p);
        consume(p, TOK_LBRACE, "Se esperaba '{' tras objetivo de branch");

        u32 cap = 4, count = 0;
        zs_ast_branch_arm_t* arms = (zs_ast_branch_arm_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_branch_arm_t), sizeof(void*));

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (count >= cap) {
                cap *= 2;
                zs_ast_branch_arm_t* na = (zs_ast_branch_arm_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_branch_arm_t), sizeof(void*));
                memcpy(na, arms, count * sizeof(zs_ast_branch_arm_t));
                arms = na;
            }

            // Parse pattern
            zs_ast_pattern_t pat;
            memset(&pat, 0, sizeof(pat));

            if (match(p, TOK_KW_ELSE)) {
                pat.kind = PAT_ELSE;
            } else {
                pat.expr1 = zs_parser_parse_expr(p);
                if (match(p, TOK_DOT_DOT)) {
                    pat.kind = PAT_RANGE;
                    pat.expr2 = zs_parser_parse_expr(p);
                } else {
                    pat.kind = PAT_LITERAL;
                }
            }

            consume(p, TOK_FAT_ARROW, "Se esperaba '=>' tras patrón de branch");

            if (check(p, TOK_LBRACE)) {
                arms[count].block = parse_block(p);
                arms[count].single_stmt = NULL;
            } else {
                arms[count].block = NULL;
                arms[count].single_stmt = zs_parser_parse_stmt(p);
            }
            arms[count].pattern = pat;
            count++;
        }
        consume(p, TOK_RBRACE, "Se esperaba '}' tras arms de branch");

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_BRANCH, loc);
        s->branch_stmt.target = target;
        s->branch_stmt.arms = arms;
        s->branch_stmt.arm_count = count;
        return s;
    }

    // return
    if (match(p, TOK_KW_RETURN)) {
        zs_ast_expr_t* val = NULL;
        if (!check(p, TOK_SEMICOLON)) {
            val = zs_parser_parse_expr(p);
        }
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras return");
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_RETURN, loc);
        s->return_expr = val;
        return s;
    }

    // fail
    if (match(p, TOK_KW_FAIL)) {
        zs_ast_expr_t* val = zs_parser_parse_expr(p);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras fail");
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_FAIL, loc);
        s->fail_expr = val;
        return s;
    }

    // trap
    if (match(p, TOK_KW_TRAP)) {
        zs_ast_expr_t* val = NULL;
        if (!check(p, TOK_SEMICOLON)) {
            val = zs_parser_parse_expr(p);
        }
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras trap");
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_TRAP, loc);
        s->trap_expr = val;
        return s;
    }

    // break / continue
    if (match(p, TOK_KW_BREAK)) {
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras break");
        return zs_ast_stmt_create(p->arena, STMT_BREAK, loc);
    }
    if (match(p, TOK_KW_CONTINUE)) {
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras continue");
        return zs_ast_stmt_create(p->arena, STMT_CONTINUE, loc);
    }

    // raw { ... }
    if (match(p, TOK_KW_RAW)) {
        zs_ast_block_t* blk = parse_block(p);
        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_RAW, loc);
        s->raw_block = blk;
        return s;
    }

    // Expresión o Asignación
    zs_ast_expr_t* expr = zs_parser_parse_expr(p);

    // Asignación simple o compuesta (=, +=, -=, etc.)
    if (p->current.kind == TOK_EQ || p->current.kind == TOK_PLUS_EQ ||
        p->current.kind == TOK_MINUS_EQ || p->current.kind == TOK_STAR_EQ ||
        p->current.kind == TOK_SLASH_EQ || p->current.kind == TOK_PERCENT_EQ ||
        p->current.kind == TOK_AMP_EQ || p->current.kind == TOK_PIPE_EQ ||
        p->current.kind == TOK_CARET_EQ || p->current.kind == TOK_LSHIFT_EQ ||
        p->current.kind == TOK_RSHIFT_EQ) {

        zs_token_kind_t op = p->current.kind;
        advance(p);
        zs_ast_expr_t* rhs = zs_parser_parse_expr(p);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras asignación");

        zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_ASSIGN, loc);
        s->assign.lvalue = expr;
        s->assign.op = op;
        s->assign.value = rhs;
        return s;
    }

    consume(p, TOK_SEMICOLON, "Se esperaba ';' tras sentencia de expresión");
    zs_ast_stmt_t* s = zs_ast_stmt_create(p->arena, STMT_EXPR, loc);
    s->expr = expr;
    return s;
}

// --- PARSER DE DECLARACIONES DE ALTO NIVEL ---
static zs_ast_fn_sig_t parse_fn_signature(zs_parser_t* p) {
    zs_ast_fn_sig_t sig;
    memset(&sig, 0, sizeof(sig));

    consume(p, TOK_KW_FN, "Se esperaba 'fn'");
    if (!check(p, TOK_IDENT)) {
        error_at_current(p, "Se esperaba nombre de función");
        return sig;
    }
    sig.name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
    advance(p);

    consume(p, TOK_LPAREN, "Se esperaba '(' tras nombre de función");

    u32 cap = 4, count = 0;
    zs_ast_param_t* params = (zs_ast_param_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_param_t), sizeof(void*));

    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        if (count >= cap) {
            cap *= 2;
            zs_ast_param_t* np = (zs_ast_param_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_param_t), sizeof(void*));
            memcpy(np, params, count * sizeof(zs_ast_param_t));
            params = np;
        }

        bool is_var = match(p, TOK_KW_VAR);
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de parámetro");
            while (!check(p, TOK_COMMA) && !check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                advance(p);
            }
        } else {
            params[count].name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            params[count].is_var = is_var;
            advance(p);
            consume(p, TOK_COLON, "Se esperaba ':' tras parámetro");
            params[count].type = zs_parser_parse_type(p);
            count++;
        }

        if (!match(p, TOK_COMMA)) break;
    }
    if (!consume(p, TOK_RPAREN, "Se esperaba ')' tras parámetros")) {
        while (!check(p, TOK_RPAREN) && !check(p, TOK_LBRACE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
            advance(p);
        }
        match(p, TOK_RPAREN);
    }


    sig.params = params;
    sig.param_count = count;

    if (match(p, TOK_ARROW) || match(p, TOK_COLON)) {
        sig.return_type = zs_parser_parse_type(p);
    } else {
        sig.return_type = zs_ast_type_primitive(p->arena, p->current.loc, TOK_KW_VOID);
    }

    if (match(p, TOK_KW_OR)) {
        sig.error_type = zs_parser_parse_type(p);
    }

    return sig;
}

zs_ast_decl_t* zs_parser_parse_decl(zs_parser_t* p) {
    zs_loc_t loc = p->current.loc;
    bool is_pub = match(p, TOK_KW_PUB);

    // Función
    if (check(p, TOK_KW_FN)) {
        zs_ast_fn_sig_t sig = parse_fn_signature(p);
        zs_ast_block_t* body = parse_block(p);

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_FUNCTION, loc, is_pub);
        d->function.name = sig.name;
        d->function.params = sig.params;
        d->function.param_count = sig.param_count;
        d->function.return_type = sig.return_type;
        d->function.error_type = sig.error_type;
        d->function.body = body;
        return d;
    }

    // record Nombre [align(N)] { campo: Tipo; ... }
    if (match(p, TOK_KW_RECORD)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de record");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);

        u32 alignment = 0;
        if (match(p, TOK_KW_ALIGN)) {
            consume(p, TOK_LPAREN, "Se esperaba '(' tras align");
            if (check(p, TOK_INT_LIT)) {
                alignment = (u32)p->current.u64_val;
                advance(p);
            }
            consume(p, TOK_RPAREN, "Se esperaba ')' tras valor de align");
        }

        consume(p, TOK_LBRACE, "Se esperaba '{' al inicio del record");

        u32 cap = 8, count = 0;
        zs_ast_record_field_t* fields = (zs_ast_record_field_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_record_field_t), sizeof(void*));

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (count >= cap) {
                cap *= 2;
                zs_ast_record_field_t* nf = (zs_ast_record_field_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_record_field_t), sizeof(void*));
                memcpy(nf, fields, count * sizeof(zs_ast_record_field_t));
                fields = nf;
            }

            bool f_pub = match(p, TOK_KW_PUB);
            if (!check(p, TOK_IDENT)) {
                error_at_current(p, "Se esperaba nombre de campo de record");
                break;
            }
            fields[count].name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            fields[count].is_pub = f_pub;
            advance(p);
            consume(p, TOK_COLON, "Se esperaba ':' tras nombre de campo");
            fields[count].type = zs_parser_parse_type(p);
            consume(p, TOK_SEMICOLON, "Se esperaba ';' tras campo de record");
            count++;
        }
        consume(p, TOK_RBRACE, "Se esperaba '}' al cerrar record");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_RECORD, loc, is_pub);
        d->record.name = name;
        d->record.alignment = alignment;
        d->record.fields = fields;
        d->record.field_count = count;
        return d;
    }

    // choice Nombre { Variante(Tipos); ... }
    if (match(p, TOK_KW_CHOICE)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de choice");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);

        consume(p, TOK_LBRACE, "Se esperaba '{' en choice");

        u32 cap = 4, count = 0;
        zs_ast_choice_variant_t* variants = (zs_ast_choice_variant_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_choice_variant_t), sizeof(void*));

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (count >= cap) {
                cap *= 2;
                zs_ast_choice_variant_t* nv = (zs_ast_choice_variant_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_choice_variant_t), sizeof(void*));
                memcpy(nv, variants, count * sizeof(zs_ast_choice_variant_t));
                variants = nv;
            }

            if (!check(p, TOK_IDENT)) {
                error_at_current(p, "Se esperaba nombre de variante");
                break;
            }
            variants[count].name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
            advance(p);

            variants[count].payload_count = 0;
            variants[count].payload_types = NULL;

            if (match(p, TOK_LPAREN)) {
                u32 pcap = 2, pcount = 0;
                zs_ast_type_t** ptypes = (zs_ast_type_t**)zs_arena_alloc(p->arena, pcap * sizeof(void*), sizeof(void*));
                while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
                    if (pcount >= pcap) {
                        pcap *= 2;
                        zs_ast_type_t** npt = (zs_ast_type_t**)zs_arena_alloc(p->arena, pcap * sizeof(void*), sizeof(void*));
                        memcpy(npt, ptypes, pcount * sizeof(void*));
                        ptypes = npt;
                    }
                    ptypes[pcount++] = zs_parser_parse_type(p);
                    if (!match(p, TOK_COMMA)) break;
                }
                consume(p, TOK_RPAREN, "Se esperaba ')' tras tipos de variante");
                variants[count].payload_types = ptypes;
                variants[count].payload_count = pcount;
            }

            consume(p, TOK_SEMICOLON, "Se esperaba ';' tras variante de choice");
            count++;
        }
        consume(p, TOK_RBRACE, "Se esperaba '}' al cerrar choice");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_CHOICE, loc, is_pub);
        d->choice.name = name;
        d->choice.variants = variants;
        d->choice.variant_count = count;
        return d;
    }

    // contract Nombre { fn firma(); ... }
    if (match(p, TOK_KW_CONTRACT)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de contract");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);
        consume(p, TOK_LBRACE, "Se esperaba '{' en contract");

        u32 cap = 4, count = 0;
        zs_ast_fn_sig_t* methods = (zs_ast_fn_sig_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_fn_sig_t), sizeof(void*));

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (count >= cap) {
                cap *= 2;
                zs_ast_fn_sig_t* nm = (zs_ast_fn_sig_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_fn_sig_t), sizeof(void*));
                memcpy(nm, methods, count * sizeof(zs_ast_fn_sig_t));
                methods = nm;
            }
            methods[count++] = parse_fn_signature(p);
            if (!consume(p, TOK_SEMICOLON, "Se esperaba ';' tras método en contract")) {
                while (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    advance(p);
                }
                match(p, TOK_SEMICOLON);
            }
        }
        consume(p, TOK_RBRACE, "Se esperaba '}' al cerrar contract");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_CONTRACT, loc, is_pub);
        d->contract.name = name;
        d->contract.methods = methods;
        d->contract.method_count = count;
        return d;
    }

    // foreign "C" { ... }
    if (match(p, TOK_KW_FOREIGN)) {
        const char* abi = "C";
        if (check(p, TOK_STRING_LIT)) {
            abi = p->current.string_val.str;
            advance(p);
        }
        consume(p, TOK_LBRACE, "Se esperaba '{' en bloque foreign");

        u32 cap = 4, count = 0;
        zs_ast_fn_sig_t* fns = (zs_ast_fn_sig_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_fn_sig_t), sizeof(void*));

        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (count >= cap) {
                cap *= 2;
                zs_ast_fn_sig_t* nfn = (zs_ast_fn_sig_t*)zs_arena_alloc(p->arena, cap * sizeof(zs_ast_fn_sig_t), sizeof(void*));
                memcpy(nfn, fns, count * sizeof(zs_ast_fn_sig_t));
                fns = nfn;
            }
            fns[count++] = parse_fn_signature(p);
            if (!consume(p, TOK_SEMICOLON, "Se esperaba ';' tras firma en foreign")) {
                while (!check(p, TOK_SEMICOLON) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    advance(p);
                }
                match(p, TOK_SEMICOLON);
            }
        }

        consume(p, TOK_RBRACE, "Se esperaba '}' tras foreign");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_FOREIGN, loc, is_pub);
        d->foreign_block.abi_name = abi;
        d->foreign_block.functions = fns;
        d->foreign_block.function_count = count;
        return d;
    }

    // const
    if (match(p, TOK_KW_CONST)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de constante");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);

        zs_ast_type_t* type = NULL;
        if (match(p, TOK_COLON)) {
            type = zs_parser_parse_type(p);
        }
        consume(p, TOK_EQ, "Se esperaba '=' en declaración const");
        zs_ast_expr_t* val = zs_parser_parse_expr(p);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras const");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_CONST, loc, is_pub);
        d->const_decl.name = name;
        d->const_decl.type = type;
        d->const_decl.value = val;
        return d;
    }

    // type Alias = Tipo;
    if (match(p, TOK_KW_TYPE)) {
        if (!check(p, TOK_IDENT)) {
            error_at_current(p, "Se esperaba nombre de alias");
            return NULL;
        }
        const char* name = zs_arena_strndup(p->arena, p->current.start, p->current.length);
        advance(p);
        consume(p, TOK_EQ, "Se esperaba '=' en type alias");
        zs_ast_type_t* target = zs_parser_parse_type(p);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras type alias");

        zs_ast_decl_t* d = zs_ast_decl_create(p->arena, DECL_TYPE_ALIAS, loc, is_pub);
        d->type_alias.name = name;
        d->type_alias.target_type = target;
        return d;
    }

    error_at_current(p, "Declaración de alto nivel esperada");
    synchronize(p);
    return NULL;
}

zs_ast_program_t* zs_parser_parse_program(zs_parser_t* p) {
    zs_ast_program_t* prog = (zs_ast_program_t*)zs_arena_alloc_zero(p->arena, sizeof(zs_ast_program_t), sizeof(void*));

    // Cabecera de módulo opcional: module nombre::espacio;
    if (match(p, TOK_KW_MODULE)) {
        // Concatenar ruta
        u32 start = p->current.loc.offset;
        while (!check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
            advance(p);
        }
        u32 len = p->previous.loc.offset + p->previous.length - start;
        prog->module_name = zs_arena_strndup(p->arena, p->lexer->file->content + start, len);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras module");
    }

    // Directivas use
    u32 cap_imports = 4, import_count = 0;
    prog->imports = (const char**)zs_arena_alloc(p->arena, cap_imports * sizeof(char*), sizeof(void*));

    while (match(p, TOK_KW_USE)) {
        u32 start = p->current.loc.offset;
        while (!check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
            advance(p);
        }
        u32 len = p->previous.loc.offset + p->previous.length - start;
        const char* imp = zs_arena_strndup(p->arena, p->lexer->file->content + start, len);
        consume(p, TOK_SEMICOLON, "Se esperaba ';' tras use");

        if (import_count >= cap_imports) {
            cap_imports *= 2;
            const char** ni = (const char**)zs_arena_alloc(p->arena, cap_imports * sizeof(char*), sizeof(void*));
            memcpy(ni, prog->imports, import_count * sizeof(char*));
            prog->imports = ni;
        }
        prog->imports[import_count++] = imp;
    }
    prog->import_count = import_count;

    // Declaraciones de alto nivel
    u32 cap_decls = 16, decl_count = 0;
    prog->decls = (zs_ast_decl_t**)zs_arena_alloc(p->arena, cap_decls * sizeof(void*), sizeof(void*));

    while (!check(p, TOK_EOF)) {
        zs_ast_decl_t* d = zs_parser_parse_decl(p);
        if (d) {
            if (decl_count >= cap_decls) {
                cap_decls *= 2;
                zs_ast_decl_t** nd = (zs_ast_decl_t**)zs_arena_alloc(p->arena, cap_decls * sizeof(void*), sizeof(void*));
                memcpy(nd, prog->decls, decl_count * sizeof(void*));
                prog->decls = nd;
            }
            prog->decls[decl_count++] = d;
        } else {
            if (!check(p, TOK_EOF)) {
                advance(p);
            }
        }
    }

    prog->decl_count = decl_count;

    return prog;
}
