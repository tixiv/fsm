
#include "calculate_stacks.h"
#include "dyn_array.h"
#include "resolver.h"
#include "common.h"
#include <stdio.h>

typedef struct {
    Dyn_array var_symbols;
    Dyn_array arg_symbols;
    Symbol *current_function;
} StackVisitor;

static void calculate_function_stack(StackVisitor *res, Symbol *s_fun) {
    {
        size_t offset = 0;
        for (int i = res->arg_symbols.count-1; i >= 0; i--) {
            Symbol *s = get_symbol(&res->arg_symbols, i);
            s->offset = offset;
            offset += get_stack_offset_for(s->type);
        }
    }
    {
        size_t offset = 0;
        for (int i = 0; i < res->var_symbols.count; i++) {
            Symbol *s = get_symbol(&res->var_symbols, i);
            if (s->kind == SYM_local) {
                offset += get_stack_offset_for(s->type);
                s->offset = offset;
            }
        }
        s_fun->size = offset;
    }
}

static void enter_function(StackVisitor *res, Symbol *s_fun) {
    res->current_function = s_fun;
    res->arg_symbols.count = 0;
    res->var_symbols.count = 0;
}

static void leave_function(StackVisitor *res) {
    ASSERT(res->current_function, "Resolver leave function called without active function\n");
    calculate_function_stack(res, res->current_function);
    res->current_function = nullptr;
    res->arg_symbols.count = 0;
    res->var_symbols.count = 0;
}

static void stack_calc_vistitor(AST_node *n, StackVisitor *res) {
    switch (n->kind) {
        case AST_function: {
            enter_function(res, n->fun.symbol);
            ast_visit_children(n, (AstVisitor)stack_calc_vistitor, res);
            leave_function(res);
            break;
        }

        case AST_arg_decl: {
            ASSERT(res->current_function, "Found function argument declaration for '%.*s' outside of function\n", SV_prnt(n->arg_decl.name));
            dyn_array_push_p(&res->arg_symbols, n->arg_decl.symbol);
            ast_visit_children(n, (AstVisitor)stack_calc_vistitor, res);
            break;
        }

        case AST_var_decl: {
            ASSERT(res->current_function, "Found local variable declaration for '%.*s' outside of function\n", SV_prnt(n->var_decl.name));
            dyn_array_push_p(&res->var_symbols, n->var_decl.symbol);
            ast_visit_children(n, (AstVisitor)stack_calc_vistitor, res);
            break;
        }

        default:
            ast_visit_children(n, (AstVisitor)stack_calc_vistitor, res);
            break;
    }
}


void calculate_stacks(AST_node *root) {
    StackVisitor v;
    dyn_array_init(&v.arg_symbols, sizeof(void*), 8);
    dyn_array_init(&v.var_symbols, sizeof(void*), 8);
    stack_calc_vistitor(root, &v);
}