#include "zs_source.h"
#include <stdarg.h>

static void zs_source_build_line_offsets(zs_arena_t* arena, zs_source_file_t* file) {
    u32 capacity = 128;
    file->line_offsets = (u32*)zs_arena_alloc(arena, capacity * sizeof(u32), sizeof(u32));
    file->line_count = 0;

    // La primera línea comienza en el offset 0
    file->line_offsets[file->line_count++] = 0;

    for (usize i = 0; i < file->length; ++i) {
        if (file->content[i] == '\n') {
            if (file->line_count >= capacity) {
                u32 new_cap = capacity * 2;
                u32* new_table = (u32*)zs_arena_alloc(arena, new_cap * sizeof(u32), sizeof(u32));
                memcpy(new_table, file->line_offsets, capacity * sizeof(u32));
                file->line_offsets = new_table;
                capacity = new_cap;
            }
            file->line_offsets[file->line_count++] = (u32)(i + 1);
        }
    }
}

zs_source_file_t* zs_source_file_from_string(zs_arena_t* arena, const char* filename, const char* content, usize len) {
    zs_source_file_t* file = (zs_source_file_t*)zs_arena_alloc_zero(arena, sizeof(zs_source_file_t), sizeof(void*));
    file->filename = zs_arena_strdup(arena, filename);
    file->content = content;
    file->length = len;
    zs_source_build_line_offsets(arena, file);
    return file;
}

zs_source_file_t* zs_source_file_load(zs_arena_t* arena, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        return NULL;
    }

    char* buffer = (char*)zs_arena_alloc(arena, (usize)sz + 1, 1);
    size_t read_bytes = fread(buffer, 1, (size_t)sz, fp);
    fclose(fp);

    buffer[read_bytes] = '\0';
    return zs_source_file_from_string(arena, filename, buffer, read_bytes);
}

zs_loc_t zs_source_get_loc(const zs_source_file_t* file, u32 offset) {
    zs_loc_t loc = {1, 1, offset};
    if (!file || file->line_count == 0) return loc;

    // Búsqueda binaria en la tabla de líneas
    u32 low = 0;
    u32 high = file->line_count - 1;

    while (low <= high) {
        u32 mid = low + (high - low) / 2;
        if (file->line_offsets[mid] <= offset) {
            loc.line = mid + 1;
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    u32 line_start = file->line_offsets[loc.line - 1];
    loc.col = (offset >= line_start) ? (offset - line_start + 1) : 1;
    return loc;
}

void zs_source_print_error(const zs_source_file_t* file, zs_loc_t loc, const char* fmt, ...) {
    fprintf(stderr, "\033[1;31merror:\033[0m \033[1m%s:%u:%u:\033[0m ",
            file ? file->filename : "<desconocido>", loc.line, loc.col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    if (!file || loc.line == 0 || loc.line > file->line_count) return;

    // Imprimir la línea del error
    u32 start = file->line_offsets[loc.line - 1];
    u32 end = (loc.line < file->line_count) ? file->line_offsets[loc.line] : (u32)file->length;

    fprintf(stderr, "%5u | ", loc.line);
    for (u32 i = start; i < end; ++i) {
        char c = file->content[i];
        if (c == '\r' || c == '\n') break;
        fputc(c, stderr);
    }
    fprintf(stderr, "\n");

    // Imprimir el indicador ^
    fprintf(stderr, "      | ");
    for (u32 c = 1; c < loc.col; ++c) {
        fputc(' ', stderr);
    }
    fprintf(stderr, "\033[1;32m^\033[0m\n");
}
