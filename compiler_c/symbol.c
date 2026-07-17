
#include "symbol.h"
#include "common.h"
#include <stdlib.h>
#include "string.h"

Dyn_array global_symbols;


Symbol *alloc_symbol(SymbolKind kind, SV name) {
    Symbol *s = malloc(sizeof(Symbol));
    memset(s, 0, sizeof(Symbol));
    s->kind = kind;
    s->name = name;
    return s;
}

Symbol *get_symbol(Dyn_array *arr, size_t index) {
    ASSERT(index < arr->count, "Tried to access symbol that does not exist at index %lu\n", index);
    return ((Symbol**)arr->data)[index];
}

Symbol *get_symbol_by_name(Dyn_array *arr, SV *name) {
    for (int i = 0; i < arr->count; i++) {
        Symbol *s = get_symbol(arr, i);
        if (sv_equal(&s->name, name)) return s;
    }
    return nullptr;
}


