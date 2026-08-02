
#include "symbol.h"
#include "common.h"
#include <stdlib.h>
#include "dyn_array.h"
#include "string.h"
#include "type_resolver.h"

Dyn_array global_symbols;

Dyn_array builtin_type_symbols;

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

Symbol *make_type_symbol(SV *name) {
    Type *t = get_type_by_name(name);
    if (!t) return nullptr;

    Symbol *s = alloc_symbol(SYM_type, *name);
    s->type = t;
    return s;
}

Symbol *get_symbol_by_name(Dyn_array *arr, SV *name) {
    for (int i = 0; i < arr->count; i++) {
        Symbol *s = get_symbol(arr, i);
        if (sv_equal(&s->name, name)) return s;
    }

    return nullptr;
}


