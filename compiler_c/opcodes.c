
#include "opcodes.h"
#include "type.h"
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
    char buf[1024];
    for (size_t i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];

        if (op->location) {
            printf("%s:%d", op->location->filename, op->location->line);
        }

        if (op->string_value.begin) {
            printf("[%s, \"" SV_FMT "\", %ld, %ld, %s]\n", opcode_name(op->kind), SV_prnt(op->string_value), op->i64_value[0], op->i64_value[1], get_type_name_r(buf, op->type));
        } else {
            printf("[%s, %lu, %lu, %s]\n", opcode_name(op->kind), op->i64_value[0], op->i64_value[1], get_type_name_r(buf, op->type));
        }
    }
}

size_t push_opcode_2(int kind, SV *value, int64_t i64_value_0, int64_t i64_value_1,  struct Type_s *type, const Location *location) {
    Opcode * op = dyn_array_push(&opcodes_dyn);

    op->kind = kind;
    op->i64_value[0] = i64_value_0;
    op->i64_value[1] = i64_value_1;
    if (value) op->string_value = *value;
    op->location = location;
    op->type = type;

    return num_opcodes - 1;
}

size_t push_opcode(int kind, SV *value, int64_t i64_value, struct Type_s *type, const Location *location) {
    return push_opcode_2(kind, value, i64_value, 0, type, location);
}
