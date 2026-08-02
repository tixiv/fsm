
#pragma once

#include "dyn_array.h"
#include "type.h"

#define SYM_LIST \
    X(SYM_global) \
    X(SYM_local) \
    X(SYM_arg) \
    X(SYM_type) \


typedef enum {
#define X(name) name,
    SYM_LIST
#undef X
} SymbolKind;

const char *symbol_kind_name(SymbolKind kind);

typedef struct {
    SV name;
    SymbolKind kind;
    Type *type;

    size_t size; // stack frame size for functions
    size_t offset; // stack offset for args and local vars
} Symbol;

Symbol *alloc_symbol(SymbolKind kind, SV name);

Symbol *get_symbol_by_name(Dyn_array *arr, SV *name);

extern Dyn_array global_symbols;

Symbol *make_type_symbol(SV *name);

extern Dyn_array builtin_functions;
