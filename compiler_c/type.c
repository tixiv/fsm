
#include "type.h"
#include "common.h"
#include "string_builder.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

Type builtin_void = (Type){T_void};

Type builtin_u64 = (Type){T_unsigned_integer, 8, .integer.num_bits = 64};
Type builtin_i64 = (Type){T_signed_integer,   8, .integer.num_bits = 64};
Type builtin_u32 = (Type){T_unsigned_integer, 4, .integer.num_bits = 32};
Type builtin_i32 = (Type){T_signed_integer,   4, .integer.num_bits = 32};
Type builtin_u16 = (Type){T_unsigned_integer, 2, .integer.num_bits = 16};
Type builtin_i16 = (Type){T_signed_integer,   2, .integer.num_bits = 16};
Type builtin_u8 =  (Type){T_unsigned_integer, 1, .integer.num_bits =  8};
Type builtin_i8 =  (Type){T_signed_integer,   1, .integer.num_bits =  8};

Type builtin_u64_literal = (Type){T_unsigned_integer, 8, true, .integer.num_bits = 64};
Type builtin_i64_literal = (Type){T_signed_integer,   8, true, .integer.num_bits = 64};
Type builtin_u32_literal = (Type){T_unsigned_integer, 4, true, .integer.num_bits = 32};
Type builtin_i32_literal = (Type){T_signed_integer,   4, true, .integer.num_bits = 32};
Type builtin_u16_literal = (Type){T_unsigned_integer, 2, true, .integer.num_bits = 16};
Type builtin_i16_literal = (Type){T_signed_integer,   2, true, .integer.num_bits = 16};
Type builtin_u8_literal =  (Type){T_unsigned_integer, 1, true, .integer.num_bits =  8};
Type builtin_i8_literal =  (Type){T_signed_integer,   1, true, .integer.num_bits =  8};

Type *type_alloc(TypeKind kind) {
    Type *t = malloc(sizeof(Type));
    memset(t, 0, sizeof(Type));
    t->kind = kind;
    return t;
}

const char *get_type_name_r(char print_buf[1024], Type *type) {
    if (type == nullptr)       return "(null type)";
    if (type == &builtin_void) return "void";
    if (type == &builtin_u64)  return "u64";
    if (type == &builtin_i64)  return "i64";
    if (type == &builtin_u32)  return "u32";
    if (type == &builtin_i32)  return "i32";
    if (type == &builtin_u16)  return "u16";
    if (type == &builtin_i16)  return "i16";
    if (type == &builtin_u8)   return "u8";
    if (type == &builtin_i8)   return "i8";

    if (type == &builtin_u64_literal)  return "u64 literal";
    if (type == &builtin_i64_literal)  return "i64 literal";
    if (type == &builtin_u32_literal)  return "u32 literal";
    if (type == &builtin_i32_literal)  return "i32 literal";
    if (type == &builtin_u16_literal)  return "u16 literal";
    if (type == &builtin_i16_literal)  return "i16 literal";
    if (type == &builtin_u8_literal)   return "u8 literal";
    if (type == &builtin_i8_literal)   return "i8 literal";

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
        default:
            NOT_IMPLEMENTED("Dumping type kind %d is not implemented yet.\n", type->kind);
            break;
    }

    return sb.buffer;
}
