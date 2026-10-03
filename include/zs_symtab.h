#ifndef ZS_SYMTAB_H
#define ZS_SYMTAB_H

#include "zs_types.h"

typedef enum {
    SYM_VAR,
    SYM_FN,
    SYM_TYPE,
    SYM_ANCHOR
} zs_sym_kind_t;

typedef struct zs_symbol zs_symbol_t;

struct zs_symbol {
    const char* name;
    zs_sym_kind_t kind;
    zs_type_t* type;
    bool is_mutable;      // true para 'var', false para 'val'
    bool is_initialized;  // true si se ha asignado antes de leer
    u32 scope_depth;      // Nivel léxico de ámbito
    zs_symbol_t* anchor_parent; // Si es loc, anclaje al que está atado
    bool is_pub;
    zs_loc_t def_loc;
};

typedef struct zs_scope {
    struct zs_scope* parent;
    u32 depth;
    zs_symbol_t** symbols;
    u32 count;
    u32 capacity;
} zs_scope_t;

typedef struct {
    zs_arena_t* arena;
    zs_scope_t* current;
    zs_scope_t* global;
} zs_symtab_t;

zs_symtab_t* zs_symtab_create(zs_arena_t* arena);
void zs_symtab_enter_scope(zs_symtab_t* st);
void zs_symtab_exit_scope(zs_symtab_t* st);

bool zs_symtab_insert(zs_symtab_t* st, zs_symbol_t* sym);
zs_symbol_t* zs_symtab_lookup(zs_symtab_t* st, const char* name);
zs_symbol_t* zs_symtab_lookup_local(zs_symtab_t* st, const char* name);

#endif // ZS_SYMTAB_H
