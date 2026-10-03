#include "zs_ast.h"

zs_ast_type_t* zs_ast_type_primitive(zs_arena_t* arena, zs_loc_t loc, zs_token_kind_t prim_tok) {
    zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_type_t), sizeof(void*));
    t->kind = TYPE_KIND_PRIMITIVE;
    t->loc = loc;
    t->primitive_token = prim_tok;
    return t;
}

zs_ast_type_t* zs_ast_type_loc(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* sub) {
    zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_type_t), sizeof(void*));
    t->kind = TYPE_KIND_LOC;
    t->loc = loc;
    t->sub_type = sub;
    return t;
}

zs_ast_type_t* zs_ast_type_view(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* sub) {
    zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_type_t), sizeof(void*));
    t->kind = TYPE_KIND_VIEW;
    t->loc = loc;
    t->sub_type = sub;
    return t;
}

zs_ast_type_t* zs_ast_type_array(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* elem, zs_ast_expr_t* sz) {
    zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_type_t), sizeof(void*));
    t->kind = TYPE_KIND_ARRAY;
    t->loc = loc;
    t->array.elem_type = elem;
    t->array.size_expr = sz;
    return t;
}

zs_ast_type_t* zs_ast_type_named(zs_arena_t* arena, zs_loc_t loc, const char* name) {
    zs_ast_type_t* t = (zs_ast_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_type_t), sizeof(void*));
    t->kind = TYPE_KIND_NAMED;
    t->loc = loc;
    t->named.parts = (const char**)zs_arena_alloc(arena, sizeof(const char*), sizeof(void*));
    t->named.parts[0] = name;
    t->named.count = 1;
    return t;
}

zs_ast_expr_t* zs_ast_expr_create(zs_arena_t* arena, zs_expr_kind_t kind, zs_loc_t loc) {
    zs_ast_expr_t* e = (zs_ast_expr_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_expr_t), sizeof(void*));
    e->kind = kind;
    e->loc = loc;
    return e;
}

zs_ast_stmt_t* zs_ast_stmt_create(zs_arena_t* arena, zs_stmt_kind_t kind, zs_loc_t loc) {
    zs_ast_stmt_t* s = (zs_ast_stmt_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_stmt_t), sizeof(void*));
    s->kind = kind;
    s->loc = loc;
    return s;
}

zs_ast_decl_t* zs_ast_decl_create(zs_arena_t* arena, zs_decl_kind_t kind, zs_loc_t loc, bool is_pub) {
    zs_ast_decl_t* d = (zs_ast_decl_t*)zs_arena_alloc_zero(arena, sizeof(zs_ast_decl_t), sizeof(void*));
    d->kind = kind;
    d->loc = loc;
    d->is_pub = is_pub;
    return d;
}
