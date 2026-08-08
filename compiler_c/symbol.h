
#pragma once

#include "dyn_array.h"
#include "type.h"
#include "location.h"

#define SYM_LIST \
    X(SYM_function) \
    X(SYM_local) \
    X(SYM_arg) \
    X(SYM_type) \

typedef enum {
#define X(name) name,
    SYM_LIST
#undef X
} SymbolKind;

const char *symbol_kind_name(SymbolKind kind);

struct AST_node_s;

typedef struct {
    SV name;
    SymbolKind kind;
    Type *type;

    size_t size; // stack frame size for functions
    size_t offset; // stack offset for args and local vars

    struct AST_node_s *source; // for generics
    const Location *location;
} Symbol;

Symbol *alloc_symbol(SymbolKind kind, SV name, const Location *location);

Symbol *get_symbol_by_name(Dyn_array *arr, SV *name);

extern Dyn_array global_symbols;

Symbol *make_type_symbol(SV *name, const Location *location);

extern Dyn_array builtin_functions;
