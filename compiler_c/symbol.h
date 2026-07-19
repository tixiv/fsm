
#include "dyn_array.h"
#include "ast.h"

Symbol *alloc_symbol(SymbolKind kind, SV name);

Symbol *get_symbol_by_name(Dyn_array *arr, SV *name);

extern Dyn_array global_symbols;
extern Dyn_array builtin_functions;