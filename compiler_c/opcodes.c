
#include "opcodes.h"
#include <stdint.h>
#include <stdio.h>

const char *opcode_name(enum Op_Kind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        OPCODE_LIST
#undef X
    }

    return "Undefined Opcode";
}

Dyn_array opcodes_dyn;

void dump_opcodes() {
    for (int i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];
        if (op->string_value.begin) {
            printf("[%s, \"" SV_FMT "\"]\n", opcode_name(op->kind), SV_prnt(op->string_value));
        } else {
            switch (op->kind) {
                case OP_push_local_var:
                    printf("[%s, sz:%u, var:%u]\n", opcode_name(op->kind), op->u32_value[1], op->u32_value[0]);
                    break;

                default:
                    printf("[%s, %lu]\n", opcode_name(op->kind), op->u64_value);
                    break;
            }
        }
    }
}

size_t push_opcode(int kind, SV *value, uint64_t u64_value) {
    Opcode * op = dyn_array_push(&opcodes_dyn);

    op->kind = kind;
    op->u64_value = u64_value;
    if (value) op->string_value = *value;

    return num_opcodes - 1;
}

size_t push_opcode_sz(int kind, SV *value, uint64_t u64_value, uint64_t size) {
    Opcode * op = dyn_array_push(&opcodes_dyn);

    op->kind = kind;
    op->u64_value = u64_value;
    if (value) op->string_value = *value;
    op->size = size;

    return num_opcodes - 1;
}

size_t push_opcode_sz_sgn(int kind, SV *value, uint64_t u64_value, uint64_t size, bool _signed) {
    Opcode * op = dyn_array_push(&opcodes_dyn);

    op->kind = kind;
    op->u64_value = u64_value;
    if (value) op->string_value = *value;
    op->size = size;
    op->_signed = _signed;

    return num_opcodes - 1;
}