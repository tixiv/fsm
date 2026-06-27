
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

Opcode opcodes[1000];
size_t num_opcodes;

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
