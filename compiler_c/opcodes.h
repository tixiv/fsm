
#include "sv.h"
#include "dyn_array.h"
#include <stdint.h>

#define OPCODE_LIST \
    X(OP_begin_fn) \
    X(OP_return) \
    X(OP_add) \
    X(OP_sub) \
    X(OP_mul) \
    X(OP_div) \
    X(OP_mod) \
    X(OP_equal) \
    X(OP_unequal) \
    X(OP_compare_GT) \
    X(OP_compare_LT) \
    X(OP_compare_GE) \
    X(OP_compare_LE) \
    X(OP_call) \
    X(OP_push_result) \
    X(OP_push_arg) \
    X(OP_push_literal) \
    X(OP_push_string_literal) \
    X(OP_push_local_var) \
    X(OP_assign_local_var) \
    X(OP_if) \
    X(OP_else) \
    X(OP_end_if) \
    X(OP_while_loop) \
    X(OP_while_check) \
    X(OP_while_end) \
    X(OP_to_bool) \
    X(OP_load) \
    X(OP_array_access) \

enum Op_Kind {
#define X(name) name,
    OPCODE_LIST
#undef X
};

const char *opcode_name(enum Op_Kind kind);

typedef struct {
    int kind;
    SV string_value;
    union {
        uint64_t u64_value;
        uint32_t u32_value[2];
    };
} Opcode;

extern Dyn_array opcodes_dyn;

#define num_opcodes (opcodes_dyn.count)
#define opcodes ((Opcode *)opcodes_dyn.data)

void dump_opcodes();

size_t push_opcode(int kind, SV *value, uint64_t u64_value);