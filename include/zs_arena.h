#ifndef ZS_ARENA_H
#define ZS_ARENA_H

#include "zs_common.h"

typedef struct zs_arena_block {
    struct zs_arena_block* next;
    usize capacity;
    usize used;
    u8 memory[];
} zs_arena_block_t;

typedef struct {
    zs_arena_block_t* first;
    zs_arena_block_t* current;
    usize default_block_size;
    usize total_allocated;
} zs_arena_t;

// Constructor y destructor
zs_arena_t* zs_arena_create(usize default_block_size);
void zs_arena_destroy(zs_arena_t* arena);
void zs_arena_reset(zs_arena_t* arena);

// Asignaciones
void* zs_arena_alloc(zs_arena_t* arena, usize size, usize alignment);
void* zs_arena_alloc_zero(zs_arena_t* arena, usize size, usize alignment);
char* zs_arena_strndup(zs_arena_t* arena, const char* str, usize len);
char* zs_arena_strdup(zs_arena_t* arena, const char* str);

#endif // ZS_ARENA_H
