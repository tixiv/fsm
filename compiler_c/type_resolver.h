
#pragma once

#include "ast.h"

void type_resolver_init();
void run_type_resolver(AST_node *root);
Type *get_type_by_name(const SV *name);
