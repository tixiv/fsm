
#pragma once

#include "dyn_array.h"
#include "sv.h"
#include <stdbool.h>


extern Dyn_array modules; // AST_node*[]

void compile_program(const char *filename);

bool resolve_import (SV name);

struct AST_node_s;

typedef struct {
    Dyn_array tokens_dyn;
    struct AST_node_s *ast;
    char filename[1024];
} Module;

extern Module *current_module;

extern const char *debug_ast;
extern bool debug_tokens;