#ifndef ZS_AST_H
#define ZS_AST_H

#include "zs_token.h"

typedef struct zs_ast_type zs_ast_type_t;
typedef struct zs_ast_expr zs_ast_expr_t;
typedef struct zs_ast_stmt zs_ast_stmt_t;
typedef struct zs_ast_decl zs_ast_decl_t;

// --- TIPOS ---
typedef enum {
    TYPE_KIND_PRIMITIVE,
    TYPE_KIND_LOC,        // loc<T>
    TYPE_KIND_VIEW,       // view<T>
    TYPE_KIND_ARRAY,      // T[N]
    TYPE_KIND_TUPLE,      // (T1, T2)
    TYPE_KIND_NAMED       // PathIdentifier
} zs_type_kind_t;

struct zs_ast_type {
    zs_type_kind_t kind;
    zs_loc_t loc;
    union {
        zs_token_kind_t primitive_token; // TOK_KW_I32, TOK_KW_STR, etc.
        zs_ast_type_t* sub_type;        // para loc<T>, view<T>
        struct {
            zs_ast_type_t* elem_type;
            zs_ast_expr_t* size_expr;
        } array;
        struct {
            zs_ast_type_t** types;
            u32 count;
        } tuple;
        struct {
            const char** parts;
            u32 count;
        } named;
    };
};

// --- EXPRESIONES ---
typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_STRING_LIT,
    EXPR_CHAR_LIT,
    EXPR_BOOL_LIT,
    EXPR_NONE_LIT,
    EXPR_IDENT,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_CAST,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_SLICE,
    EXPR_FIELD,
    EXPR_RECORD_INIT,
    EXPR_ARRAY_LIT,
    EXPR_TUPLE_LIT,
    EXPR_OR_FALLBACK,
    EXPR_CATCH,
    EXPR_RETURN_FAIL
} zs_expr_kind_t;

struct zs_ast_expr {
    zs_expr_kind_t kind;
    zs_loc_t loc;
    union {
        u64 int_val;
        f64 float_val;
        struct {
            const char* str;
            usize len;
        } string_val;
        char32 char_val;
        bool bool_val;
        const char* ident;

        struct {
            zs_token_kind_t op;
            zs_ast_expr_t* expr;
        } unary;

        struct {
            zs_token_kind_t op;
            zs_ast_expr_t* left;
            zs_ast_expr_t* right;
        } binary;

        struct {
            zs_ast_expr_t* expr;
            zs_ast_type_t* target_type;
        } cast;

        struct {
            zs_ast_expr_t* callee;
            zs_ast_expr_t** args;
            u32 arg_count;
        } call;

        struct {
            zs_ast_expr_t* target;
            zs_ast_expr_t* index;
        } index;

        struct {
            zs_ast_expr_t* target;
            zs_ast_expr_t* start;
            zs_ast_expr_t* end;
        } slice;

        struct {
            zs_ast_expr_t* target;
            const char* field_name;
        } field;

        struct {
            const char* type_name;
            const char** field_names;
            zs_ast_expr_t** field_values;
            u32 field_count;
        } record_init;

        struct {
            zs_ast_expr_t** elems;
            u32 count;
        } array_lit;

        struct {
            zs_ast_expr_t** elems;
            u32 count;
        } tuple_lit;

        struct {
            zs_ast_expr_t* primary;
            zs_ast_expr_t* fallback;
        } or_fallback;

        struct {
            zs_ast_expr_t* primary;
            const char* err_var;
            struct zs_ast_block* catch_block;
        } catch_expr;
    };
};

// --- BLOQUE Y SENTENCIAS ---
typedef struct zs_ast_block {
    zs_ast_stmt_t** stmts;
    u32 stmt_count;
} zs_ast_block_t;

typedef enum {
    STMT_VAL_DECL,
    STMT_VAR_DECL,
    STMT_ANCHOR_DECL,
    STMT_ASSIGN,
    STMT_DEFER,
    STMT_IF,
    STMT_LOOP,
    STMT_WHILE,
    STMT_WALK,
    STMT_BRANCH,
    STMT_RETURN,
    STMT_FAIL,
    STMT_TRAP,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RAW,
    STMT_BLOCK,
    STMT_EXPR
} zs_stmt_kind_t;

typedef struct {
    enum {
        PAT_LITERAL,
        PAT_IDENT,
        PAT_CHOICE,
        PAT_RANGE,
        PAT_ELSE
    } kind;
    zs_ast_expr_t* expr1;
    zs_ast_expr_t* expr2;
    const char* ident;
    const char** sub_idents;
    u32 sub_count;
} zs_ast_pattern_t;

typedef struct {
    zs_ast_pattern_t pattern;
    zs_ast_block_t* block;
    zs_ast_stmt_t* single_stmt;
} zs_ast_branch_arm_t;

struct zs_ast_stmt {
    zs_stmt_kind_t kind;
    zs_loc_t loc;
    union {
        struct {
            const char* name;
            zs_ast_type_t* type;
            zs_ast_expr_t* init;
        } var_decl; // para val y var

        struct {
            const char* name;
            zs_ast_type_t* elem_type;
            zs_ast_expr_t* size_expr;
            zs_ast_expr_t* init_expr;
        } anchor_decl;

        struct {
            zs_ast_expr_t* lvalue;
            zs_token_kind_t op;
            zs_ast_expr_t* value;
        } assign;

        struct {
            zs_ast_block_t* block;
            zs_ast_stmt_t* single_stmt;
        } defer_stmt;

        struct {
            zs_ast_expr_t* cond;
            zs_ast_block_t* then_block;
            zs_ast_block_t* else_block;
            zs_ast_stmt_t* else_if;
        } if_stmt;

        struct {
            zs_ast_block_t* body;
        } loop_stmt;

        struct {
            zs_ast_expr_t* cond;
            zs_ast_block_t* body;
        } while_stmt;

        struct {
            const char* var_name;
            zs_ast_expr_t* iterable;
            zs_ast_expr_t* step;
            zs_ast_block_t* body;
        } walk_stmt;

        struct {
            zs_ast_expr_t* target;
            zs_ast_branch_arm_t* arms;
            u32 arm_count;
        } branch_stmt;

        zs_ast_expr_t* return_expr;
        zs_ast_expr_t* fail_expr;
        zs_ast_expr_t* trap_expr;
        zs_ast_expr_t* expr;
        zs_ast_block_t* raw_block;
        zs_ast_block_t* block_stmt;
    };
};

// --- DECLARACIONES DE ALTO NIVEL ---
typedef enum {
    DECL_FUNCTION,
    DECL_ANCHOR,
    DECL_RECORD,
    DECL_CHOICE,
    DECL_CONTRACT,
    DECL_CONST,
    DECL_TYPE_ALIAS,
    DECL_FOREIGN,
    DECL_COMPTIME
} zs_decl_kind_t;

typedef struct {
    const char* name;
    zs_ast_type_t* type;
    bool is_var;
} zs_ast_param_t;

typedef struct {
    const char* name;
    zs_ast_type_t* type;
    bool is_pub;
} zs_ast_record_field_t;

typedef struct {
    const char* name;
    zs_ast_type_t** payload_types;
    u32 payload_count;
} zs_ast_choice_variant_t;

typedef struct {
    const char* name;
    zs_ast_param_t* params;
    u32 param_count;
    zs_ast_type_t* return_type;
    zs_ast_type_t* error_type;
} zs_ast_fn_sig_t;

struct zs_ast_decl {
    zs_decl_kind_t kind;
    zs_loc_t loc;
    bool is_pub;
    union {
        struct {
            const char* name;
            zs_ast_param_t* params;
            u32 param_count;
            zs_ast_type_t* return_type;
            zs_ast_type_t* error_type;
            zs_ast_block_t* body;
        } function;

        struct {
            const char* name;
            zs_ast_type_t* elem_type;
            zs_ast_expr_t* size_expr;
            zs_ast_expr_t* init_expr;
        } anchor;

        struct {
            const char* name;
            u32 alignment;
            zs_ast_record_field_t* fields;
            u32 field_count;
        } record;

        struct {
            const char* name;
            zs_ast_choice_variant_t* variants;
            u32 variant_count;
        } choice;

        struct {
            const char* name;
            zs_ast_fn_sig_t* methods;
            u32 method_count;
        } contract;

        struct {
            const char* name;
            zs_ast_type_t* type;
            zs_ast_expr_t* value;
        } const_decl;

        struct {
            const char* name;
            zs_ast_type_t* target_type;
        } type_alias;

        struct {
            const char* abi_name;
            zs_ast_fn_sig_t* functions;
            u32 function_count;
        } foreign_block;

        zs_ast_block_t* comptime_block;
    };
};

// --- PROGRAMA / UNIDAD DE COMPILACIÓN ---
typedef struct {
    const char* module_name;
    const char** imports;
    u32 import_count;
    zs_ast_decl_t** decls;
    u32 decl_count;
} zs_ast_program_t;

// Constructores de AST
zs_ast_type_t* zs_ast_type_primitive(zs_arena_t* arena, zs_loc_t loc, zs_token_kind_t prim_tok);
zs_ast_type_t* zs_ast_type_loc(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* sub);
zs_ast_type_t* zs_ast_type_view(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* sub);
zs_ast_type_t* zs_ast_type_array(zs_arena_t* arena, zs_loc_t loc, zs_ast_type_t* elem, zs_ast_expr_t* sz);
zs_ast_type_t* zs_ast_type_named(zs_arena_t* arena, zs_loc_t loc, const char* name);

zs_ast_expr_t* zs_ast_expr_create(zs_arena_t* arena, zs_expr_kind_t kind, zs_loc_t loc);
zs_ast_stmt_t* zs_ast_stmt_create(zs_arena_t* arena, zs_stmt_kind_t kind, zs_loc_t loc);
zs_ast_decl_t* zs_ast_decl_create(zs_arena_t* arena, zs_decl_kind_t kind, zs_loc_t loc, bool is_pub);

#endif // ZS_AST_H
