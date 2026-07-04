
#include "type.h"
#include "ast.h"
#include "common.h"
#include "string_builder.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

Type builtin_void = (Type){T_void};
Type builtin_bool = (Type){T_boolean};

Type builtin_u64 = (Type){T_unsigned_integer, .integer.storage_size = 8, .integer.num_bits = 64};
Type builtin_i64 = (Type){T_signed_integer,   .integer.storage_size = 8, .integer.num_bits = 64};
Type builtin_u32 = (Type){T_unsigned_integer, .integer.storage_size = 4, .integer.num_bits = 32};
Type builtin_i32 = (Type){T_signed_integer,   .integer.storage_size = 4, .integer.num_bits = 32};
Type builtin_u16 = (Type){T_unsigned_integer, .integer.storage_size = 2, .integer.num_bits = 16};
Type builtin_i16 = (Type){T_signed_integer,   .integer.storage_size = 2, .integer.num_bits = 16};
Type builtin_u8 =  (Type){T_unsigned_integer, .integer.storage_size = 1, .integer.num_bits =  8};
Type builtin_i8 =  (Type){T_signed_integer,   .integer.storage_size = 1, .integer.num_bits =  8};

Type builtin_u8_reference = (Type){T_reference, .reference.target_type = &builtin_u8};
Type builtin_u8_array = (Type){T_array, .array.element_type = &builtin_u8};

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

    if (t->kind == T_reference)
    {
        if (out_warn) *out_warn = "Implicitly casting pointer to integer";
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
