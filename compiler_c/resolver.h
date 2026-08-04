
#include "ast.h"

Symbol *get_symbol(Dyn_array *arr, size_t index);
Symbol *resolver_speciate_generic(AST_node *root, Type *t);
void resolver(AST_node *root);
void init_resolver ();
