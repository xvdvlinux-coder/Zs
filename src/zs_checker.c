#include "zs_checker.h"
#include <stdarg.h>

static void checker_error(zs_checker_t* c, zs_loc_t loc, const char* fmt, ...) {
    c->error_count++;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31merror semántico:\033[0m \033[1m%s:%u:%u:\033[0m ",
            c->file ? c->file->filename : "<desconocido>", loc.line, loc.col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void zs_checker_init(zs_checker_t* checker, const zs_source_file_t* file, zs_arena_t* arena) {
    checker->arena = arena;
    checker->symtab = zs_symtab_create(arena);
    checker->file = file;
    checker->error_count = 0;
    checker->current_fn_ret_type = NULL;
    checker->current_fn_err_type = NULL;
}

// Convertir nodo AST de tipo a tipo concreto
static zs_type_t* resolve_ast_type(zs_checker_t* c, zs_ast_type_t* at) {
    if (!at) return zs_type_get_primitive(c->arena, TOK_KW_VOID);

    switch (at->kind) {
        case TYPE_KIND_PRIMITIVE:
            return zs_type_get_primitive(c->arena, at->primitive_token);

        case TYPE_KIND_LOC: {
            zs_type_t* sub = resolve_ast_type(c, at->sub_type);
            return zs_type_make_loc(c->arena, sub);
        }

        case TYPE_KIND_VIEW: {
            zs_type_t* sub = resolve_ast_type(c, at->sub_type);
            return zs_type_make_view(c->arena, sub);
        }

        case TYPE_KIND_ARRAY: {
            zs_type_t* elem = resolve_ast_type(c, at->array.elem_type);
            usize count = 0;
            if (at->array.size_expr && at->array.size_expr->kind == EXPR_INT_LIT) {
                count = (usize)at->array.size_expr->int_val;
            }
            zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_type_t), sizeof(void*));
            t->category = ZS_TYPE_ARRAY;
            t->array.elem_type = elem;
            t->array.count = count;
            t->alignment = elem->alignment;
            t->size = elem->size * count;
            return t;
        }

        case TYPE_KIND_NAMED: {
            const char* name = at->named.parts[0];
            zs_symbol_t* sym = zs_symtab_lookup(c->symtab, name);
            if (!sym || sym->kind != SYM_TYPE) {
                checker_error(c, at->loc, "Tipo desconocido: '%s'", name);
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }
            return sym->type;
        }

        default:
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);
    }
}

static bool is_integer_type(const zs_type_t* t) {
    if (!t || t->category != ZS_TYPE_PRIMITIVE) return false;
    switch (t->primitive_kind) {
        case TOK_KW_I8: case TOK_KW_I16: case TOK_KW_I32: case TOK_KW_I64: case TOK_KW_ISIZE:
        case TOK_KW_U8: case TOK_KW_U16: case TOK_KW_U32: case TOK_KW_U64: case TOK_KW_USIZE:
            return true;
        default:
            return false;
    }
}

// Analizador de expresiones
zs_type_t* zs_checker_check_expr(zs_checker_t* c, zs_ast_expr_t* expr) {
    if (!expr) return zs_type_get_primitive(c->arena, TOK_KW_VOID);

    switch (expr->kind) {
        case EXPR_INT_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_I32);

        case EXPR_FLOAT_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_F64);

        case EXPR_STRING_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_STR);

        case EXPR_CHAR_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_CHAR32);

        case EXPR_BOOL_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_BOOL);

        case EXPR_NONE_LIT:
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);

        case EXPR_IDENT: {
            zs_symbol_t* sym = zs_symtab_lookup(c->symtab, expr->ident);
            if (!sym) {
                checker_error(c, expr->loc, "Identificador no declarado: '%s'", expr->ident);
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }
            if (!sym->is_initialized) {
                checker_error(c, expr->loc, "Uso de variable no inicializada: '%s'", expr->ident);
            }
            return sym->type;
        }

        case EXPR_UNARY: {
            zs_type_t* sub = zs_checker_check_expr(c, expr->unary.expr);
            if (expr->unary.op == TOK_BANG) {
                if (sub->primitive_kind != TOK_KW_BOOL) {
                    checker_error(c, expr->loc, "Operador '!' requiere tipo bool");
                }
                return zs_type_get_primitive(c->arena, TOK_KW_BOOL);
            }
            if (expr->unary.op == TOK_MINUS) {
                if (!is_integer_type(sub) && sub->primitive_kind != TOK_KW_F32 && sub->primitive_kind != TOK_KW_F64) {
                    checker_error(c, expr->loc, "Operador '-' unario requiere tipo numérico");
                }
                return sub;
            }
            if (expr->unary.op == TOK_STAR || expr->unary.op == TOK_AT) {
                if (sub->category != ZS_TYPE_LOC) {
                    checker_error(c, expr->loc, "Operador de desreferencia requiere tipo loc<T>");
                    return zs_type_get_primitive(c->arena, TOK_KW_VOID);
                }
                return sub->sub_type;
            }
            return sub;
        }

        case EXPR_CAST: {
            zs_checker_check_expr(c, expr->cast.expr);
            return resolve_ast_type(c, expr->cast.target_type);
        }

        case EXPR_BINARY: {
            zs_token_kind_t op = expr->binary.op;
            zs_type_t* left = zs_checker_check_expr(c, expr->binary.left);
            zs_type_t* right = zs_checker_check_expr(c, expr->binary.right);

            // Operadores TAD
            if (op == TOK_TAD_FWD || op == TOK_TAD_BWD || op == TOK_TAD_RING || op == TOK_TAD_SAFE) {
                if (left->category != ZS_TYPE_LOC) {
                    checker_error(c, expr->loc, "El operando izquierdo de navegación TAD debe ser de tipo loc<T>");
                }
                if (!is_integer_type(right)) {
                    checker_error(c, expr->loc, "El desplazamiento TAD debe ser de tipo entero");
                }
                return left;
            }

            // Desplazamiento por bytes físicos: ~>#
            if (op == TOK_TAD_BYTE) {
                if (left->category != ZS_TYPE_LOC) {
                    checker_error(c, expr->loc, "El operando izquierdo de ~># debe ser de tipo loc<T>");
                }
                if (!is_integer_type(right)) {
                    checker_error(c, expr->loc, "El desplazamiento en bytes de ~># debe ser entero");
                }
                // REGLA DE CONGRUENCIA DE FASE: bytes % alignof(T) == 0
                if (expr->binary.right->kind == EXPR_INT_LIT) {
                    usize bytes = (usize)expr->binary.right->int_val;
                    usize elem_align = left->sub_type ? left->sub_type->alignment : 1;
                    if (elem_align > 0 && (bytes % elem_align) != 0) {
                        checker_error(c, expr->loc, "Violación de regla de fase de alineación: %zu bytes no es múltiplo de alignof(%zu)",
                                      bytes, elem_align);
                    }
                }
                return left;
            }

            // Métrica de distancia: <=>
            if (op == TOK_TAD_DIST) {
                if (left->category != ZS_TYPE_LOC || right->category != ZS_TYPE_LOC) {
                    checker_error(c, expr->loc, "Ambos operandos de métrica <=> deben ser de tipo loc<T>");
                }
                if (!zs_type_equals(left->sub_type, right->sub_type)) {
                    checker_error(c, expr->loc, "Métrica <=> requiere coordenadas del mismo tipo de elemento");
                }

                // COMPROBACIÓN DE IDENTIDAD DE ANCLAJE:
                if (expr->binary.left->kind == EXPR_IDENT && expr->binary.right->kind == EXPR_IDENT) {
                    zs_symbol_t* s1 = zs_symtab_lookup(c->symtab, expr->binary.left->ident);
                    zs_symbol_t* s2 = zs_symtab_lookup(c->symtab, expr->binary.right->ident);
                    if (s1 && s2 && s1->anchor_parent && s2->anchor_parent) {
                        if (s1->anchor_parent != s2->anchor_parent) {
                            checker_error(c, expr->loc, "Violación métrica: prohibido calcular distancia entre coordenadas de anclajes distintos ('%s' y '%s')",
                                          s1->anchor_parent->name, s2->anchor_parent->name);
                        }
                    }
                }
                return zs_type_get_primitive(c->arena, TOK_KW_ISIZE);
            }

            // Operadores relacionales estándar: <, <=, >, >=
            if (op == TOK_LESS || op == TOK_LESS_EQ || op == TOK_GREATER || op == TOK_GREATER_EQ) {
                if (!zs_type_equals(left, right)) {
                    checker_error(c, expr->loc, "Comparación entre tipos incompatibles sin cast explícito (%s y %s)",
                                  zs_type_to_string(c->arena, left), zs_type_to_string(c->arena, right));
                }
                return zs_type_get_primitive(c->arena, TOK_KW_BOOL);
            }

            // Operadores de igualdad: ==, !=
            if (op == TOK_EQ_EQ || op == TOK_BANG_EQ) {
                if (!zs_type_equals(left, right)) {
                    checker_error(c, expr->loc, "Comparación de igualdad entre tipos incompatibles (%s y %s)",
                                  zs_type_to_string(c->arena, left), zs_type_to_string(c->arena, right));
                }
                return zs_type_get_primitive(c->arena, TOK_KW_BOOL);
            }

            // Operadores lógicos: &&, ||
            if (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE) {
                if (left->primitive_kind != TOK_KW_BOOL || right->primitive_kind != TOK_KW_BOOL) {
                    checker_error(c, expr->loc, "Operadores lógicos && y || requieren operandos bool");
                }
                return zs_type_get_primitive(c->arena, TOK_KW_BOOL);
            }

            // Operaciones aritméticas y de bits convencionales
            // REGLA ESTRICTA: CERO COERCIÓN IMPLÍCITA
            if (!zs_type_equals(left, right)) {
                checker_error(c, expr->loc, "Operación aritmética entre tipos distintos sin cast explícito 'as': %s y %s",
                              zs_type_to_string(c->arena, left), zs_type_to_string(c->arena, right));
            }
            return left;
        }

        case EXPR_CALL: {
            zs_symbol_t* fn_sym = NULL;
            if (expr->call.callee->kind == EXPR_IDENT) {
                fn_sym = zs_symtab_lookup(c->symtab, expr->call.callee->ident);
            }

            if (!fn_sym || fn_sym->kind != SYM_FN) {
                checker_error(c, expr->loc, "Llamada a símbolo que no es una función");
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }

            zs_type_t* fn_type = fn_sym->type;
            if (expr->call.arg_count != fn_type->function.param_count) {
                checker_error(c, expr->loc, "Número incorrecto de argumentos en llamada a '%s': esperados %u, recibidos %u",
                              fn_sym->name, fn_type->function.param_count, expr->call.arg_count);
            }

            for (u32 i = 0; i < expr->call.arg_count && i < fn_type->function.param_count; ++i) {
                zs_type_t* arg_t = zs_checker_check_expr(c, expr->call.args[i]);
                if (!zs_type_equals(arg_t, fn_type->function.param_types[i])) {
                    checker_error(c, expr->call.args[i]->loc, "Tipo de argumento %u incompatible: esperado %s, recibido %s",
                                  i + 1, zs_type_to_string(c->arena, fn_type->function.param_types[i]),
                                  zs_type_to_string(c->arena, arg_t));
                }
            }

            return fn_type->function.return_type;
        }

        case EXPR_FIELD: {
            zs_type_t* target = zs_checker_check_expr(c, expr->field.target);
            if (target->category == ZS_TYPE_ANCHOR) {
                if (strcmp(expr->field.field_name, "start") == 0 ||
                    strcmp(expr->field.field_name, "end") == 0) {
                    return zs_type_make_loc(c->arena, target->anchor.elem_type);
                }
                if (strcmp(expr->field.field_name, "len") == 0) {
                    return zs_type_get_primitive(c->arena, TOK_KW_USIZE);
                }
                checker_error(c, expr->loc, "Campo desconocido '%s' en tipo anclaje", expr->field.field_name);
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }
            if ((target->category == ZS_TYPE_PRIMITIVE && target->primitive_kind == TOK_KW_STR) ||
                target->category == ZS_TYPE_VIEW) {
                if (strcmp(expr->field.field_name, "len") == 0) {
                    return zs_type_get_primitive(c->arena, TOK_KW_USIZE);
                }
            }
            if (target->category != ZS_TYPE_RECORD) {
                checker_error(c, expr->loc, "Acceso a campo en un tipo que no es record");
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }
            for (u32 i = 0; i < target->record.field_count; ++i) {
                if (strcmp(target->record.fields[i].name, expr->field.field_name) == 0) {
                    return target->record.fields[i].type;
                }
            }
            checker_error(c, expr->loc, "Campo '%s' no existe en record '%s'",
                          expr->field.field_name, target->record.name);
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);
        }

        case EXPR_ARRAY_LIT: {
            if (expr->array_lit.count == 0) {
                return zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }
            zs_type_t* elem_t = zs_checker_check_expr(c, expr->array_lit.elems[0]);
            for (u32 i = 1; i < expr->array_lit.count; ++i) {
                zs_type_t* next_t = zs_checker_check_expr(c, expr->array_lit.elems[i]);
                if (!zs_type_equals(elem_t, next_t)) {
                    checker_error(c, expr->array_lit.elems[i]->loc, "Tipo heterogéneo en literal de array");
                }
            }
            zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_type_t), sizeof(void*));
            t->category = ZS_TYPE_ARRAY;
            t->array.elem_type = elem_t;
            t->array.count = expr->array_lit.count;
            t->alignment = elem_t->alignment;
            t->size = elem_t->size * t->array.count;
            return t;
        }

        case EXPR_INDEX: {
            zs_type_t* target = zs_checker_check_expr(c, expr->index.target);
            zs_type_t* idx = zs_checker_check_expr(c, expr->index.index);
            if (!is_integer_type(idx)) {
                checker_error(c, expr->loc, "El índice debe ser de tipo entero");
            }
            if (target->category == ZS_TYPE_ARRAY) return target->array.elem_type;
            if (target->category == ZS_TYPE_ANCHOR) return target->anchor.elem_type;
            if (target->category == ZS_TYPE_VIEW) return target->sub_type;
            if (target->category == ZS_TYPE_LOC) return target->sub_type;
            checker_error(c, expr->loc, "Tipo no indexable: %s", zs_type_to_string(c->arena, target));
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);
        }

        case EXPR_SLICE: {
            zs_type_t* target = zs_checker_check_expr(c, expr->slice.target);
            zs_type_t* elem_t = NULL;
            if (target->category == ZS_TYPE_ARRAY) elem_t = target->array.elem_type;
            else if (target->category == ZS_TYPE_ANCHOR) elem_t = target->anchor.elem_type;
            else if (target->category == ZS_TYPE_VIEW) elem_t = target->sub_type;
            else {
                checker_error(c, expr->loc, "Tipo no admite slice [..]: %s", zs_type_to_string(c->arena, target));
                elem_t = zs_type_get_primitive(c->arena, TOK_KW_U8);
            }
            if (expr->slice.start) zs_checker_check_expr(c, expr->slice.start);
            if (expr->slice.end) zs_checker_check_expr(c, expr->slice.end);
            return zs_type_make_view(c->arena, elem_t);
        }

        case EXPR_RETURN_FAIL:
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);

        case EXPR_CATCH: {
            zs_type_t* prim = zs_checker_check_expr(c, expr->catch_expr.primary);
            zs_symtab_enter_scope(c->symtab);
            if (expr->catch_expr.err_var) {
                zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                sym->name = expr->catch_expr.err_var;
                sym->kind = SYM_VAR;
                sym->type = c->current_fn_err_type ? c->current_fn_err_type : zs_type_get_primitive(c->arena, TOK_KW_I32);
                sym->is_mutable = false;
                sym->is_initialized = true;
                zs_symtab_insert(c->symtab, sym);
            }
            if (expr->catch_expr.catch_block) {
                for (u32 i = 0; i < expr->catch_expr.catch_block->stmt_count; ++i) {
                    zs_checker_check_stmt(c, expr->catch_expr.catch_block->stmts[i]);
                }
            }
            zs_symtab_exit_scope(c->symtab);
            return prim;
        }

        case EXPR_OR_FALLBACK: {
            zs_type_t* prim = zs_checker_check_expr(c, expr->or_fallback.primary);
            if (expr->or_fallback.fallback->kind == EXPR_RETURN_FAIL) {
                return prim;
            }
            zs_type_t* fall = zs_checker_check_expr(c, expr->or_fallback.fallback);
            if (!zs_type_equals(prim, fall)) {
                checker_error(c, expr->loc, "Tipos incompatibles en operador 'or': %s frente a %s",
                              zs_type_to_string(c->arena, prim), zs_type_to_string(c->arena, fall));
            }
            return prim;
        }

        default:
            return zs_type_get_primitive(c->arena, TOK_KW_VOID);
    }
}

// Analizador de sentencias
void zs_checker_check_stmt(zs_checker_t* c, zs_ast_stmt_t* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_VAL_DECL:
        case STMT_VAR_DECL: {
            bool is_var = (stmt->kind == STMT_VAR_DECL);
            zs_type_t* declared_type = stmt->var_decl.type ? resolve_ast_type(c, stmt->var_decl.type) : NULL;
            zs_type_t* init_type = NULL;
            zs_symbol_t* anchor_parent = NULL;

            if (stmt->var_decl.init) {
                init_type = zs_checker_check_expr(c, stmt->var_decl.init);

                // Si la inicialización es arr.start o similar, rastrear anchor
                if (stmt->var_decl.init->kind == EXPR_FIELD && stmt->var_decl.init->field.target->kind == EXPR_IDENT) {
                    zs_symbol_t* asym = zs_symtab_lookup(c->symtab, stmt->var_decl.init->field.target->ident);
                    if (asym && asym->kind == SYM_ANCHOR) {
                        anchor_parent = asym;
                    }
                }
            }

            zs_type_t* final_type = declared_type ? declared_type : init_type;
            if (!final_type) {
                checker_error(c, stmt->loc, "Variable '%s' requiere tipo explícito o valor de inicialización", stmt->var_decl.name);
                final_type = zs_type_get_primitive(c->arena, TOK_KW_VOID);
            }

            if (declared_type && init_type && !zs_type_equals(declared_type, init_type)) {
                checker_error(c, stmt->loc, "Tipo de inicialización incompatible para '%s': esperado %s, recibido %s",
                              stmt->var_decl.name, zs_type_to_string(c->arena, declared_type),
                              zs_type_to_string(c->arena, init_type));
            }

            zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
            sym->name = stmt->var_decl.name;
            sym->kind = SYM_VAR;
            sym->type = final_type;
            sym->is_mutable = is_var;
            sym->is_initialized = (stmt->var_decl.init != NULL);
            sym->anchor_parent = anchor_parent;
            sym->def_loc = stmt->loc;

            if (!zs_symtab_insert(c->symtab, sym)) {
                checker_error(c, stmt->loc, "Redefinición de variable '%s' en el mismo ámbito", stmt->var_decl.name);
            }
            break;
        }

        case STMT_ANCHOR_DECL: {
            zs_type_t* elem = resolve_ast_type(c, stmt->anchor_decl.elem_type);
            usize count = 0;
            if (stmt->anchor_decl.size_expr && stmt->anchor_decl.size_expr->kind == EXPR_INT_LIT) {
                count = (usize)stmt->anchor_decl.size_expr->int_val;
            } else {
                checker_error(c, stmt->loc, "El tamaño del anclaje debe ser una constante entera positiva");
            }

            zs_type_t* anch_t = zs_type_make_anchor(c->arena, elem, count);
            zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
            sym->name = stmt->anchor_decl.name;
            sym->kind = SYM_ANCHOR;
            sym->type = anch_t;
            sym->is_mutable = true;
            sym->is_initialized = true;
            sym->def_loc = stmt->loc;

            zs_symtab_insert(c->symtab, sym);
            break;
        }

        case STMT_ASSIGN: {
            if (stmt->assign.lvalue->kind == EXPR_UNARY &&
                (stmt->assign.lvalue->unary.op == TOK_STAR || stmt->assign.lvalue->unary.op == TOK_AT)) {
                zs_type_t* target_t = zs_checker_check_expr(c, stmt->assign.lvalue);
                zs_type_t* val_type = zs_checker_check_expr(c, stmt->assign.value);
                if (!zs_type_equals(target_t, val_type)) {
                    checker_error(c, stmt->loc, "Tipo incompatible en asignación a coordenada desreferenciada");
                }
                break;
            }
            if (stmt->assign.lvalue->kind == EXPR_INDEX) {
                zs_type_t* target_t = zs_checker_check_expr(c, stmt->assign.lvalue);
                zs_type_t* val_type = zs_checker_check_expr(c, stmt->assign.value);
                if (!zs_type_equals(target_t, val_type)) {
                    checker_error(c, stmt->loc, "Tipo incompatible en asignación a elemento indexado");
                }
                break;
            }
            if (stmt->assign.lvalue->kind != EXPR_IDENT) {
                checker_error(c, stmt->loc, "El objetivo de la asignación debe ser un lvalue");
                break;
            }
            zs_symbol_t* sym = zs_symtab_lookup(c->symtab, stmt->assign.lvalue->ident);
            if (!sym) {
                checker_error(c, stmt->loc, "Variable no declarada: '%s'", stmt->assign.lvalue->ident);
                break;
            }

            // REGLA: INMUTABILIDAD ESTRICTA DE 'val'
            if (!sym->is_mutable) {
                checker_error(c, stmt->loc, "No se puede reasignar la variable inmutable 'val' '%s'", sym->name);
            }

            zs_type_t* val_type = zs_checker_check_expr(c, stmt->assign.value);
            if (!zs_type_equals(sym->type, val_type)) {
                checker_error(c, stmt->loc, "Tipo incompatible en asignación a '%s': esperado %s, recibido %s",
                              sym->name, zs_type_to_string(c->arena, sym->type),
                              zs_type_to_string(c->arena, val_type));
            }

            // REGLA DE NO-ESCAPE LÉXICO TAD: depth(loc) <= depth(anchor)
            if (stmt->assign.value->kind == EXPR_IDENT) {
                zs_symbol_t* src_sym = zs_symtab_lookup(c->symtab, stmt->assign.value->ident);
                if (src_sym && src_sym->anchor_parent) {
                    if (sym->scope_depth < src_sym->anchor_parent->scope_depth) {
                        checker_error(c, stmt->loc, "Violación de no-escape léxico: la coordenada escapa del ámbito de su anclaje '%s'",
                                      src_sym->anchor_parent->name);
                    }
                    sym->anchor_parent = src_sym->anchor_parent;
                }
            }

            sym->is_initialized = true;
            break;
        }

        case STMT_IF: {
            zs_type_t* cond_t = zs_checker_check_expr(c, stmt->if_stmt.cond);
            if (cond_t->primitive_kind != TOK_KW_BOOL) {
                checker_error(c, stmt->loc, "La condición de 'if' debe ser de tipo bool");
            }
            zs_symtab_enter_scope(c->symtab);
            if (stmt->if_stmt.then_block) {
                for (u32 i = 0; i < stmt->if_stmt.then_block->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->if_stmt.then_block->stmts[i]);
                }
            }
            zs_symtab_exit_scope(c->symtab);

            if (stmt->if_stmt.else_block) {
                zs_symtab_enter_scope(c->symtab);
                for (u32 i = 0; i < stmt->if_stmt.else_block->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->if_stmt.else_block->stmts[i]);
                }
                zs_symtab_exit_scope(c->symtab);
            }
            if (stmt->if_stmt.else_if) {
                zs_checker_check_stmt(c, stmt->if_stmt.else_if);
            }
            break;
        }

        case STMT_BRANCH: {
            zs_type_t* target_t = zs_checker_check_expr(c, stmt->branch_stmt.target);
            bool has_else = false;

            for (u32 i = 0; i < stmt->branch_stmt.arm_count; ++i) {
                if (stmt->branch_stmt.arms[i].pattern.kind == PAT_ELSE) {
                    has_else = true;
                }
                zs_symtab_enter_scope(c->symtab);
                if (stmt->branch_stmt.arms[i].block) {
                    for (u32 j = 0; j < stmt->branch_stmt.arms[i].block->stmt_count; ++j) {
                        zs_checker_check_stmt(c, stmt->branch_stmt.arms[i].block->stmts[j]);
                    }
                }
                if (stmt->branch_stmt.arms[i].single_stmt) {
                    zs_checker_check_stmt(c, stmt->branch_stmt.arms[i].single_stmt);
                }
                zs_symtab_exit_scope(c->symtab);
            }

            // REGLA: EXHAUSTIVIDAD DE BRANCH
            if (target_t->category == ZS_TYPE_CHOICE) {
                if (!has_else && stmt->branch_stmt.arm_count < target_t->choice.variant_count) {
                    checker_error(c, stmt->loc, "Sentencia branch no exhaustiva para choice '%s': faltan variantes o cláusula else",
                                  target_t->choice.name);
                }
            } else {
                if (!has_else) {
                    checker_error(c, stmt->loc, "Sentencia branch no exhaustiva: se requiere una cláusula 'else'");
                }
            }
            break;
        }

        case STMT_LOOP:
            if (stmt->loop_stmt.body) {
                zs_symtab_enter_scope(c->symtab);
                for (u32 i = 0; i < stmt->loop_stmt.body->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->loop_stmt.body->stmts[i]);
                }
                zs_symtab_exit_scope(c->symtab);
            }
            break;

        case STMT_WHILE: {
            zs_type_t* cond_t = zs_checker_check_expr(c, stmt->while_stmt.cond);
            if (cond_t->primitive_kind != TOK_KW_BOOL) {
                checker_error(c, stmt->loc, "La condición de 'while' debe ser bool");
            }
            if (stmt->while_stmt.body) {
                zs_symtab_enter_scope(c->symtab);
                for (u32 i = 0; i < stmt->while_stmt.body->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->while_stmt.body->stmts[i]);
                }
                zs_symtab_exit_scope(c->symtab);
            }
            break;
        }

        case STMT_WALK: {
            zs_type_t* iter_t = zs_checker_check_expr(c, stmt->walk_stmt.iterable);
            zs_type_t* elem_t = NULL;
            if (iter_t->category == ZS_TYPE_ARRAY) {
                elem_t = iter_t->array.elem_type;
            } else if (iter_t->category == ZS_TYPE_ANCHOR) {
                elem_t = iter_t->anchor.elem_type;
            } else if (iter_t->category == ZS_TYPE_VIEW) {
                elem_t = iter_t->sub_type;
            } else {
                checker_error(c, stmt->loc, "Tipo no iterable en 'walk'");
                elem_t = zs_type_get_primitive(c->arena, TOK_KW_I32);
            }
            zs_symtab_enter_scope(c->symtab);
            zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
            sym->name = stmt->walk_stmt.var_name;
            sym->kind = SYM_VAR;
            sym->type = elem_t;
            sym->is_mutable = false;
            sym->is_initialized = true;
            zs_symtab_insert(c->symtab, sym);
            if (stmt->walk_stmt.body) {
                for (u32 i = 0; i < stmt->walk_stmt.body->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->walk_stmt.body->stmts[i]);
                }
            }
            zs_symtab_exit_scope(c->symtab);
            break;
        }

        case STMT_RETURN: {
            zs_type_t* ret_t = stmt->return_expr ? zs_checker_check_expr(c, stmt->return_expr) : zs_type_get_primitive(c->arena, TOK_KW_VOID);
            if (c->current_fn_ret_type && !zs_type_equals(c->current_fn_ret_type, ret_t)) {
                checker_error(c, stmt->loc, "Tipo de retorno incompatible: esperado %s, obtenido %s",
                              zs_type_to_string(c->arena, c->current_fn_ret_type),
                              zs_type_to_string(c->arena, ret_t));
            }
            break;
        }

        case STMT_FAIL: {
            zs_type_t* err_t = stmt->fail_expr ? zs_checker_check_expr(c, stmt->fail_expr) : zs_type_get_primitive(c->arena, TOK_KW_I32);
            if (c->current_fn_err_type && !zs_type_equals(c->current_fn_err_type, err_t)) {
                checker_error(c, stmt->loc, "Tipo de error incompatible en 'fail': esperado %s, obtenido %s",
                              zs_type_to_string(c->arena, c->current_fn_err_type),
                              zs_type_to_string(c->arena, err_t));
            }
            break;
        }

        case STMT_DEFER: {
            if (stmt->defer_stmt.block) {
                zs_symtab_enter_scope(c->symtab);
                for (u32 i = 0; i < stmt->defer_stmt.block->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->defer_stmt.block->stmts[i]);
                }
                zs_symtab_exit_scope(c->symtab);
            }
            if (stmt->defer_stmt.single_stmt) {
                zs_checker_check_stmt(c, stmt->defer_stmt.single_stmt);
            }
            break;
        }

        case STMT_BLOCK:
            zs_symtab_enter_scope(c->symtab);
            if (stmt->block_stmt) {
                for (u32 i = 0; i < stmt->block_stmt->stmt_count; ++i) {
                    zs_checker_check_stmt(c, stmt->block_stmt->stmts[i]);
                }
            }
            zs_symtab_exit_scope(c->symtab);
            break;

        case STMT_EXPR:
            zs_checker_check_expr(c, stmt->expr);
            break;

        default:
            break;
    }
}

// Pasada 1: Catalogación no lineal de símbolos globales
static void checker_pass_1(zs_checker_t* c, zs_ast_program_t* prog) {
    for (u32 i = 0; i < prog->decl_count; ++i) {
        zs_ast_decl_t* d = prog->decls[i];
        switch (d->kind) {
            case DECL_RECORD: {
                u32 fcount = d->record.field_count;
                zs_type_record_field_t* fields = (zs_type_record_field_t*)zs_arena_alloc(c->arena, fcount * sizeof(zs_type_record_field_t), sizeof(void*));
                for (u32 j = 0; j < fcount; ++j) {
                    fields[j].name = d->record.fields[j].name;
                    fields[j].type = resolve_ast_type(c, d->record.fields[j].type);
                    fields[j].is_pub = d->record.fields[j].is_pub;
                }
                zs_type_t* rec_t = zs_type_make_record(c->arena, d->record.name, fields, fcount, d->record.alignment);
                zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                sym->name = d->record.name;
                sym->kind = SYM_TYPE;
                sym->type = rec_t;
                sym->is_pub = d->is_pub;
                zs_symtab_insert(c->symtab, sym);
                break;
            }
            case DECL_CHOICE: {
                u32 vcount = d->choice.variant_count;
                zs_type_choice_variant_t* variants = (zs_type_choice_variant_t*)zs_arena_alloc(c->arena, vcount * sizeof(zs_type_choice_variant_t), sizeof(void*));
                for (u32 j = 0; j < vcount; ++j) {
                    variants[j].name = d->choice.variants[j].name;
                    variants[j].payload_count = d->choice.variants[j].payload_count;
                    variants[j].payload_types = (zs_type_t**)zs_arena_alloc(c->arena, variants[j].payload_count * sizeof(void*), sizeof(void*));
                    for (u32 k = 0; k < variants[j].payload_count; ++k) {
                        variants[j].payload_types[k] = resolve_ast_type(c, d->choice.variants[j].payload_types[k]);
                    }
                }
                zs_type_t* cho_t = zs_type_make_choice(c->arena, d->choice.name, variants, vcount);
                zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                sym->name = d->choice.name;
                sym->kind = SYM_TYPE;
                sym->type = cho_t;
                sym->is_pub = d->is_pub;
                zs_symtab_insert(c->symtab, sym);

                for (u32 j = 0; j < vcount; ++j) {
                    char qname[256];
                    snprintf(qname, sizeof(qname), "%s::%s", d->choice.name, d->choice.variants[j].name);
                    zs_symbol_t* vsym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                    vsym->name = zs_arena_strdup(c->arena, qname);
                    vsym->kind = SYM_VAR;
                    vsym->type = cho_t;
                    vsym->is_mutable = false;
                    vsym->is_initialized = true;
                    zs_symtab_insert(c->symtab, vsym);
                }
                break;
            }
            case DECL_FUNCTION: {
                u32 pcount = d->function.param_count;
                zs_type_t** ptypes = (zs_type_t**)zs_arena_alloc(c->arena, pcount * sizeof(void*), sizeof(void*));
                for (u32 j = 0; j < pcount; ++j) {
                    ptypes[j] = resolve_ast_type(c, d->function.params[j].type);
                }
                zs_type_t* ret = resolve_ast_type(c, d->function.return_type);
                zs_type_t* err = d->function.error_type ? resolve_ast_type(c, d->function.error_type) : NULL;

                zs_type_t* fn_t = zs_type_make_function(c->arena, ptypes, pcount, ret, err);
                zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                sym->name = d->function.name;
                sym->kind = SYM_FN;
                sym->type = fn_t;
                sym->is_pub = d->is_pub;
                zs_symtab_insert(c->symtab, sym);
                break;
            }
            case DECL_CONTRACT: {
                u32 mcount = d->contract.method_count;
                zs_type_contract_method_t* methods = (zs_type_contract_method_t*)zs_arena_alloc(c->arena, mcount * sizeof(zs_type_contract_method_t), sizeof(void*));
                for (u32 j = 0; j < mcount; ++j) {
                    methods[j].name = d->contract.methods[j].name;
                    u32 pcount = d->contract.methods[j].param_count;
                    zs_type_t** ptypes = (zs_type_t**)zs_arena_alloc(c->arena, pcount * sizeof(void*), sizeof(void*));
                    for (u32 k = 0; k < pcount; ++k) {
                        ptypes[k] = resolve_ast_type(c, d->contract.methods[j].params[k].type);
                    }
                    zs_type_t* ret = resolve_ast_type(c, d->contract.methods[j].return_type);
                    zs_type_t* err = d->contract.methods[j].error_type ? resolve_ast_type(c, d->contract.methods[j].error_type) : NULL;
                    methods[j].fn_type = zs_type_make_function(c->arena, ptypes, pcount, ret, err);
                }
                zs_type_t* ct = zs_type_make_contract(c->arena, d->contract.name, methods, mcount);
                zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                sym->name = d->contract.name;
                sym->kind = SYM_TYPE;
                sym->type = ct;
                sym->is_pub = d->is_pub;
                zs_symtab_insert(c->symtab, sym);
                break;
            }
            case DECL_FOREIGN: {
                for (u32 j = 0; j < d->foreign_block.function_count; ++j) {
                    zs_ast_fn_sig_t* sig = &d->foreign_block.functions[j];
                    u32 pcount = sig->param_count;
                    zs_type_t** ptypes = (zs_type_t**)zs_arena_alloc(c->arena, pcount * sizeof(void*), sizeof(void*));
                    for (u32 k = 0; k < pcount; ++k) {
                        ptypes[k] = resolve_ast_type(c, sig->params[k].type);
                    }
                    zs_type_t* ret = resolve_ast_type(c, sig->return_type);
                    zs_type_t* err = sig->error_type ? resolve_ast_type(c, sig->error_type) : NULL;
                    zs_type_t* fn_t = zs_type_make_function(c->arena, ptypes, pcount, ret, err);
                    zs_symbol_t* sym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                    sym->name = sig->name;
                    sym->kind = SYM_FN;
                    sym->type = fn_t;
                    sym->is_pub = d->is_pub;
                    zs_symtab_insert(c->symtab, sym);
                }
                break;
            }
            default:
                break;
        }
    }
}

// Pasada 2: Validación de cuerpos y seguridad
static void checker_pass_2(zs_checker_t* c, zs_ast_program_t* prog) {
    for (u32 i = 0; i < prog->decl_count; ++i) {
        zs_ast_decl_t* d = prog->decls[i];
        if (d->kind == DECL_FUNCTION && d->function.body) {
            zs_symbol_t* fn_sym = zs_symtab_lookup(c->symtab, d->function.name);
            c->current_fn_ret_type = fn_sym ? fn_sym->type->function.return_type : NULL;
            c->current_fn_err_type = fn_sym ? fn_sym->type->function.error_type : NULL;

            zs_symtab_enter_scope(c->symtab);

            // Insertar parámetros en el ámbito de la función
            for (u32 j = 0; j < d->function.param_count; ++j) {
                zs_symbol_t* psym = (zs_symbol_t*)zs_arena_alloc_zero(c->arena, sizeof(zs_symbol_t), sizeof(void*));
                psym->name = d->function.params[j].name;
                psym->kind = SYM_VAR;
                psym->type = fn_sym->type->function.param_types[j];
                psym->is_mutable = d->function.params[j].is_var;
                psym->is_initialized = true;
                psym->def_loc = d->loc;
                zs_symtab_insert(c->symtab, psym);
            }

            for (u32 j = 0; j < d->function.body->stmt_count; ++j) {
                zs_checker_check_stmt(c, d->function.body->stmts[j]);
            }

            zs_symtab_exit_scope(c->symtab);
            c->current_fn_ret_type = NULL;
            c->current_fn_err_type = NULL;
        }
    }
}

bool zs_checker_check_program(zs_checker_t* checker, zs_ast_program_t* program) {
    if (!program) return false;
    checker_pass_1(checker, program);
    checker_pass_2(checker, program);
    return checker->error_count == 0;
}
