#ifndef ZS_CHECKER_H
#define ZS_CHECKER_H

#include "zs_ast.h"
#include "zs_types.h"
#include "zs_symtab.h"

typedef struct {
    zs_arena_t* arena;
    zs_symtab_t* symtab;
    const zs_source_file_t* file;
    u32 error_count;
    zs_type_t* current_fn_ret_type;
    zs_type_t* current_fn_err_type;
} zs_checker_t;

void zs_checker_init(zs_checker_t* checker, const zs_source_file_t* file, zs_arena_t* arena);
bool zs_checker_check_program(zs_checker_t* checker, zs_ast_program_t* program);

// Verificaciones semánticas individuales
zs_type_t* zs_checker_check_expr(zs_checker_t* checker, zs_ast_expr_t* expr);
void zs_checker_check_stmt(zs_checker_t* checker, zs_ast_stmt_t* stmt);

#endif // ZS_CHECKER_H
