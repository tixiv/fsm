
#pragma once

#include "ast.h"

void run_type_resolver(AST_node *root);
Type *get_type_by_name(const SV *name);
