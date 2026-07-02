
#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    T_void,
    T_unsigned_integer,
    T_signed_integer,
    T_pointer,
    T_function,
    T_boolean
} TypeKind;

typedef struct Type_s {
    TypeKind kind;
    size_t storage_size;
    bool is_literal;
    union {
        struct {
            size_t num_bits;
        } integer;
        
        struct {
            struct Type_s *target_type;
        } pointer;

        struct {
            int num_arguments;
            struct Type_s **argument_types;
            struct Type_s *return_type;
        } fun;
    };
} Type;

extern Type builtin_void;
extern Type builtin_bool;

extern Type builtin_u64;
extern Type builtin_i64;
extern Type builtin_u32;
extern Type builtin_i32;
extern Type builtin_u16;
extern Type builtin_i16;
extern Type builtin_u8;
extern Type builtin_i8;

extern Type builtin_u64_literal;
extern Type builtin_i64_literal;
extern Type builtin_u32_literal;
extern Type builtin_i32_literal;
extern Type builtin_u16_literal;
extern Type builtin_i16_literal;
extern Type builtin_u8_literal;
extern Type builtin_i8_literal;

Type *type_alloc(TypeKind kind);

void append_fn_arg_type(Type *fn_type, Type *arg_type);

const char *get_type_name_r(char print_buf[1024], Type *type);

bool is_integer_kind(Type *t);
bool is_boolean_kind(Type *t);

bool types_are_equivalent(Type *t1, Type *t2);

bool is_castable_to_integer(Type *t, const char **out_warn);
bool is_castable_to(Type *to, Type *from, const char **out_warn);

struct AST_node_s;
struct AST_node_s *make_cast(Type *to, Type *from);
