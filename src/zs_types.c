#include "zs_types.h"

static inline usize align_up(usize val, usize align) {
    if (align == 0) return val;
    return (val + align - 1) & ~(align - 1);
}

zs_type_t* zs_type_get_primitive(zs_arena_t* arena, zs_token_kind_t kind) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));

    if (kind == TOK_KW_VOID) {
        t->category = ZS_TYPE_VOID;
        t->size = 0;
        t->alignment = 1;
        return t;
    }

    t->category = ZS_TYPE_PRIMITIVE;
    t->primitive_kind = kind;

    switch (kind) {
        case TOK_KW_I8:
        case TOK_KW_U8:
        case TOK_KW_BOOL:
            t->size = 1;
            t->alignment = 1;
            break;

        case TOK_KW_I16:
        case TOK_KW_U16:
            t->size = 2;
            t->alignment = 2;
            break;

        case TOK_KW_I32:
        case TOK_KW_U32:
        case TOK_KW_F32:
        case TOK_KW_CHAR32:
            t->size = 4;
            t->alignment = 4;
            break;

        case TOK_KW_I64:
        case TOK_KW_U64:
        case TOK_KW_ISIZE:
        case TOK_KW_USIZE:
        case TOK_KW_F64:
            t->size = 8;
            t->alignment = 8;
            break;

        case TOK_KW_STR:
            t->size = 16;       // { ptr: 8 bytes, len: 8 bytes }
            t->alignment = 8;
            break;

        case TOK_KW_STRING:
            t->size = 24;       // { ptr: 8 bytes, len: 8 bytes, cap: 8 bytes }
            t->alignment = 8;
            break;

        default:
            t->size = 8;
            t->alignment = 8;
            break;
    }

    return t;
}

zs_type_t* zs_type_make_loc(zs_arena_t* arena, zs_type_t* sub) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_LOC;
    t->size = 8;       // Puntero de 64 bits en hardware ARM64
    t->alignment = 8;
    t->sub_type = sub;
    return t;
}

zs_type_t* zs_type_make_view(zs_arena_t* arena, zs_type_t* sub) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_VIEW;
    t->size = 16;      // Par { ptr: 8 bytes, len: 8 bytes }
    t->alignment = 8;
    t->sub_type = sub;
    return t;
}

zs_type_t* zs_type_make_anchor(zs_arena_t* arena, zs_type_t* elem, usize count) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_ANCHOR;
    t->alignment = elem->alignment;
    t->size = elem->size * count;
    t->anchor.elem_type = elem;
    t->anchor.count = count;
    return t;
}

zs_type_t* zs_type_make_record(zs_arena_t* arena, const char* name, zs_type_record_field_t* fields, u32 count, usize explicit_align) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_RECORD;
    t->record.name = name;
    t->record.fields = fields;
    t->record.field_count = count;

    usize max_align = explicit_align ? explicit_align : 1;
    usize curr_offset = 0;

    for (u32 i = 0; i < count; ++i) {
        usize field_align = fields[i].type->alignment;
        if (field_align > max_align && explicit_align == 0) {
            max_align = field_align;
        }
        curr_offset = align_up(curr_offset, field_align);
        fields[i].offset = curr_offset;
        curr_offset += fields[i].type->size;
    }

    t->alignment = max_align;
    t->size = align_up(curr_offset, max_align);
    return t;
}

zs_type_t* zs_type_make_choice(zs_arena_t* arena, const char* name, zs_type_choice_variant_t* variants, u32 count) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_CHOICE;
    t->choice.name = name;
    t->choice.variants = variants;
    t->choice.variant_count = count;
    t->choice.tag_size = 4; // Tag u32 de 4 bytes para variantes

    usize max_payload_size = 0;
    usize max_payload_align = 4; // mínimo alineación del tag

    for (u32 i = 0; i < count; ++i) {
        variants[i].tag = i;
        usize psize = 0;
        for (u32 j = 0; j < variants[i].payload_count; ++j) {
            zs_type_t* pt = variants[i].payload_types[j];
            if (pt->alignment > max_payload_align) {
                max_payload_align = pt->alignment;
            }
            psize += pt->size;
        }
        if (psize > max_payload_size) {
            max_payload_size = psize;
        }
    }

    t->alignment = max_payload_align;
    t->size = align_up(4 + max_payload_size, max_payload_align);
    return t;
}

zs_type_t* zs_type_make_contract(zs_arena_t* arena, const char* name, zs_type_contract_method_t* methods, u32 count) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_CONTRACT;
    t->contract.name = name;
    t->contract.methods = methods;
    t->contract.method_count = count;
    t->size = 16;      // Par { data_ptr, vtable_ptr } si fuera dinámico, o estático
    t->alignment = 8;
    return t;
}

zs_type_t* zs_type_make_function(zs_arena_t* arena, zs_type_t** params, u32 pcount, zs_type_t* ret, zs_type_t* err) {
    zs_type_t* t = (zs_type_t*)zs_arena_alloc_zero(arena, sizeof(zs_type_t), sizeof(void*));
    t->category = ZS_TYPE_FUNCTION;
    t->function.param_types = params;
    t->function.param_count = pcount;
    t->function.return_type = ret;
    t->function.error_type = err;
    t->size = 8;       // Puntero a función
    t->alignment = 8;
    return t;
}

bool zs_type_equals(const zs_type_t* a, const zs_type_t* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->category != b->category) return false;

    switch (a->category) {
        case ZS_TYPE_VOID:
            return true;

        case ZS_TYPE_PRIMITIVE:
            return a->primitive_kind == b->primitive_kind;

        case ZS_TYPE_LOC:
        case ZS_TYPE_VIEW:
            return zs_type_equals(a->sub_type, b->sub_type);

        case ZS_TYPE_ANCHOR:
            return (a->anchor.count == b->anchor.count) &&
                   zs_type_equals(a->anchor.elem_type, b->anchor.elem_type);

        case ZS_TYPE_ARRAY:
            return (a->array.count == b->array.count) &&
                   zs_type_equals(a->array.elem_type, b->array.elem_type);

        case ZS_TYPE_RECORD:
            // Tipado nominal: dos records son iguales si comparten el mismo nombre
            return strcmp(a->record.name, b->record.name) == 0;

        case ZS_TYPE_CHOICE:
            // Tipado nominal
            return strcmp(a->choice.name, b->choice.name) == 0;

        case ZS_TYPE_CONTRACT:
            return strcmp(a->contract.name, b->contract.name) == 0;

        case ZS_TYPE_FUNCTION: {
            if (a->function.param_count != b->function.param_count) return false;
            for (u32 i = 0; i < a->function.param_count; ++i) {
                if (!zs_type_equals(a->function.param_types[i], b->function.param_types[i])) return false;
            }
            if (!zs_type_equals(a->function.return_type, b->function.return_type)) return false;
            if ((a->function.error_type == NULL) != (b->function.error_type == NULL)) return false;
            if (a->function.error_type && !zs_type_equals(a->function.error_type, b->function.error_type)) return false;
            return true;
        }

        default:
            return false;
    }
}

const char* zs_type_to_string(zs_arena_t* arena, const zs_type_t* type) {
    if (!type) return "<desconocido>";

    switch (type->category) {
        case ZS_TYPE_VOID: return "void";
        case ZS_TYPE_PRIMITIVE: return zs_token_kind_name(type->primitive_kind);
        case ZS_TYPE_LOC: {
            const char* sub = zs_type_to_string(arena, type->sub_type);
            char buf[128];
            snprintf(buf, sizeof(buf), "loc<%s>", sub);
            return zs_arena_strdup(arena, buf);
        }
        case ZS_TYPE_VIEW: {
            const char* sub = zs_type_to_string(arena, type->sub_type);
            char buf[128];
            snprintf(buf, sizeof(buf), "view<%s>", sub);
            return zs_arena_strdup(arena, buf);
        }
        case ZS_TYPE_ANCHOR: {
            const char* elem = zs_type_to_string(arena, type->anchor.elem_type);
            char buf[128];
            snprintf(buf, sizeof(buf), "anchor %s[%zu]", elem, type->anchor.count);
            return zs_arena_strdup(arena, buf);
        }
        case ZS_TYPE_RECORD: return type->record.name;
        case ZS_TYPE_CHOICE: return type->choice.name;
        case ZS_TYPE_CONTRACT: return type->contract.name;
        case ZS_TYPE_FUNCTION: return "function";
        default: return "<tipo>";
    }
}
