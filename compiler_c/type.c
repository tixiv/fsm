
#include "type.h"
#include <malloc.h>
#include <string.h>

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
