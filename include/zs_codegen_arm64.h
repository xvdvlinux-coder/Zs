#ifndef ZS_CODEGEN_ARM64_H
#define ZS_CODEGEN_ARM64_H

#include "zs_common.h"
#include "zs_arena.h"
#include "zs_ast.h"
#include "zs_types.h"
#include "zs_symtab.h"

typedef struct {
    const char* name;
    i32 offset;          // offset relativo a x29: [x29, #-offset]
    zs_type_t* type;
    bool is_anchor;
    u8 slot_count;
    usize anchor_len;
    usize elem_size;
} zs_codegen_local_t;

typedef struct {
    const char* label;
    const char* content;
    usize len;
} zs_codegen_string_t;

typedef struct {
    zs_arena_t* arena;
    char* output;
    usize output_len;
    usize output_cap;
    u32 label_count;

    // Literales en .rodata
    zs_codegen_string_t strings[128];
    u32 string_count;

    // Función actual
    const char* current_fn_name;
    i32 current_frame_size;
    
    // Variables locales de la función
    zs_codegen_local_t locals[256];
    u32 local_count;

    // Pila determinista de sentencias defer LIFO
    zs_ast_stmt_t* defers[64];
    u32 defer_count;
} zs_codegen_arm64_t;

void zs_codegen_init(zs_codegen_arm64_t* cg, zs_arena_t* arena);
const char* zs_codegen_generate(zs_codegen_arm64_t* cg, zs_ast_program_t* prog);

#endif // ZS_CODEGEN_ARM64_H
