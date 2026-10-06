#include "zs_arena.h"
#include "zs_source.h"
#include "zs_token.h"
#include "zs_lexer.h"
#include "zs_ast.h"
#include "zs_parser.h"
#include "zs_types.h"
#include "zs_symtab.h"
#include "zs_checker.h"
#include "zs_codegen_arm64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

static void print_usage(const char* prog_name) {
    fprintf(stderr, "Uso: %s <archivo.Zs> [opciones]\n", prog_name);
    fprintf(stderr, "Opciones:\n");
    fprintf(stderr, "  -c, --emit-obj    Generar archivo objeto reubicable (.o)\n");
    fprintf(stderr, "  -o <archivo>      Especificar archivo de salida (binario, .s o .o)\n");
    fprintf(stderr, "  --emit-asm        Emitir ensamblador GNU ARM64 (.s)\n");
    fprintf(stderr, "  --freestanding    Enlazar con runtime autónomo Zs sin libc (_start)\n");
    fprintf(stderr, "  -h, --help        Mostrar este mensaje de ayuda\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_path = NULL;
    const char* output_path = NULL;
    bool emit_asm = false;
    bool emit_obj = false;
    bool freestanding = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--emit-obj") == 0) {
            emit_obj = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                fprintf(stderr, "Error: Se esperaba argumento tras '-o'\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            emit_asm = true;
        } else if (strcmp(argv[i], "--freestanding") == 0) {
            freestanding = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Opción desconocida: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "Error: No se especificó archivo fuente .Zs\n");
        return 1;
    }

    if (output_path) {
        size_t out_len = strlen(output_path);
        if (out_len >= 2 && strcmp(output_path + out_len - 2, ".s") == 0) {
            emit_asm = true;
        } else if (out_len >= 2 && strcmp(output_path + out_len - 2, ".o") == 0) {
            emit_obj = true;
        }
    } else {
        if (emit_asm) {
            output_path = "output.s";
        } else if (emit_obj) {
            // Derivar nombre .o a partir del nombre base de input_path
            static char derived_obj[256];
            snprintf(derived_obj, sizeof(derived_obj), "%s", input_path);
            char* dot = strrchr(derived_obj, '.');
            if (dot) strcpy(dot, ".o");
            else strcat(derived_obj, ".o");
            output_path = derived_obj;
        } else {
            output_path = "a.out";
        }
    }

    zs_arena_t* arena = zs_arena_create(2 * 1024 * 1024);
    if (!arena) {
        fprintf(stderr, "Error: Fallo al asignar memoria para el compilador Zs\n");
        return 1;
    }

    zs_source_file_t* source = zs_source_file_load(arena, input_path);
    if (!source) {
        fprintf(stderr, "Error: No se pudo abrir o leer el archivo fuente '%s'\n", input_path);
        zs_arena_destroy(arena);
        return 1;
    }

    // Fase 1: Análisis Léxico
    zs_lexer_t lexer;
    zs_lexer_init(&lexer, source, arena);

    // Fase 2: Análisis Sintáctico (AST)
    zs_parser_t parser;
    zs_parser_init(&parser, &lexer, arena);
    zs_ast_program_t* prog = zs_parser_parse_program(&parser);
    if (!prog || parser.error_count > 0) {
        fprintf(stderr, "Error de compilación: %u errores sintácticos detectados.\n", parser.error_count);
        zs_arena_destroy(arena);
        return 1;
    }

    // Fase 3: Análisis Semántico y Verificación de Tipos
    zs_checker_t checker;
    zs_checker_init(&checker, source, arena);
    if (!zs_checker_check_program(&checker, prog)) {
        fprintf(stderr, "Error de tipos: %u errores semánticos detectados.\n", checker.error_count);
        zs_arena_destroy(arena);
        return 1;
    }

    // Fase 4: Generación de Código ARM64
    zs_codegen_arm64_t cg;
    zs_codegen_init(&cg, arena);
    const char* asm_code = zs_codegen_generate(&cg, prog);
    if (!asm_code) {
        fprintf(stderr, "Error: Fallo en la generación de ensamblador ARM64.\n");
        zs_arena_destroy(arena);
        return 1;
    }

    if (emit_asm) {
        FILE* f = fopen(output_path, "w");
        if (!f) {
            fprintf(stderr, "Error: No se pudo escribir en el archivo de salida '%s'\n", output_path);
            zs_arena_destroy(arena);
            return 1;
        }
        fputs(asm_code, f);
        fclose(f);
        printf("Ensamblador generado con éxito: %s\n", output_path);
    } else if (emit_obj) {
        const char* tmp = getenv("TMPDIR");
        if (!tmp || tmp[0] == '\0') tmp = "/tmp";
        char tmp_asm[256];
        snprintf(tmp_asm, sizeof(tmp_asm), "%s/zsc_%d.s", tmp, getpid());
        FILE* f = fopen(tmp_asm, "w");
        if (!f) {
            fprintf(stderr, "Error al crear archivo de ensamblador temporal\n");
            zs_arena_destroy(arena);
            return 1;
        }
        fputs(asm_code, f);
        fclose(f);

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "as -o %s %s", output_path, tmp_asm);
        int res = system(cmd);
        remove(tmp_asm);

        if (res != 0) {
            fprintf(stderr, "Error al generar archivo objeto reubicable '%s'\n", output_path);
            zs_arena_destroy(arena);
            return 1;
        }
        printf("Objeto ELF generado con éxito: %s\n", output_path);
    } else {
        // Escribir ensamblador temporal y ensamblar con gcc/as
        const char* tmp = getenv("TMPDIR");
        if (!tmp || tmp[0] == '\0') tmp = "/tmp";
        char tmp_asm[256];
        snprintf(tmp_asm, sizeof(tmp_asm), "%s/zsc_%d.s", tmp, getpid());
        FILE* f = fopen(tmp_asm, "w");
        if (!f) {
            fprintf(stderr, "Error al crear archivo de ensamblador temporal\n");
            zs_arena_destroy(arena);
            return 1;
        }
        fputs(asm_code, f);
        fclose(f);

        char cmd[1024];
        if (freestanding) {
            snprintf(cmd, sizeof(cmd), "gcc -nostdlib -pie -o %s %s runtime/zs_start.s runtime/zs_runtime.s", output_path, tmp_asm);
        } else {
            snprintf(cmd, sizeof(cmd), "gcc -pie -o %s %s runtime/zs_runtime.s", output_path, tmp_asm);
        }

        int res = system(cmd);
        remove(tmp_asm);

        if (res != 0) {
            fprintf(stderr, "Error al ensamblar y enlazar binario final.\n");
            zs_arena_destroy(arena);
            return 1;
        }
        printf("Ejecutable compilado con éxito: %s\n", output_path);
    }

    zs_arena_destroy(arena);
    return 0;
}
