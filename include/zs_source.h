#ifndef ZS_SOURCE_H
#define ZS_SOURCE_H

#include "zs_common.h"
#include "zs_arena.h"

typedef struct {
    u32 line;
    u32 col;
    u32 offset;
} zs_loc_t;

typedef struct {
    const char* filename;
    const char* content;
    usize length;
    // Tabla de líneas para búsqueda rápida de líneas y columnas
    u32* line_offsets;
    u32 line_count;
} zs_source_file_t;

zs_source_file_t* zs_source_file_load(zs_arena_t* arena, const char* filename);
zs_source_file_t* zs_source_file_from_string(zs_arena_t* arena, const char* filename, const char* content, usize len);

zs_loc_t zs_source_get_loc(const zs_source_file_t* file, u32 offset);
void zs_source_print_error(const zs_source_file_t* file, zs_loc_t loc, const char* fmt, ...);

#endif // ZS_SOURCE_H
