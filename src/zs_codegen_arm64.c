#include "zs_codegen_arm64.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void emit(zs_codegen_arm64_t* cg, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n <= 0) return;

    if (cg->output_len + (usize)n + 1 > cg->output_cap) {
        while (cg->output_len + (usize)n + 1 > cg->output_cap) {
            cg->output_cap *= 2;
        }
        char* new_out = (char*)zs_arena_alloc(cg->arena, cg->output_cap, sizeof(void*));
        memcpy(new_out, cg->output, cg->output_len);
        cg->output = new_out;
    }

    memcpy(cg->output + cg->output_len, buf, (usize)n);
    cg->output_len += (usize)n;
    cg->output[cg->output_len] = '\0';
}

void zs_codegen_init(zs_codegen_arm64_t* cg, zs_arena_t* arena) {
    cg->arena = arena;
    cg->output_cap = 64 * 1024;
    cg->output = (char*)zs_arena_alloc(arena, cg->output_cap, sizeof(void*));
    cg->output_len = 0;
    cg->output[0] = '\0';
    cg->label_count = 0;
    cg->string_count = 0;
    cg->current_fn_name = NULL;
    cg->current_frame_size = 0;
    cg->local_count = 0;
}

static u32 new_label(zs_codegen_arm64_t* cg) {
    return ++cg->label_count;
}

static zs_codegen_local_t* find_local(zs_codegen_arm64_t* cg, const char* name) {
    for (i32 i = (i32)cg->local_count - 1; i >= 0; --i) {
        if (strcmp(cg->locals[i].name, name) == 0) {
            return &cg->locals[i];
        }
    }
    return NULL;
}

static const char* register_string(zs_codegen_arm64_t* cg, const char* str, usize len) {
    for (u32 i = 0; i < cg->string_count; ++i) {
        if (cg->strings[i].len == len && memcmp(cg->strings[i].content, str, len) == 0) {
            return cg->strings[i].label;
        }
    }
    char lbl[32];
    snprintf(lbl, sizeof(lbl), ".Lstr_%u", cg->string_count);
    const char* label = zs_arena_strdup(cg->arena, lbl);
    cg->strings[cg->string_count].label = label;
    cg->strings[cg->string_count].content = str;
    cg->strings[cg->string_count].len = len;
    cg->string_count++;
    return label;
}

static bool expr_is_loc(zs_codegen_arm64_t* cg, zs_ast_expr_t* expr) {
    if (!expr) return false;
    if (expr->kind == EXPR_IDENT) {
        zs_codegen_local_t* l = find_local(cg, expr->ident);
        return l && l->offset > 0 && !l->is_anchor && l->anchor_len == 0 && l->elem_size > 0;
    }
    if (expr->kind == EXPR_FIELD) {
        return strcmp(expr->field.field_name, "start") == 0 ||
               strcmp(expr->field.field_name, "end") == 0;
    }
    if (expr->kind == EXPR_BINARY) {
        zs_token_kind_t op = expr->binary.op;
        return op == TOK_TAD_FWD || op == TOK_TAD_BWD || op == TOK_TAD_SAFE ||
               op == TOK_TAD_RING || op == TOK_TAD_BYTE;
    }
    if (expr->kind == EXPR_OR_FALLBACK) {
        return expr_is_loc(cg, expr->or_fallback.primary) || expr_is_loc(cg, expr->or_fallback.fallback);
    }
    return false;
}

// Declaraciones previas
static void codegen_expr(zs_codegen_arm64_t* cg, zs_ast_expr_t* expr);
static void codegen_stmt(zs_codegen_arm64_t* cg, zs_ast_stmt_t* stmt);
static void emit_defers(zs_codegen_arm64_t* cg, u32 from_index);

// Recorrer AST para registrar variables locales y asignar offsets positivos [x29, #offset]
static void scan_locals_in_stmts(zs_codegen_arm64_t* cg, zs_ast_stmt_t** stmts, u32 count, i32* current_offset) {
    for (u32 i = 0; i < count; ++i) {
        zs_ast_stmt_t* s = stmts[i];
        if (!s) continue;
        switch (s->kind) {
            case STMT_VAL_DECL:
            case STMT_VAR_DECL: {
                bool is_loc = false;
                bool is_view_or_str = false;
                usize elem_sz = 4;

                if (s->var_decl.type) {
                    if (s->var_decl.type->kind == TYPE_KIND_LOC) is_loc = true;
                    else if (s->var_decl.type->kind == TYPE_KIND_VIEW) is_view_or_str = true;
                    else if (s->var_decl.type->kind == TYPE_KIND_PRIMITIVE) {
                        if (s->var_decl.type->primitive_token == TOK_KW_STR ||
                            s->var_decl.type->primitive_token == TOK_KW_STRING) {
                            is_view_or_str = true;
                        } else if (s->var_decl.type->primitive_token == TOK_KW_I64 ||
                                   s->var_decl.type->primitive_token == TOK_KW_U64 ||
                                   s->var_decl.type->primitive_token == TOK_KW_ISIZE ||
                                   s->var_decl.type->primitive_token == TOK_KW_USIZE ||
                                   s->var_decl.type->primitive_token == TOK_KW_F64) {
                            elem_sz = 8;
                        }
                    }
                } else if (s->var_decl.init) {
                    if (expr_is_loc(cg, s->var_decl.init)) {
                        is_loc = true;
                    } else if (s->var_decl.init->kind == EXPR_STRING_LIT || s->var_decl.init->kind == EXPR_SLICE) {
                        is_view_or_str = true;
                    }
                }

                if (s->var_decl.init && s->var_decl.init->kind == EXPR_CATCH) {
                    if (s->var_decl.init->catch_expr.err_var && !find_local(cg, s->var_decl.init->catch_expr.err_var)) {
                        i32 err_offset = *current_offset;
                        *current_offset += 8;
                        zs_codegen_local_t* el = &cg->locals[cg->local_count++];
                        el->name = s->var_decl.init->catch_expr.err_var;
                        el->offset = err_offset;
                        el->is_anchor = false;
                        el->slot_count = 1;
                        el->anchor_len = 0;
                        el->elem_size = 4;
                    }
                    if (s->var_decl.init->catch_expr.catch_block) {
                        scan_locals_in_stmts(cg, s->var_decl.init->catch_expr.catch_block->stmts,
                                             s->var_decl.init->catch_expr.catch_block->stmt_count, current_offset);
                    }
                }

                i32 alloc_size = 8;
                u8 slots = 1;
                if (is_loc) { alloc_size = 24; slots = 3; }
                else if (is_view_or_str) { alloc_size = 16; slots = 2; }

                i32 my_offset = *current_offset;
                *current_offset += alloc_size;

                zs_codegen_local_t* loc = &cg->locals[cg->local_count++];
                loc->name = s->var_decl.name;
                loc->offset = my_offset;
                loc->is_anchor = false;
                loc->slot_count = slots;
                loc->anchor_len = is_loc ? 1 : 0;
                loc->elem_size = elem_sz;
                break;
            }
            case STMT_ANCHOR_DECL: {
                usize count = 0;
                if (s->anchor_decl.size_expr && s->anchor_decl.size_expr->kind == EXPR_INT_LIT) {
                    count = (usize)s->anchor_decl.size_expr->int_val;
                }
                usize elem_sz = 4;
                if (s->anchor_decl.elem_type && s->anchor_decl.elem_type->kind == TYPE_KIND_PRIMITIVE) {
                    if (s->anchor_decl.elem_type->primitive_token == TOK_KW_I64 ||
                        s->anchor_decl.elem_type->primitive_token == TOK_KW_U64 ||
                        s->anchor_decl.elem_type->primitive_token == TOK_KW_ISIZE ||
                        s->anchor_decl.elem_type->primitive_token == TOK_KW_USIZE ||
                        s->anchor_decl.elem_type->primitive_token == TOK_KW_F64) {
                        elem_sz = 8;
                    } else if (s->anchor_decl.elem_type->primitive_token == TOK_KW_I8 ||
                               s->anchor_decl.elem_type->primitive_token == TOK_KW_U8 ||
                               s->anchor_decl.elem_type->primitive_token == TOK_KW_BOOL) {
                        elem_sz = 1;
                    }
                }
                i32 total_bytes = (i32)(count * elem_sz);
                if (total_bytes < 8) total_bytes = 8;
                total_bytes = (total_bytes + 7) & ~7; // alinear a 8

                i32 my_offset = *current_offset;
                *current_offset += total_bytes;

                zs_codegen_local_t* loc = &cg->locals[cg->local_count++];
                loc->name = s->anchor_decl.name;
                loc->offset = my_offset;
                loc->is_anchor = true;
                loc->anchor_len = count;
                loc->elem_size = elem_sz;
                break;
            }
            case STMT_IF:
                if (s->if_stmt.then_block) scan_locals_in_stmts(cg, s->if_stmt.then_block->stmts, s->if_stmt.then_block->stmt_count, current_offset);
                if (s->if_stmt.else_block) scan_locals_in_stmts(cg, s->if_stmt.else_block->stmts, s->if_stmt.else_block->stmt_count, current_offset);
                break;
            case STMT_BLOCK:
                if (s->block_stmt) scan_locals_in_stmts(cg, s->block_stmt->stmts, s->block_stmt->stmt_count, current_offset);
                break;
            case STMT_LOOP:
                if (s->loop_stmt.body) scan_locals_in_stmts(cg, s->loop_stmt.body->stmts, s->loop_stmt.body->stmt_count, current_offset);
                break;
            case STMT_WHILE:
                if (s->while_stmt.body) scan_locals_in_stmts(cg, s->while_stmt.body->stmts, s->while_stmt.body->stmt_count, current_offset);
                break;
            case STMT_WALK: {
                i32 my_offset = *current_offset;
                *current_offset += 16; // variable de iteración (8) + índice interno (8)
                zs_codegen_local_t* loc = &cg->locals[cg->local_count++];
                loc->name = s->walk_stmt.var_name;
                loc->offset = my_offset;
                loc->is_anchor = false;
                loc->slot_count = 1;
                loc->anchor_len = 0;
                loc->elem_size = 4;
                if (s->walk_stmt.body) scan_locals_in_stmts(cg, s->walk_stmt.body->stmts, s->walk_stmt.body->stmt_count, current_offset);
                break;
            }
            case STMT_DEFER:
                if (s->defer_stmt.block) scan_locals_in_stmts(cg, s->defer_stmt.block->stmts, s->defer_stmt.block->stmt_count, current_offset);
                if (s->defer_stmt.single_stmt) scan_locals_in_stmts(cg, &s->defer_stmt.single_stmt, 1, current_offset);
                break;
            case STMT_ASSIGN:
                if (s->assign.value && s->assign.value->kind == EXPR_CATCH) {
                    if (s->assign.value->catch_expr.err_var) {
                        i32 err_offset = *current_offset;
                        *current_offset += 8;
                        zs_codegen_local_t* el = &cg->locals[cg->local_count++];
                        el->name = s->assign.value->catch_expr.err_var;
                        el->offset = err_offset;
                        el->is_anchor = false;
                        el->slot_count = 1;
                        el->anchor_len = 0;
                        el->elem_size = 4;
                    }
                    if (s->assign.value->catch_expr.catch_block) {
                        scan_locals_in_stmts(cg, s->assign.value->catch_expr.catch_block->stmts,
                                             s->assign.value->catch_expr.catch_block->stmt_count, current_offset);
                    }
                }
                break;
            case STMT_EXPR:
                if (s->expr && s->expr->kind == EXPR_CATCH) {
                    if (s->expr->catch_expr.err_var) {
                        i32 err_offset = *current_offset;
                        *current_offset += 8;
                        zs_codegen_local_t* el = &cg->locals[cg->local_count++];
                        el->name = s->expr->catch_expr.err_var;
                        el->offset = err_offset;
                        el->is_anchor = false;
                        el->slot_count = 1;
                        el->anchor_len = 0;
                        el->elem_size = 4;
                    }
                    if (s->expr->catch_expr.catch_block) {
                        scan_locals_in_stmts(cg, s->expr->catch_expr.catch_block->stmts,
                                             s->expr->catch_expr.catch_block->stmt_count, current_offset);
                    }
                }
                break;
            case STMT_RETURN:
                if (s->return_expr && s->return_expr->kind == EXPR_CATCH) {
                    if (s->return_expr->catch_expr.err_var) {
                        i32 err_offset = *current_offset;
                        *current_offset += 8;
                        zs_codegen_local_t* el = &cg->locals[cg->local_count++];
                        el->name = s->return_expr->catch_expr.err_var;
                        el->offset = err_offset;
                        el->is_anchor = false;
                        el->slot_count = 1;
                        el->anchor_len = 0;
                        el->elem_size = 4;
                    }
                    if (s->return_expr->catch_expr.catch_block) {
                        scan_locals_in_stmts(cg, s->return_expr->catch_expr.catch_block->stmts,
                                             s->return_expr->catch_expr.catch_block->stmt_count, current_offset);
                    }
                }
                break;
            default:
                break;
        }
    }
}

// Generador de expresiones ARM64
// Coloca el resultado principal en x0 (y x1, x2 si es loc o view/str)
static void codegen_expr(zs_codegen_arm64_t* cg, zs_ast_expr_t* expr) {
    if (!expr) {
        emit(cg, "    mov     x0, #0\n");
        return;
    }

    switch (expr->kind) {
        case EXPR_INT_LIT: {
            u64 val = expr->int_val;
            if (val <= 65535) {
                emit(cg, "    mov     x0, #%llu\n", (unsigned long long)val);
            } else {
                emit(cg, "    movz    x0, #%llu\n", (unsigned long long)(val & 0xFFFF));
                if ((val >> 16) & 0xFFFF) emit(cg, "    movk    x0, #%llu, lsl #16\n", (unsigned long long)((val >> 16) & 0xFFFF));
                if ((val >> 32) & 0xFFFF) emit(cg, "    movk    x0, #%llu, lsl #32\n", (unsigned long long)((val >> 32) & 0xFFFF));
                if ((val >> 48) & 0xFFFF) emit(cg, "    movk    x0, #%llu, lsl #48\n", (unsigned long long)((val >> 48) & 0xFFFF));
            }
            break;
        }

        case EXPR_BOOL_LIT:
            emit(cg, "    mov     x0, #%d\n", expr->bool_val ? 1 : 0);
            break;

        case EXPR_STRING_LIT: {
            const char* lbl = register_string(cg, expr->string_val.str, expr->string_val.len);
            emit(cg, "    adrp    x0, %s\n", lbl);
            emit(cg, "    add     x0, x0, :lo12:%s\n", lbl);
            emit(cg, "    mov     x1, #%zu\n", expr->string_val.len);
            break;
        }

        case EXPR_IDENT: {
            zs_codegen_local_t* l = find_local(cg, expr->ident);
            if (!l) {
                emit(cg, "    mov     x0, #0\n");
                break;
            }
            if (l->is_anchor) {
                // Dirección base del anclaje
                emit(cg, "    add     x0, x29, #%d\n", l->offset);
                emit(cg, "    mov     x1, x0\n"); // base
                emit(cg, "    add     x2, x0, #%zu\n", l->anchor_len * l->elem_size); // limit
            } else {
                // Cargar variable según número de slots
                emit(cg, "    ldr     x0, [x29, #%d]\n", l->offset);
                if (l->slot_count >= 2) {
                    emit(cg, "    ldr     x1, [x29, #%d]\n", l->offset + 8);
                }
                if (l->slot_count >= 3) {
                    emit(cg, "    ldr     x2, [x29, #%d]\n", l->offset + 16);
                }
            }
            break;
        }

        case EXPR_UNARY: {
            if (expr->unary.op == TOK_MINUS) {
                codegen_expr(cg, expr->unary.expr);
                emit(cg, "    neg     x0, x0\n");
            } else if (expr->unary.op == TOK_BANG) {
                codegen_expr(cg, expr->unary.expr);
                emit(cg, "    cmp     x0, #0\n");
                emit(cg, "    cset    x0, eq\n");
            } else if (expr->unary.op == TOK_STAR || expr->unary.op == TOK_AT) {
                // Desreferencia de coordenada TAD *p o @p
                codegen_expr(cg, expr->unary.expr);
                // x0 = ptr, x1 = base, x2 = limit
                // Chequeo de límites de hardware
                emit(cg, "    cmp     x0, x2\n");
                emit(cg, "    b.hs    .Lzs_trap\n");
                emit(cg, "    cmp     x0, x1\n");
                emit(cg, "    b.lo    .Lzs_trap\n");
                // Cargar elemento según tamaño de elemento
                usize elem_sz = 4;
                if (expr->unary.expr->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->unary.expr->ident);
                    if (l) elem_sz = l->elem_size;
                }
                if (elem_sz == 8) {
                    emit(cg, "    ldr     x0, [x0]\n");
                } else if (elem_sz == 2) {
                    emit(cg, "    ldrsh   x0, [x0]\n");
                } else if (elem_sz == 1) {
                    emit(cg, "    ldrb    w0, [x0]\n");
                } else {
                    emit(cg, "    ldrsw   x0, [x0]\n");
                }
            }
            break;
        }

        case EXPR_CAST:
            codegen_expr(cg, expr->cast.expr);
            break;

        case EXPR_FIELD: {
            zs_codegen_local_t* l = NULL;
            if (expr->field.target->kind == EXPR_IDENT) {
                l = find_local(cg, expr->field.target->ident);
            }

            if (l && l->is_anchor) {
                if (strcmp(expr->field.field_name, "start") == 0) {
                    emit(cg, "    add     x0, x29, #%d\n", l->offset); // ptr = base
                    emit(cg, "    mov     x1, x0\n");                  // base
                    emit(cg, "    add     x2, x0, #%zu\n", l->anchor_len * l->elem_size); // limit
                } else if (strcmp(expr->field.field_name, "end") == 0) {
                    emit(cg, "    add     x1, x29, #%d\n", l->offset); // base
                    emit(cg, "    add     x2, x1, #%zu\n", l->anchor_len * l->elem_size); // limit
                    emit(cg, "    mov     x0, x2\n");                  // ptr = limit
                } else if (strcmp(expr->field.field_name, "len") == 0) {
                    emit(cg, "    mov     x0, #%zu\n", l->anchor_len);
                }
            } else if (l && !l->is_anchor) {
                if (strcmp(expr->field.field_name, "len") == 0) {
                    emit(cg, "    ldr     x0, [x29, #%d]\n", l->offset + 8); // len
                }
            }
            break;
        }

        case EXPR_BINARY: {
            zs_token_kind_t op = expr->binary.op;

            // Desplazamiento topológico hacia adelante: p ~> k
            if (op == TOK_TAD_FWD) {
                codegen_expr(cg, expr->binary.left); // x0=ptr, x1=base, x2=limit
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right); // x0 = k
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");    // ptr
                emit(cg, "    ldp     x1, x2, [sp], #16\n"); // base, limit
                usize elem_sz = 4;
                if (expr->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x4, #%zu\n", elem_sz);
                emit(cg, "    mul     x3, x3, x4\n");
                emit(cg, "    add     x0, x0, x3\n"); // new_ptr = ptr + offset
                emit(cg, "    cmp     x0, x2\n");
                emit(cg, "    b.hi    .Lzs_trap\n");
                emit(cg, "    cmp     x0, x1\n");
                emit(cg, "    b.lo    .Lzs_trap\n");
                break;
            }

            // Desplazamiento topológico hacia atrás: q <~ k
            if (op == TOK_TAD_BWD) {
                codegen_expr(cg, expr->binary.left);
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right);
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");
                emit(cg, "    ldp     x1, x2, [sp], #16\n");
                usize elem_sz = 4;
                if (expr->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x4, #%zu\n", elem_sz);
                emit(cg, "    mul     x3, x3, x4\n");
                emit(cg, "    sub     x0, x0, x3\n"); // new_ptr = ptr - offset
                emit(cg, "    cmp     x0, x1\n");
                emit(cg, "    b.lo    .Lzs_trap\n");
                emit(cg, "    cmp     x0, x2\n");
                emit(cg, "    b.hi    .Lzs_trap\n");
                break;
            }

            // Salto por bytes físicos: p ~># bytes
            if (op == TOK_TAD_BYTE) {
                codegen_expr(cg, expr->binary.left);
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right);
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");
                emit(cg, "    ldp     x1, x2, [sp], #16\n");
                emit(cg, "    add     x0, x0, x3\n");
                emit(cg, "    cmp     x0, x2\n");
                emit(cg, "    b.hi    .Lzs_trap\n");
                emit(cg, "    cmp     x0, x1\n");
                emit(cg, "    b.lo    .Lzs_trap\n");
                break;
            }

            // Navegación con guardia: p ?~> k
            if (op == TOK_TAD_SAFE) {
                u32 lbl_fallback = new_label(cg);
                u32 lbl_end = new_label(cg);
                codegen_expr(cg, expr->binary.left);
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right);
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");
                emit(cg, "    ldp     x1, x2, [sp], #16\n");
                usize elem_sz = 4;
                if (expr->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x4, #%zu\n", elem_sz);
                emit(cg, "    mul     x3, x3, x4\n");
                emit(cg, "    add     x5, x0, x3\n");
                emit(cg, "    cmp     x5, x2\n");
                emit(cg, "    b.hi    .Lsafe_fail_%u\n", lbl_fallback);
                emit(cg, "    cmp     x5, x1\n");
                emit(cg, "    b.lo    .Lsafe_fail_%u\n", lbl_fallback);
                emit(cg, "    mov     x0, x5\n");
                emit(cg, "    b       .Lsafe_end_%u\n", lbl_end);
                emit(cg, ".Lsafe_fail_%u:\n", lbl_fallback);
                emit(cg, ".Lsafe_end_%u:\n", lbl_end);
                break;
            }

            // Aritmética toroidal cíclica: p %~> k
            if (op == TOK_TAD_RING) {
                u32 lbl_pos = new_label(cg);
                codegen_expr(cg, expr->binary.left);
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right);
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");
                emit(cg, "    ldp     x1, x2, [sp], #16\n");
                usize elem_sz = 4;
                if (expr->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    sub     x4, x0, x1\n"); // ptr - base
                emit(cg, "    mov     x5, #%zu\n", elem_sz);
                emit(cg, "    sdiv    x4, x4, x5\n"); // current_idx
                emit(cg, "    add     x4, x4, x3\n"); // current_idx + k
                emit(cg, "    sub     x6, x2, x1\n"); // limit - base = total_bytes
                emit(cg, "    sdiv    x6, x6, x5\n"); // N = total_bytes / elem_sz
                emit(cg, "    sdiv    x7, x4, x6\n");
                emit(cg, "    msub    x8, x7, x6, x4\n"); // rem = x4 % N
                emit(cg, "    cmp     x8, #0\n");
                emit(cg, "    b.ge    .Lmod_pos_%u\n", lbl_pos);
                emit(cg, "    add     x8, x8, x6\n");
                emit(cg, ".Lmod_pos_%u:\n", lbl_pos);
                emit(cg, "    mul     x9, x8, x5\n");
                emit(cg, "    add     x0, x1, x9\n"); // new_ptr = base + rem * elem_sz
                break;
            }

            // Métrica de distancia: p <=> q
            if (op == TOK_TAD_DIST) {
                codegen_expr(cg, expr->binary.left); // x0 = p.ptr
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, expr->binary.right); // x0 = q.ptr
                emit(cg, "    ldr     x1, [sp], #16\n");    // x1 = p.ptr
                emit(cg, "    sub     x0, x1, x0\n");       // p.ptr - q.ptr
                usize elem_sz = 4;
                if (expr->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, expr->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x2, #%zu\n", elem_sz);
                emit(cg, "    sdiv    x0, x0, x2\n");
                break;
            }

            // Operaciones binarias aritméticas estándar
            codegen_expr(cg, expr->binary.left);
            emit(cg, "    str     x0, [sp, #-16]!\n");
            codegen_expr(cg, expr->binary.right);
            emit(cg, "    ldr     x1, [sp], #16\n"); // x1 = left, x0 = right

            switch (op) {
                case TOK_PLUS:     emit(cg, "    add     x0, x1, x0\n"); break;
                case TOK_MINUS:    emit(cg, "    sub     x0, x1, x0\n"); break;
                case TOK_STAR:     emit(cg, "    mul     x0, x1, x0\n"); break;
                case TOK_SLASH:    emit(cg, "    sdiv    x0, x1, x0\n"); break;
                case TOK_PERCENT:  emit(cg, "    sdiv    x2, x1, x0\n    msub    x0, x2, x0, x1\n"); break;
                case TOK_EQ_EQ:    emit(cg, "    cmp     x1, x0\n    cset    x0, eq\n"); break;
                case TOK_BANG_EQ:  emit(cg, "    cmp     x1, x0\n    cset    x0, ne\n"); break;
                case TOK_LESS:     emit(cg, "    cmp     x1, x0\n    cset    x0, lt\n"); break;
                case TOK_LESS_EQ:  emit(cg, "    cmp     x1, x0\n    cset    x0, le\n"); break;
                case TOK_GREATER:  emit(cg, "    cmp     x1, x0\n    cset    x0, gt\n"); break;
                case TOK_GREATER_EQ: emit(cg, "    cmp     x1, x0\n    cset    x0, ge\n"); break;
                case TOK_AMP_AMP:  emit(cg, "    and     x0, x1, x0\n"); break;
                case TOK_PIPE_PIPE: emit(cg, "    orr     x0, x1, x0\n"); break;
                default: break;
            }
            break;
        }

        case EXPR_INDEX: {
            // target[index]
            codegen_expr(cg, expr->index.target);
            emit(cg, "    stp     x0, x1, [sp, #-16]!\n"); // ptr, len (o base)
            codegen_expr(cg, expr->index.index);
            emit(cg, "    mov     x3, x0\n");              // index
            emit(cg, "    ldp     x0, x1, [sp], #16\n");   // target ptr, len

            usize elem_sz = 4;
            usize bound = 0;
            bool has_known_bound = false;
            if (expr->index.target->kind == EXPR_IDENT) {
                zs_codegen_local_t* l = find_local(cg, expr->index.target->ident);
                if (l) {
                    elem_sz = l->elem_size;
                    if (l->is_anchor) {
                        bound = l->anchor_len;
                        has_known_bound = true;
                    }
                }
            }
            // Chequeo de límites
            if (has_known_bound) {
                emit(cg, "    cmp     x3, #%zu\n", bound);
            } else {
                emit(cg, "    cmp     x3, x1\n");
            }
            emit(cg, "    b.hs    .Lzs_trap\n");
            emit(cg, "    mov     x4, #%zu\n", elem_sz);
            emit(cg, "    mul     x5, x3, x4\n");
            emit(cg, "    add     x0, x0, x5\n");
            if (elem_sz == 8) {
                emit(cg, "    ldr     x0, [x0]\n");
            } else if (elem_sz == 2) {
                emit(cg, "    ldrsh   x0, [x0]\n");
            } else if (elem_sz == 1) {
                emit(cg, "    ldrb    w0, [x0]\n");
            } else {
                emit(cg, "    ldrsw   x0, [x0]\n");
            }
            break;
        }

        case EXPR_SLICE: {
            // target[start .. end]
            zs_codegen_local_t* l = NULL;
            if (expr->slice.target->kind == EXPR_IDENT) {
                l = find_local(cg, expr->slice.target->ident);
            }
            i64 start_idx = 0;
            if (expr->slice.start && expr->slice.start->kind == EXPR_INT_LIT) {
                start_idx = (i64)expr->slice.start->int_val;
            }
            i64 end_idx = l ? (i64)l->anchor_len : 0;
            if (expr->slice.end && expr->slice.end->kind == EXPR_INT_LIT) {
                end_idx = (i64)expr->slice.end->int_val;
            }
            usize elem_sz = l ? l->elem_size : 4;
            i64 slice_len = end_idx - start_idx;
            if (slice_len < 0) slice_len = 0;

            if (l && l->is_anchor) {
                emit(cg, "    add     x0, x29, #%d\n", l->offset);
                emit(cg, "    add     x0, x0, #%lld\n", (long long)(start_idx * (i64)elem_sz)); // v.ptr
                emit(cg, "    mov     x1, #%lld\n", (long long)slice_len);                      // v.len
            }
            break;
        }

        case EXPR_OR_FALLBACK: {
            u32 lbl_ok = new_label(cg);
            u32 lbl_end = new_label(cg);
            if (expr->or_fallback.primary->kind == EXPR_CALL) {
                codegen_expr(cg, expr->or_fallback.primary); // hace el bl
                emit(cg, "    b.cs    .Lfall_%u\n", lbl_ok);
                emit(cg, "    b       .Lend_%u\n", lbl_end);
                emit(cg, ".Lfall_%u:\n", lbl_ok);
                if (expr->or_fallback.fallback->kind == EXPR_RETURN_FAIL) {
                    emit_defers(cg, 0);
                    emit(cg, "    b       .L%s_fail_epilogue\n", cg->current_fn_name);
                } else {
                    codegen_expr(cg, expr->or_fallback.fallback);
                }
                emit(cg, ".Lend_%u:\n", lbl_end);
                break;
            }
            if (expr->or_fallback.primary->kind == EXPR_BINARY &&
                expr->or_fallback.primary->binary.op == TOK_TAD_SAFE) {
                zs_ast_expr_t* saf = expr->or_fallback.primary;
                codegen_expr(cg, saf->binary.left); // x0=ptr, x1=base, x2=limit
                emit(cg, "    stp     x1, x2, [sp, #-16]!\n");
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, saf->binary.right); // x0 = k
                emit(cg, "    mov     x3, x0\n");
                emit(cg, "    ldr     x0, [sp], #16\n");
                emit(cg, "    ldp     x1, x2, [sp], #16\n");
                usize elem_sz = 4;
                if (saf->binary.left->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, saf->binary.left->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x4, #%zu\n", elem_sz);
                emit(cg, "    mul     x3, x3, x4\n");
                emit(cg, "    add     x5, x0, x3\n");
                emit(cg, "    cmp     x5, x2\n");
                emit(cg, "    b.hi    .Lfall_%u\n", lbl_ok);
                emit(cg, "    cmp     x5, x1\n");
                emit(cg, "    b.lo    .Lfall_%u\n", lbl_ok);
                // Válido
                emit(cg, "    mov     x0, x5\n");
                emit(cg, "    b       .Lend_%u\n", lbl_end);
                // Fallback
                emit(cg, ".Lfall_%u:\n", lbl_ok);
                codegen_expr(cg, expr->or_fallback.fallback);
                emit(cg, ".Lend_%u:\n", lbl_end);
            }
            break;
        }

        case EXPR_CATCH: {
            u32 lbl_catch = new_label(cg);
            u32 lbl_end = new_label(cg);
            codegen_expr(cg, expr->catch_expr.primary);
            emit(cg, "    b.cs    .Lcatch_%u\n", lbl_catch);
            emit(cg, "    b       .Lend_%u\n", lbl_end);
            emit(cg, ".Lcatch_%u:\n", lbl_catch);
            if (expr->catch_expr.err_var) {
                zs_codegen_local_t* el = find_local(cg, expr->catch_expr.err_var);
                if (el) {
                    emit(cg, "    str     x1, [x29, #%d]\n", el->offset);
                }
            }
            if (expr->catch_expr.catch_block) {
                for (u32 j = 0; j < expr->catch_expr.catch_block->stmt_count; ++j) {
                    codegen_stmt(cg, expr->catch_expr.catch_block->stmts[j]);
                }
            }
            emit(cg, ".Lend_%u:\n", lbl_end);
            break;
        }

        case EXPR_CALL: {
            for (u32 i = 0; i < expr->call.arg_count && i < 8; ++i) {
                codegen_expr(cg, expr->call.args[i]);
                emit(cg, "    str     x0, [sp, #-16]!\n");
            }
            for (i32 i = (i32)expr->call.arg_count - 1; i >= 0 && i < 8; --i) {
                emit(cg, "    ldr     x%d, [sp], #16\n", i);
            }
            if (expr->call.callee->kind == EXPR_IDENT) {
                emit(cg, "    bl      %s\n", expr->call.callee->ident);
            }
            break;
        }

        default:
            emit(cg, "    mov     x0, #0\n");
            break;
    }
}

static void emit_defers(zs_codegen_arm64_t* cg, u32 from_index) {
    for (i32 i = (i32)cg->defer_count - 1; i >= (i32)from_index; --i) {
        zs_ast_stmt_t* d = cg->defers[i];
        if (d->defer_stmt.block) {
            for (u32 j = 0; j < d->defer_stmt.block->stmt_count; ++j) {
                codegen_stmt(cg, d->defer_stmt.block->stmts[j]);
            }
        } else if (d->defer_stmt.single_stmt) {
            codegen_stmt(cg, d->defer_stmt.single_stmt);
        }
    }
}

// Generador de sentencias ARM64
static void codegen_stmt(zs_codegen_arm64_t* cg, zs_ast_stmt_t* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_VAL_DECL:
        case STMT_VAR_DECL: {
            zs_codegen_local_t* l = find_local(cg, stmt->var_decl.name);
            if (!l) break;

            if (stmt->var_decl.init) {
                codegen_expr(cg, stmt->var_decl.init);
                // Guardar valor(es) según número de slots
                emit(cg, "    str     x0, [x29, #%d]\n", l->offset);
                if (l->slot_count >= 2) {
                    emit(cg, "    str     x1, [x29, #%d]\n", l->offset + 8);
                }
                if (l->slot_count >= 3) {
                    emit(cg, "    str     x2, [x29, #%d]\n", l->offset + 16);
                }
            }
            break;
        }

        case STMT_ANCHOR_DECL: {
            zs_codegen_local_t* l = find_local(cg, stmt->anchor_decl.name);
            if (!l) break;

            if (stmt->anchor_decl.init_expr && stmt->anchor_decl.init_expr->kind == EXPR_ARRAY_LIT) {
                zs_ast_expr_t* arr = stmt->anchor_decl.init_expr;
                for (u32 i = 0; i < arr->array_lit.count && i < l->anchor_len; ++i) {
                    codegen_expr(cg, arr->array_lit.elems[i]);
                    i32 elem_offset = l->offset + (i32)(i * l->elem_size);
                    if (l->elem_size == 8) {
                        emit(cg, "    str     x0, [x29, #%d]\n", elem_offset);
                    } else if (l->elem_size == 2) {
                        emit(cg, "    strh    w0, [x29, #%d]\n", elem_offset);
                    } else if (l->elem_size == 1) {
                        emit(cg, "    strb    w0, [x29, #%d]\n", elem_offset);
                    } else {
                        emit(cg, "    str     w0, [x29, #%d]\n", elem_offset);
                    }
                }
            }
            break;
        }

        case STMT_ASSIGN: {
            if (stmt->assign.lvalue->kind == EXPR_UNARY &&
                (stmt->assign.lvalue->unary.op == TOK_STAR || stmt->assign.lvalue->unary.op == TOK_AT)) {
                codegen_expr(cg, stmt->assign.value); // x0 = val
                emit(cg, "    str     x0, [sp, #-16]!\n");
                codegen_expr(cg, stmt->assign.lvalue->unary.expr); // x0 = p.ptr
                emit(cg, "    ldr     x1, [sp], #16\n");          // x1 = val
                usize elem_sz = 4;
                if (stmt->assign.lvalue->unary.expr->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, stmt->assign.lvalue->unary.expr->ident);
                    if (l) elem_sz = l->elem_size;
                }
                if (elem_sz == 8) {
                    emit(cg, "    str     x1, [x0]\n");
                } else if (elem_sz == 2) {
                    emit(cg, "    strh    w1, [x0]\n");
                } else if (elem_sz == 1) {
                    emit(cg, "    strb    w1, [x0]\n");
                } else {
                    emit(cg, "    str     w1, [x0]\n");
                }
                break;
            }

            if (stmt->assign.lvalue->kind == EXPR_INDEX) {
                codegen_expr(cg, stmt->assign.value);
                emit(cg, "    str     x0, [sp, #-16]!\n"); // guardar val
                codegen_expr(cg, stmt->assign.lvalue->index.target); // x0 = base
                emit(cg, "    str     x0, [sp, #-16]!\n"); // guardar base
                codegen_expr(cg, stmt->assign.lvalue->index.index); // x0 = index
                emit(cg, "    mov     x3, x0\n");          // x3 = index
                emit(cg, "    ldr     x0, [sp], #16\n");   // x0 = base
                emit(cg, "    ldr     x1, [sp], #16\n");   // x1 = val

                usize elem_sz = 4;
                if (stmt->assign.lvalue->index.target->kind == EXPR_IDENT) {
                    zs_codegen_local_t* l = find_local(cg, stmt->assign.lvalue->index.target->ident);
                    if (l) elem_sz = l->elem_size;
                }
                emit(cg, "    mov     x4, #%zu\n", elem_sz);
                emit(cg, "    mul     x5, x3, x4\n");
                emit(cg, "    add     x0, x0, x5\n");
                if (elem_sz == 8) {
                    emit(cg, "    str     x1, [x0]\n");
                } else if (elem_sz == 2) {
                    emit(cg, "    strh    w1, [x0]\n");
                } else if (elem_sz == 1) {
                    emit(cg, "    strb    w1, [x0]\n");
                } else {
                    emit(cg, "    str     w1, [x0]\n");
                }
                break;
            }

            if (stmt->assign.lvalue->kind == EXPR_IDENT) {
                zs_codegen_local_t* l = find_local(cg, stmt->assign.lvalue->ident);
                if (!l) break;

                codegen_expr(cg, stmt->assign.value);
                emit(cg, "    str     x0, [x29, #%d]\n", l->offset);
                if (l->slot_count >= 2) {
                    emit(cg, "    str     x1, [x29, #%d]\n", l->offset + 8);
                }
                if (l->slot_count >= 3) {
                    emit(cg, "    str     x2, [x29, #%d]\n", l->offset + 16);
                }
            }
            break;
        }

        case STMT_RETURN: {
            if (stmt->return_expr) {
                codegen_expr(cg, stmt->return_expr);
            }
            emit(cg, "    str     x0, [sp, #-16]!\n");
            emit_defers(cg, 0);
            emit(cg, "    ldr     x0, [sp], #16\n");
            emit(cg, "    b       .L%s_epilogue\n", cg->current_fn_name);
            break;
        }

        case STMT_FAIL: {
            if (stmt->fail_expr) {
                codegen_expr(cg, stmt->fail_expr);
                emit(cg, "    mov     x1, x0\n");
            } else {
                emit(cg, "    mov     x1, #1\n");
            }
            emit(cg, "    str     x1, [sp, #-16]!\n");
            emit_defers(cg, 0);
            emit(cg, "    ldr     x1, [sp], #16\n");
            emit(cg, "    b       .L%s_fail_epilogue\n", cg->current_fn_name);
            break;
        }

        case STMT_DEFER:
            if (cg->defer_count < 64) {
                cg->defers[cg->defer_count++] = stmt;
            }
            break;

        case STMT_IF: {
            u32 lbl_else = new_label(cg);
            u32 lbl_end = new_label(cg);

            codegen_expr(cg, stmt->if_stmt.cond);
            emit(cg, "    cbz     x0, .Lelse_%u\n", lbl_else);

            if (stmt->if_stmt.then_block) {
                for (u32 i = 0; i < stmt->if_stmt.then_block->stmt_count; ++i) {
                    codegen_stmt(cg, stmt->if_stmt.then_block->stmts[i]);
                }
            }
            emit(cg, "    b       .Lend_%u\n", lbl_end);

            emit(cg, ".Lelse_%u:\n", lbl_else);
            if (stmt->if_stmt.else_block) {
                for (u32 i = 0; i < stmt->if_stmt.else_block->stmt_count; ++i) {
                    codegen_stmt(cg, stmt->if_stmt.else_block->stmts[i]);
                }
            }
            if (stmt->if_stmt.else_if) {
                codegen_stmt(cg, stmt->if_stmt.else_if);
            }
            emit(cg, ".Lend_%u:\n", lbl_end);
            break;
        }

        case STMT_LOOP: {
            u32 lbl_loop = new_label(cg);
            emit(cg, ".Lloop_%u:\n", lbl_loop);
            if (stmt->loop_stmt.body) {
                for (u32 i = 0; i < stmt->loop_stmt.body->stmt_count; ++i) {
                    codegen_stmt(cg, stmt->loop_stmt.body->stmts[i]);
                }
            }
            emit(cg, "    b       .Lloop_%u\n", lbl_loop);
            break;
        }

        case STMT_WHILE: {
            u32 lbl_start = new_label(cg);
            u32 lbl_end = new_label(cg);
            emit(cg, ".Lwhile_start_%u:\n", lbl_start);
            codegen_expr(cg, stmt->while_stmt.cond);
            emit(cg, "    cbz     x0, .Lwhile_end_%u\n", lbl_end);
            if (stmt->while_stmt.body) {
                for (u32 i = 0; i < stmt->while_stmt.body->stmt_count; ++i) {
                    codegen_stmt(cg, stmt->while_stmt.body->stmts[i]);
                }
            }
            emit(cg, "    b       .Lwhile_start_%u\n", lbl_start);
            emit(cg, ".Lwhile_end_%u:\n", lbl_end);
            break;
        }

        case STMT_WALK: {
            // walk x in ancla
            zs_codegen_local_t* item = find_local(cg, stmt->walk_stmt.var_name);
            zs_codegen_local_t* src = NULL;
            if (stmt->walk_stmt.iterable->kind == EXPR_IDENT) {
                src = find_local(cg, stmt->walk_stmt.iterable->ident);
            }
            if (!item || !src || !src->is_anchor) break;

            u32 lbl_walk = new_label(cg);
            u32 lbl_end = new_label(cg);
            i32 idx_slot = item->offset + 8; // slot de índice iterador en stack

            emit(cg, "    mov     x10, #0\n");
            emit(cg, "    str     x10, [x29, #%d]\n", idx_slot);

            emit(cg, ".Lwalk_%u:\n", lbl_walk);
            emit(cg, "    ldr     x10, [x29, #%d]\n", idx_slot);
            emit(cg, "    cmp     x10, #%zu\n", src->anchor_len);
            emit(cg, "    b.ge    .Lwalk_end_%u\n", lbl_end);

            // Cargar elemento i en el local item
            emit(cg, "    add     x11, x29, #%d\n", src->offset);
            emit(cg, "    mov     x12, #%zu\n", src->elem_size);
            emit(cg, "    mul     x13, x10, x12\n");
            emit(cg, "    add     x11, x11, x13\n");
            emit(cg, "    ldrsw   x14, [x11]\n");
            emit(cg, "    str     x14, [x29, #%d]\n", item->offset);

            if (stmt->walk_stmt.body) {
                for (u32 j = 0; j < stmt->walk_stmt.body->stmt_count; ++j) {
                    codegen_stmt(cg, stmt->walk_stmt.body->stmts[j]);
                }
            }

            // Incrementar índice
            emit(cg, "    ldr     x10, [x29, #%d]\n", idx_slot);
            emit(cg, "    add     x10, x10, #1\n");
            emit(cg, "    str     x10, [x29, #%d]\n", idx_slot);
            emit(cg, "    b       .Lwalk_%u\n", lbl_walk);
            emit(cg, ".Lwalk_end_%u:\n", lbl_end);
            break;
        }

        case STMT_TRAP:
            emit(cg, "    brk     #0x42\n");
            break;

        case STMT_BLOCK:
            if (stmt->block_stmt) {
                u32 saved_defer_count = cg->defer_count;
                for (u32 i = 0; i < stmt->block_stmt->stmt_count; ++i) {
                    codegen_stmt(cg, stmt->block_stmt->stmts[i]);
                }
                emit_defers(cg, saved_defer_count);
                cg->defer_count = saved_defer_count;
            }
            break;

        case STMT_EXPR:
            codegen_expr(cg, stmt->expr);
            break;

        default:
            break;
    }
}

// Generar una función completa
static void codegen_function(zs_codegen_arm64_t* cg, zs_ast_decl_t* d) {
    cg->current_fn_name = d->function.name;
    cg->local_count = 0;
    cg->defer_count = 0;
    i32 offset = 16; // después de x29/x30 en [x29, #0] y [x29, #8]

    // Parámetros de función
    for (u32 i = 0; i < d->function.param_count; ++i) {
        zs_codegen_local_t* loc = &cg->locals[cg->local_count++];
        loc->name = d->function.params[i].name;
        loc->offset = offset;
        loc->is_anchor = false;
        loc->slot_count = 1;
        loc->anchor_len = 0;
        loc->elem_size = 4;
        offset += 8;
    }

    // Escanear variables locales y anclajes en el cuerpo
    if (d->function.body) {
        scan_locals_in_stmts(cg, d->function.body->stmts, d->function.body->stmt_count, &offset);
    }

    // Alinear stack frame a múltiplo de 16 bytes
    i32 frame_size = (offset + 15) & ~15;
    if (frame_size < 32) frame_size = 32;
    cg->current_frame_size = frame_size;

    // Prólogo
    emit(cg, "\n    .globl  %s\n", d->function.name);
    emit(cg, "    .type   %s, %%function\n", d->function.name);
    emit(cg, "%s:\n", d->function.name);
    emit(cg, "    stp     x29, x30, [sp, #-%d]!\n", frame_size);
    emit(cg, "    mov     x29, sp\n");

    // Guardar parámetros de entrada en stack
    for (u32 i = 0; i < d->function.param_count && i < 8; ++i) {
        emit(cg, "    str     x%d, [x29, #%d]\n", i, cg->locals[i].offset);
    }

    // Emitir sentencias del cuerpo
    if (d->function.body) {
        for (u32 i = 0; i < d->function.body->stmt_count; ++i) {
            codegen_stmt(cg, d->function.body->stmts[i]);
        }
    }

    // Si la función cae hasta el final sin return explícito, ejecutar defers
    emit_defers(cg, 0);

    // Epílogo de éxito (Carry = 0)
    emit(cg, ".L%s_epilogue:\n", d->function.name);
    emit(cg, "    adds    xzr, xzr, xzr\n");
    emit(cg, "    mov     sp, x29\n");
    emit(cg, "    ldp     x29, x30, [sp], #%d\n", frame_size);
    emit(cg, "    ret\n");

    // Epílogo de fallo (Carry = 1)
    emit(cg, ".L%s_fail_epilogue:\n", d->function.name);
    emit(cg, "    cmp     xzr, xzr\n");
    emit(cg, "    mov     sp, x29\n");
    emit(cg, "    ldp     x29, x30, [sp], #%d\n", frame_size);
    emit(cg, "    ret\n");
}

const char* zs_codegen_generate(zs_codegen_arm64_t* cg, zs_ast_program_t* prog) {
    emit(cg, "    .arch   armv8-a\n");
    emit(cg, "    .text\n");

    // Trampa común de hardware TAD
    emit(cg, ".Lzs_trap:\n");
    emit(cg, "    brk     #0x42\n");

    for (u32 i = 0; i < prog->decl_count; ++i) {
        if (prog->decls[i]->kind == DECL_FUNCTION) {
            codegen_function(cg, prog->decls[i]);
        }
    }

    // Sección de literales de solo lectura
    if (cg->string_count > 0) {
        emit(cg, "\n    .section .rodata\n");
        for (u32 i = 0; i < cg->string_count; ++i) {
            emit(cg, "%s:\n", cg->strings[i].label);
            emit(cg, "    .asciz  \"%s\"\n", cg->strings[i].content);
        }
    }

    return cg->output;
}
