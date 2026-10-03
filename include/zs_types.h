#ifndef ZS_TYPES_H
#define ZS_TYPES_H

#include "zs_common.h"
#include "zs_token.h"

typedef struct zs_type zs_type_t;

typedef enum {
    ZS_TYPE_PRIMITIVE,
    ZS_TYPE_LOC,       // loc<T>
    ZS_TYPE_VIEW,      // view<T>
    ZS_TYPE_ANCHOR,    // anchor T[N]
    ZS_TYPE_ARRAY,     // T[N]
    ZS_TYPE_RECORD,    // record Name { ... }
    ZS_TYPE_CHOICE,    // choice Name { ... }
    ZS_TYPE_CONTRACT,  // contract Name { ... }
    ZS_TYPE_FUNCTION,  // fn(...) -> T or E
    ZS_TYPE_TUPLE,     // (T1, T2)
    ZS_TYPE_VOID
} zs_type_category_t;

typedef struct {
    const char* name;
    zs_type_t* type;
    usize offset;
    bool is_pub;
} zs_type_record_field_t;

typedef struct {
    const char* name;
    u32 tag;
    zs_type_t** payload_types;
    u32 payload_count;
} zs_type_choice_variant_t;

typedef struct {
    const char* name;
    zs_type_t* fn_type;
} zs_type_contract_method_t;

struct zs_type {
    zs_type_category_t category;
    usize size;
    usize alignment;

    union {
        zs_token_kind_t primitive_kind; // TOK_KW_I32, TOK_KW_STR, etc.
        zs_type_t* sub_type;           // para loc<T>, view<T>

        struct {
            zs_type_t* elem_type;
            usize count;
        } anchor;

        struct {
            zs_type_t* elem_type;
            usize count;
        } array;

        struct {
            const char* name;
            zs_type_record_field_t* fields;
            u32 field_count;
        } record;

        struct {
            const char* name;
            zs_type_choice_variant_t* variants;
            u32 variant_count;
            usize tag_size;
        } choice;

        struct {
            const char* name;
            zs_type_contract_method_t* methods;
            u32 method_count;
        } contract;

        struct {
            zs_type_t** param_types;
            u32 param_count;
            zs_type_t* return_type;
            zs_type_t* error_type;
        } function;

        struct {
            zs_type_t** elem_types;
            u32 count;
        } tuple;
    };
};

// Constructores de tipos
zs_type_t* zs_type_get_primitive(zs_arena_t* arena, zs_token_kind_t kind);
zs_type_t* zs_type_make_loc(zs_arena_t* arena, zs_type_t* sub);
zs_type_t* zs_type_make_view(zs_arena_t* arena, zs_type_t* sub);
zs_type_t* zs_type_make_anchor(zs_arena_t* arena, zs_type_t* elem, usize count);
zs_type_t* zs_type_make_record(zs_arena_t* arena, const char* name, zs_type_record_field_t* fields, u32 count, usize explicit_align);
zs_type_t* zs_type_make_choice(zs_arena_t* arena, const char* name, zs_type_choice_variant_t* variants, u32 count);
zs_type_t* zs_type_make_contract(zs_arena_t* arena, const char* name, zs_type_contract_method_t* methods, u32 count);
zs_type_t* zs_type_make_function(zs_arena_t* arena, zs_type_t** params, u32 pcount, zs_type_t* ret, zs_type_t* err);

bool zs_type_equals(const zs_type_t* a, const zs_type_t* b);
const char* zs_type_to_string(zs_arena_t* arena, const zs_type_t* type);

#endif // ZS_TYPES_H
