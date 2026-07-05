
#include "type.h"
#include "ast.h"
#include "common.h"
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
Type builtin_u8_array = (Type){T_array,         .storage_size = 8, .array.element_type = &builtin_u8};

Type *type_alloc(TypeKind kind) {
    Type *t = malloc(sizeof(Type));
    memset(t, 0, sizeof(Type));
    t->kind = kind;
    return t;
}

const char *get_type_name_r(char print_buf[1024], Type *type) {
    if (type == nullptr)       return "(null type)";
    if (type == &builtin_void) return "void";
    if (type == &builtin_bool) return "bool";
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
            sb_printf(&sb, "%s[]", get_type_name_r(child_print_buf, type->reference.target_type));
            break;
        case T_struct:
            sb_printf(&sb, "struct %.*s", SV_prnt(type->name));
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

bool is_array_kind(Type *t) {
    return t->kind == T_array;
}

bool is_reference_kind(Type *t) {
    return t->kind == T_reference;
}

Type *dereferenced_type(Type *t) {
    char buf[1024];
    ASSERT(is_reference_kind(t), "Tried to dereference '%s' which is not a reference.\n",
        get_type_name_r(buf, t));

    return t->reference.target_type;
}

bool type_can_have_members(Type *t) {
    return t->kind == T_struct || (is_reference_kind(t) && dereferenced_type(t)->kind == T_struct);
}

size_t get_storage_size(Type *t) {
    return t->storage_size;
}

bool types_are_equivalent(Type *t1, Type *t2) {
    if (t1 == t2) return true;
    if (is_integer_kind(t1) && t1->kind == t2->kind && t1->integer.num_bits == t2->integer.num_bits) return true;
    
    return false;
}

Type *get_ref_type_for_array_type(Type *t) {
    char buf[1024];
    ASSERT(is_array_kind(t), "Called get_ref_type_for_array_type on something that is not an array type '%s'.\n",
        get_type_name_r(buf, t));

    if (t == &builtin_u8_array) return &builtin_u8_reference;

    NOT_IMPLEMENTED("get_ref_type_for_array_type() is not implemented yet for %s\n", get_type_name_r(buf, t));
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

void calculate_storage_size(Type *_struct) {
    ASSERT(_struct->kind == T_struct,"calculate_storage_size() called on something that is not a struct.\n")
    size_t offset = 0;
    for (int i = 0; i < _struct->_struct.num_members; i++) {
        TypeMember *member = &_struct->_struct.members[i];
        offset += member->type->storage_size;
    }
    _struct->storage_size = offset;
}

Type *make_ref_to(Type *t) {
    // leaky memory, I don't care for now
    Type *ref = type_alloc(T_reference);
    ref->reference.target_type = t;
    ref->storage_size = 8;
    return ref;
}

bool type_should_be_handled_as_ref(Type *t) {
    if (t->kind == T_struct) return true;
    return false;
}
