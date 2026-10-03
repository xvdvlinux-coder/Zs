#include "zs_arena.h"

#define ZS_ARENA_DEFAULT_SIZE (64 * 1024) // 64 KB por bloque por defecto

static zs_arena_block_t* zs_arena_create_block(usize capacity) {
    zs_arena_block_t* block = (zs_arena_block_t*)malloc(sizeof(zs_arena_block_t) + capacity);
    if (!block) {
        ZS_FATAL("Fallo de memoria al crear bloque de arena de %zu bytes", capacity);
    }
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

zs_arena_t* zs_arena_create(usize default_block_size) {
    if (default_block_size == 0) {
        default_block_size = ZS_ARENA_DEFAULT_SIZE;
    }
    zs_arena_t* arena = (zs_arena_t*)malloc(sizeof(zs_arena_t));
    if (!arena) {
        ZS_FATAL("Fallo de memoria al crear arena");
    }
    arena->default_block_size = default_block_size;
    arena->first = zs_arena_create_block(default_block_size);
    arena->current = arena->first;
    arena->total_allocated = sizeof(zs_arena_t) + sizeof(zs_arena_block_t) + default_block_size;
    return arena;
}

void zs_arena_destroy(zs_arena_t* arena) {
    if (!arena) return;
    zs_arena_block_t* curr = arena->first;
    while (curr) {
        zs_arena_block_t* next = curr->next;
        free(curr);
        curr = next;
    }
    free(arena);
}

void zs_arena_reset(zs_arena_t* arena) {
    if (!arena) return;
    zs_arena_block_t* curr = arena->first;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
    arena->current = arena->first;
}

static inline usize zs_align_up(usize val, usize align) {
    if (align == 0) return val;
    return (val + align - 1) & ~(align - 1);
}

void* zs_arena_alloc(zs_arena_t* arena, usize size, usize alignment) {
    if (!arena || size == 0) return NULL;
    if (alignment == 0) alignment = sizeof(void*);

    zs_arena_block_t* curr = arena->current;
    usize aligned_used = zs_align_up(curr->used, alignment);

    if (aligned_used + size > curr->capacity) {
        // Necesitamos un nuevo bloque
        usize new_cap = arena->default_block_size;
        if (size + alignment > new_cap) {
            new_cap = size + alignment; // Bloque grande especial
        }
        zs_arena_block_t* new_block = zs_arena_create_block(new_cap);
        curr->next = new_block;
        arena->current = new_block;
        curr = new_block;
        aligned_used = zs_align_up(curr->used, alignment);
        arena->total_allocated += sizeof(zs_arena_block_t) + new_cap;
    }

    void* ptr = (void*)(curr->memory + aligned_used);
    curr->used = aligned_used + size;
    return ptr;
}

void* zs_arena_alloc_zero(zs_arena_t* arena, usize size, usize alignment) {
    void* ptr = zs_arena_alloc(arena, size, alignment);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

char* zs_arena_strndup(zs_arena_t* arena, const char* str, usize len) {
    if (!str) return NULL;
    char* copy = (char*)zs_arena_alloc(arena, len + 1, 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

char* zs_arena_strdup(zs_arena_t* arena, const char* str) {
    if (!str) return NULL;
    return zs_arena_strndup(arena, str, strlen(str));
}
