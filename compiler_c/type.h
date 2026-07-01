
#pragma once

#include <stddef.h>

typedef enum {
    T_unsigned_integer,
    T_signed_integer,
    T_pointer,
} TypeKind;

typedef struct Type_s {
    TypeKind kind;
    size_t storage_size;
    union {
        struct {
            size_t num_bits;
        } integer;
        
        struct {
            struct Type_s *target_type;
        } pointer;
    };
} Type;

typedef struct {
    const char *name;
    Type type;
} BuiltinTypes_t;
