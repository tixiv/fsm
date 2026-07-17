
#include "type.h"
#include "ast.h"
#include "common.h"
#include "dyn_array.h"
#include "string_builder.h"
#include "sv.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

Type builtin_void = (Type){T_void};
Type builtin_bool = (Type){T_boolean, .storage_size = 8};

Type builtin_u64 = (Type){T_unsigned_integer, .storage_size = 8, .integer.num_bits = 64};
Type builtin_i64 = (Type){T_signed_integer,   .storage_size = 8, .integer.num_bits = 64};
Type builtin_u32 = (Type){T_unsigned_integer, .storage_size = 4, .integer.num_bits = 32};
Type builtin_i32 = (Type){T_signed_integer,   .storage_size = 4, .integer.num_bits = 32};
Type builtin_u16 = (Type){T_unsigned_integer, .storage_size = 2, .integer.num_bits = 16};
Type builtin_i16 = (Type){T_signed_integer,   .storage_size = 2, .integer.num_bits = 16};
Type builtin_u8 =  (Type){T_unsigned_integer, .storage_size = 1, .integer.num_bits =  8};
Type builtin_i8 =  (Type){T_signed_integer,   .storage_size = 1, .integer.num_bits =  8};


Type builtin_u8_reference = (Type){T_reference, .storage_size = 8, .reference.target_type = &builtin_u8};
static TypeMember builtin_u8_slice_members[2] = {{.name = mkSV("begin"), .type = &builtin_u8_reference}, {.name = mkSV("len"), .type = &builtin_i64}};
Type builtin_u8_slice = (Type){T_struct, .storage_size = 16, ._struct.num_members = 2, ._struct.members = builtin_u8_slice_members};

Type builtin_any = (Type){T_any, .storage_size = 0};

Type *type_alloc(TypeKind kind) {
    Type *t = malloc(sizeof(Type));
    memset(t, 0, sizeof(Type));
    t->kind = kind;
    return t;
}

static void print_array_type(SB *sb, Type *arr_t) {
    char child_print_buf[1024];

    Type *t = arr_t;
    while (is_array_kind(t)) t = t->_array.element_type;

    sb_printf(sb, "%s", get_type_name_r(child_print_buf, t));

    t = arr_t;
    while (is_array_kind(t)) {
         sb_printf(sb, "[%lu]", t->_array.n_elements);
         t = t->_array.element_type;
    }
}

const char *get_type_name_r(char print_buf[1024], Type *type) {
    if (type == nullptr)       return "(null type)";
    if (type == &builtin_void) return "void";
    if (type == &builtin_bool) return "bool";
    if (type == &builtin_any)  return "any";
    if (type == &builtin_u64)  return "u64";
    if (type == &builtin_i64)  return "i64";
    if (type == &builtin_u32)  return "u32";
    if (type == &builtin_i32)  return "i32";
    if (type == &builtin_u16)  return "u16";
    if (type == &builtin_i16)  return "i16";
    if (type == &builtin_u8)   return "u8";
    if (type == &builtin_i8)   return "i8";

    SB sb;
    sb_init(&sb, print_buf, 1024);

    char child_print_buf[1024];
    
    switch (type->kind) {
        case T_function:
            sb_printf(&sb, "fn (");
            Type **args = type->fun.argument_types;
            for (int i = 0; i < type->fun.num_arguments; i++) {
                sb_printf(&sb, "%s", get_type_name_r(child_print_buf, args[i]));
                if (i < type->fun.num_arguments-1) sb_printf(&sb, ", ");
            }
            sb_printf(&sb, ") :%s", get_type_name_r(child_print_buf, type->fun.return_type));
            break;
        case T_reference:
            sb_printf(&sb, "%s &", get_type_name_r(child_print_buf, type->reference.target_type));
            break;
        case T_array:
            print_array_type(&sb, type);
            break;
        case T_struct:
            if (is_slice_type(type)) {
                sb_printf(&sb, "%s[]", get_type_name_r(child_print_buf, dereferenced_type(type->_struct.members[0].type)));
            }
            else if (type->name.begin) {
                sb_printf(&sb, "struct %.*s", SV_prnt(type->name));
            }
            else {
                sb_printf(&sb, "anonymous struct", SV_prnt(type->name));
            }
            break;
        case T_enum:
            if (type->name.begin) {
                sb_printf(&sb, "enum %.*s", SV_prnt(type->name));
            }
            else {
                sb_printf(&sb, "anonymous enum", SV_prnt(type->name));
            }
            break;
        default:
            NOT_IMPLEMENTED("Dumping type kind %d is not implemented yet.\n", type->kind);
            break;
    }

    return sb.buffer;
}

bool is_boolean_kind(Type *t) {
    return t && t->kind == T_boolean;
}

bool is_integer_kind(Type *t) {
    return t && (t->kind == T_unsigned_integer || t->kind == T_signed_integer);
}

bool is_signed_integer(Type *t) {
    return t && (t->kind == T_signed_integer);
}

bool is_array_kind(Type *t) {
    return t->kind == T_array;
}

bool is_struct_kind(Type *t) {
    return t->kind == T_struct;
}

bool is_enum_kind(Type *t) {
    return t->kind == T_enum;
}

bool is_reference_kind(Type *t) {
    return t->kind == T_reference;
}

int get_ref_order(Type *t) {
    int ord = 0;
    while (is_reference_kind(t)) {
        ord++;
        t = t->reference.target_type;
    }
    return ord;
}

Type *dereferenced_type(Type *t) {
    char buf[1024];
    ASSERT(is_reference_kind(t), "Tried to dereference '%s' which is not a reference.\n",
        get_type_name_r(buf, t));

    return t->reference.target_type;
}

bool type_can_have_members(Type *t) {
    return t->kind == T_struct || t->kind == T_array || t->kind == T_slice;
}

size_t get_storage_size(Type *t) {
    return t->storage_size;
}

bool types_are_equivalent(Type *t1, Type *t2) {
    if (t1 == t2) return true;
    if (is_integer_kind(t1) && t1->kind == t2->kind && t1->integer.num_bits == t2->integer.num_bits) return true;
    
    return false;
}

bool is_castable_to_integer(Type *t, const char **out_warn) {
    if (out_warn) *out_warn = nullptr;

    if (t->kind == T_unsigned_integer || t->kind == T_signed_integer || t->kind == T_boolean)
    {
        return true;
    }
    return false;
}

bool is_castable_to(Type *to, Type *from, const char **out_warn) {
    if (out_warn) *out_warn = nullptr;
    if (is_integer_kind(to)) return is_castable_to_integer(from, out_warn);
    if (is_boolean_kind(to)) {
        if (from->kind == T_unsigned_integer || from->kind == T_signed_integer || from->kind == T_boolean || from->kind == T_reference)
        {
            return true;
        }
    }

    return false;
}

AST_node *make_cast(Type *to, Type *from) {
    ASSERT(is_castable_to(to, from, nullptr), "make_cast() should only be called if the cast is possible.\n");
    
    AST_node *n = ast_alloc(AST_cast, 0); // 'line_number' will be filled in when the cast is inserted.
    n->type = to;
    n->_cast.right_type = from;

    return n;
}

Type *get_member_type_and_offset(Type *_struct, SV *member_name, size_t *out_offset) {

    if (is_reference_kind(_struct)) _struct = dereferenced_type(_struct);
    ASSERT(_struct->kind == T_struct,"get_member_type_and_offset() called on something that is not a struct or a struct reference.\n")

    size_t offset = 0;
    for (int i = 0; i < _struct->_struct.num_members; i++) {
        TypeMember *member = &_struct->_struct.members[i];

        if (sv_equal(&member->name, member_name)) {
            if (out_offset) *out_offset = offset;
            return member->type;
        }
        offset += member->type->storage_size;
    }

    return nullptr;
}

bool get_enum_member_value (Type *t, SV *member_name, int64_t *enum_value) {
    for (int i = 0; i < t->_enum.num_members; i++) {
        EnumMember *member = &t->_enum.members[i];

        if (sv_equal(&member->name, member_name)) {
            if (enum_value) *enum_value = member->value;
            return true;
        }
    }
    return false;
}

void calculate_storage_size(Type *_struct) {
    ASSERT(_struct->kind == T_struct,"calculate_storage_size() called on something that is not a struct.\n")
    size_t offset = 0;
    for (int i = 0; i < _struct->_struct.num_members; i++) {
        TypeMember *member = &_struct->_struct.members[i];
        offset += member->type->storage_size;
    }
    _struct->storage_size = offset;
}

size_t get_function_arguments_size(Type *t) {
    ASSERT(t->kind == T_function,"get_function_arguments_size() called on something that is not a function.\n")
    size_t size = 0;
    for (int i = 0; i < t->fun.num_arguments; i++) {
        Type *arg = t->fun.argument_types[i];
        size += arg->storage_size;
        if (size % 8) size += 8 - (size % 8);
    }
    return size;
}

static Type *make_ref_type_for(Type *t) {
    Type *ref = type_alloc(T_reference);
    ref->reference.target_type = t;
    ref->storage_size = 8;
    return ref;
}

Dyn_array ref_types;

Type *get_ref_type_for(Type *t) {
    if (t == &builtin_u8) return &builtin_u8_reference;

    if (ref_types.capacity == 0)
        dyn_array_init(&ref_types, sizeof(Type*), 16);

    for (int i = 0; i < ref_types.count; i++) {
        Type *ref = ((Type**)ref_types.data)[i];
        if (ref->reference.target_type == t) return ref;
    }
    Type *ref = make_ref_type_for(t);
    dyn_array_push_p(&ref_types, ref);
    return ref;
}

static Type *make_array_type(Type *element_type, size_t n_elements) {
    Type *array_t = type_alloc(T_array);
    array_t->_array.element_type = element_type;
    array_t->_array.n_elements = n_elements;
    array_t->storage_size = element_type->storage_size * n_elements;
    return array_t;
}

Dyn_array array_types;

Type *get_array_type(Type *element_type, size_t n_elements) {
    if (array_types.capacity == 0)
        dyn_array_init(&array_types, sizeof(Type*), 16);

    for (int i = 0; i < array_types.count; i++) {
        Type *arr = ((Type**)array_types.data)[i];
        if (arr->_array.element_type == element_type
            && arr->_array.n_elements == n_elements) return arr;
    }
    Type *ref = make_array_type(element_type, n_elements);
    dyn_array_push_p(&array_types, ref);
    return ref;
}

static Type *make_slice_type(Type *element_type) {
    Type *slice_t = type_alloc(T_struct);
    slice_t->_struct.num_members = 2;

    size_t size = 2 * sizeof(TypeMember);
    slice_t->_struct.members = malloc(size);
    slice_t->_struct.members[0].type = get_ref_type_for(element_type);
    slice_t->_struct.members[0].name = mkSV("begin");
    slice_t->_struct.members[1].type = &builtin_i64;
    slice_t->_struct.members[1].name = mkSV("len");

    calculate_storage_size(slice_t);
    return slice_t;
}

Dyn_array slice_types;

Type *get_sclice_type(Type *element_type) {
    if (element_type == &builtin_u8) return &builtin_u8_slice;
    
    if (slice_types.capacity == 0)
        dyn_array_init(&slice_types, sizeof(Type*), 16);

    for (int i = 0; i < slice_types.count; i++) {
        Type *slice = ((Type**)slice_types.data)[i];
        if (slice->_struct.members[0].type == get_ref_type_for(element_type))
            return slice;
    }
    Type *slice = make_slice_type(element_type);
    dyn_array_push_p(&slice_types, slice);
    return slice;
}

bool is_slice_type(Type *t) {
    if (t == &builtin_u8_slice) return true;
    
    for (int i = 0; i < slice_types.count; i++) {
        Type *slice = ((Type**)slice_types.data)[i];
        if (t == slice) return true;
    }
    return false;
}

Type *get_slice_element_type(Type *t) {
    ASSERT(is_slice_type(t), "Tried to get slice element type for non slice type.\n");
    return dereferenced_type(t->_struct.members[0].type);
}

size_t get_stack_offset_for(Type *t) {
    ASSERT(t, "get_stack_offset_for() called with nullptr.\n");
    int size = t->storage_size;

    //char buf[1024];
    //printf("%s: %s %d %d", __FUNCTION__, get_type_name_r(buf, t), size, ((size + 7) / 8) * 8);

    return ((size + 7) / 8) * 8;
}
