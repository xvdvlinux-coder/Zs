#include "zs_symtab.h"

static zs_scope_t* zs_scope_create(zs_arena_t* arena, zs_scope_t* parent, u32 depth) {
    zs_scope_t* s = (zs_scope_t*)zs_arena_alloc_zero(arena, sizeof(zs_scope_t), sizeof(void*));
    s->parent = parent;
    s->depth = depth;
    s->capacity = 16;
    s->count = 0;
    s->symbols = (zs_symbol_t**)zs_arena_alloc(arena, s->capacity * sizeof(void*), sizeof(void*));
    return s;
}

zs_symtab_t* zs_symtab_create(zs_arena_t* arena) {
    zs_symtab_t* st = (zs_symtab_t*)zs_arena_alloc_zero(arena, sizeof(zs_symtab_t), sizeof(void*));
    st->arena = arena;
    st->global = zs_scope_create(arena, NULL, 0);
    st->current = st->global;
    return st;
}

void zs_symtab_enter_scope(zs_symtab_t* st) {
    st->current = zs_scope_create(st->arena, st->current, st->current->depth + 1);
}

void zs_symtab_exit_scope(zs_symtab_t* st) {
    if (st->current->parent) {
        st->current = st->current->parent;
    }
}

bool zs_symtab_insert(zs_symtab_t* st, zs_symbol_t* sym) {
    zs_scope_t* scope = st->current;

    // Verificar si ya existe en este ámbito exacto
    for (u32 i = 0; i < scope->count; ++i) {
        if (strcmp(scope->symbols[i]->name, sym->name) == 0) {
            return false; // Conflicto de redefinición
        }
    }

    if (scope->count >= scope->capacity) {
        u32 new_cap = scope->capacity * 2;
        zs_symbol_t** new_syms = (zs_symbol_t**)zs_arena_alloc(st->arena, new_cap * sizeof(void*), sizeof(void*));
        memcpy(new_syms, scope->symbols, scope->count * sizeof(void*));
        scope->symbols = new_syms;
        scope->capacity = new_cap;
    }

    sym->scope_depth = scope->depth;
    scope->symbols[scope->count++] = sym;
    return true;
}

zs_symbol_t* zs_symtab_lookup(zs_symtab_t* st, const char* name) {
    zs_scope_t* scope = st->current;
    while (scope) {
        for (u32 i = 0; i < scope->count; ++i) {
            if (strcmp(scope->symbols[i]->name, name) == 0) {
                return scope->symbols[i];
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

zs_symbol_t* zs_symtab_lookup_local(zs_symtab_t* st, const char* name) {
    zs_scope_t* scope = st->current;
    for (u32 i = 0; i < scope->count; ++i) {
        if (strcmp(scope->symbols[i]->name, name) == 0) {
            return scope->symbols[i];
        }
    }
    return NULL;
}
