
#include "opcodes.h"
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
        Opcode *t = &opcodes[i];
        if (t->string_value.begin) {
            printf("[%s, \"" SV_FMT "\"]\n", opcode_name(t->kind), SV_prnt(t->string_value));
        } else {
            printf("[%s, %lu]\n", opcode_name(t->kind), t->u64_value);
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