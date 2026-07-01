
#include "type.h"

BuiltinTypes_t builtin_types[] = {
    {"u64", {T_unsigned_integer, 8, .integer.num_bits = 64}},
    {"i64", {T_signed_integer,   8, .integer.num_bits = 64}},
    {"u32", {T_unsigned_integer, 4, .integer.num_bits = 32}},
    {"i32", {T_signed_integer,   4, .integer.num_bits = 32}},
    {"u16", {T_unsigned_integer, 2, .integer.num_bits = 16}},
    {"i16", {T_signed_integer,   2, .integer.num_bits = 16}},
    {"u8",  {T_unsigned_integer, 1, .integer.num_bits = 8}},
    {"i8",  {T_signed_integer,   1, .integer.num_bits = 8}},
};
