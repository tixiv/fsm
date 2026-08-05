
#pragma once

#include "sv.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    T_void,
    T_unsigned_integer,
    T_signed_integer,
    T_boolean,
    T_function,
    T_reference,
    T_null,
    T_array,
    T_slice,
    T_struct,
    T_record,
    T_union,
    T_enumerator,
    T_enum,
    T_any, // 'any' works like the 'void' in C's void* for now, just with a better name.
    T_generic,
    T_speciated,
} TypeKind;

struct Type_s;

typedef struct {
    SV name;
    struct Type_s *type;
} StructMember;

typedef struct {
    SV name;
    struct Type_s *type;
    int64_t value;
} UnionMember;

typedef struct {
    SV name;
    int64_t value;
} EnumMember;

typedef struct Type_s {
    TypeKind kind;
    SV name;
    size_t storage_size;
    struct Type_s *speciation;
    union {
        struct {
            size_t num_bits;
        } integer;
        
        struct {
            struct Type_s *target_type;
        } reference;

        struct {
            struct Type_s *element_type;
            size_t n_elements;
        } _array;

        struct {
            size_t num_arguments;
            struct Type_s **argument_types;
            struct Type_s *return_type;
        } fun;

        struct {
            size_t num_members;
            StructMember *members;
        } _struct;

        struct {
            size_t num_members;
            EnumMember *members;
        } _enum;

        struct {
            size_t num_members;
            UnionMember *members;
            SV enumarator_name;
        } _union;

        struct {
            struct Type_s *target_type;
        } enumerator;
    };
} Type;

extern Type builtin_void;
extern Type builtin_null;
extern Type builtin_bool;
extern Type builtin_any;

extern Type builtin_u64;
extern Type builtin_i64;
extern Type builtin_u32;
extern Type builtin_i32;
extern Type builtin_u16;
extern Type builtin_i16;
extern Type builtin_u8;
extern Type builtin_i8;

extern Type builtin_u8_reference;
extern Type builtin_u8_slice;

extern Type builtin_generic;


Type *type_alloc(TypeKind kind);

void append_fn_arg_type(Type *fn_type, Type *arg_type);

const char *get_type_name_r(char print_buf[1024], Type *type);
const char *get_type_name_encoded_r(char print_buf[1024], Type *type);

bool is_integer_kind(Type *t);
bool is_signed_integer(Type *t);
bool is_boolean_kind(Type *t);
bool is_array_kind(Type *t);
bool is_struct_kind(Type *t);
bool is_record_kind(Type *t);
bool is_union_kind(Type *t);
bool is_enum_kind(Type *t);
bool is_enumerator_kind(Type *t);
bool is_reference_kind(Type *t);
bool is_function_kind(Type *t);
bool is_function_reference(Type *t);

bool is_slice_type(Type *t);

int get_ref_order(Type *t);

bool types_are_equivalent(Type *t1, Type *t2);
Type *dereferenced_type(Type *t);
size_t get_member_offset(Type *_struct, size_t index);
Type *get_member_type_and_offset_by_name(Type *_struct, SV *member_name, size_t *out_offset);

bool get_enum_member_value (Type *t, SV *member_name, int64_t *enum_value);
bool get_union_member_value (Type *t, SV *member_name, int64_t *enum_value);

size_t get_storage_size(Type *t);
void calculate_storage_size(Type *t);

size_t get_function_arguments_size(Type *t);

Type *get_ref_type_for(Type *t);
Type *get_enumerator_type_for(Type *t);

Type *get_array_type(Type *element_type, size_t n_elements);
Type *get_sclice_type(Type *element_type);
Type *get_function_type(Type *ret_type, Type *arg_types[], size_t num_args);

Type *get_speciated_type(Type *base, Type *speciation);

Type *get_slice_element_type(Type *slice);

bool is_castable_to(Type *to, Type *from, const char **out_warn);

size_t get_stack_offset_for(Type *t);

struct AST_node_s;
struct AST_node_s *make_cast(Type *to, Type *from);

int64_t get_max_enum_value (Type *t);
int64_t get_min_enum_value (Type *t);
SV *get_enum_member_name_by_value(Type *t, int64_t value);
