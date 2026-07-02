
#include "ast.h"
#include "common.h"
#include "dyn_array.h"
#include "sv.h"
#include "type.h"
#include <string.h>
#include <stdarg.h>

static void resolver_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Resolver] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

Dyn_array global_symbols;

typedef struct {
    Dyn_array local_symbols;
    Symbol *current_function;
} Resolver;

Symbol *alloc_symbol(SymbolKind kind, SV *name) {
    Symbol *s = malloc(sizeof(Symbol));
    memset(s, 0, sizeof(Symbol));
    s->kind = kind;
    s->name = *name;
    return s;
}

static Symbol *get_symbol(Dyn_array *arr, size_t index) {
    ASSERT(index < arr->count, "Tried to access symbol that does not exist at index %lu\n", index);
    return ((Symbol**)arr->data)[index];
}

static Symbol *get_symbol_by_name(Dyn_array *arr, SV *name) {
    for (int i = 0; i < arr->count; i++) {
        Symbol *s = get_symbol(arr, i);
        if (sv_equal(&s->name, name)) return s;
    }
    return nullptr;
}

static void push_symbol(Dyn_array *arr, Symbol *s, int line_number) {
    Symbol *conflicting = get_symbol_by_name(arr, &s->name);
    if (conflicting) {
        resolver_error(line_number, "Symbol '%.*s' redefined\n", SV_prnt(s->name));
        return;
    }

    dyn_array_push_p(arr, s);
}       

static void calculate_function_stack(Resolver *res, Symbol *s_fun) {
    {
        int arg_count = 0;
        for (int i = res->local_symbols.count-1; i >= 0; i--) {
            Symbol *s = get_symbol(&res->local_symbols, i);
            if (s->kind == SYM_arg) {
                s->offset = arg_count;
                arg_count ++;
            }
        }
        s_fun->num_fn_args = arg_count;

        Type **arg_types = malloc(sizeof(Type*) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            arg_types[i] = get_symbol(&res->local_symbols, i)->type;
        }
        s_fun->type = type_alloc(T_function);
        s_fun->type->fun.argument_types = arg_types;
    }
    {
        int var_count = 0;
        for (int i = 0; i < res->local_symbols.count; i++) {
            Symbol *s = get_symbol(&res->local_symbols, i);
            if (s->kind == SYM_local) {
                s->offset = var_count;
                var_count ++;
            }
        }
        s_fun->size = var_count;
    }
}

static void resolver_enter_function(Resolver *res, Symbol *s_fun) {
    res->current_function = s_fun;
    res->local_symbols.count = 0;
}

static void resolver_leave_function(Resolver *res) {
    ASSERT(res->current_function, "Resolver leave function called without active function\n");
    calculate_function_stack(res, res->current_function);
    res->current_function = nullptr;
    res->local_symbols.count = 0;
}

Symbol builtin_print = {
    .kind = SYM_global,
    .name.begin = "print",
    .name.len = 5,
    .num_fn_args = 1,
};

static Symbol *resolver_lookup_symbol(Resolver *res, SV *name, int line_number) {
    // locals
    Symbol *s = get_symbol_by_name(&res->local_symbols, name);
    if (s) return s;

    // globals
    s = get_symbol_by_name(&global_symbols, name);
    if (s) return s;

    // builtin
    if (sv_equal(name, &builtin_print.name)) {
        return &builtin_print;
    }

    resolver_error(line_number, "Undefined symbol %.*s", SV_prnt(*name));
    return nullptr;
}

static void resolver_visitor(AST_node *n, Resolver *res) {
    switch (n->kind) {

        case AST_function: {
            Symbol *s_fun = alloc_symbol(SYM_global, &n->fun.name);
            push_symbol(&global_symbols, s_fun, n->line_number);
            n->fun.symbol = s_fun;
            resolver_enter_function(res, s_fun);
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            resolver_leave_function(res);
            break;
        }

        case AST_arg_decl: {
            ASSERT(res->current_function, "Found function argument declaration for '%.*s' outside of function\n", SV_prnt(n->var_decl.name));
            Symbol *s_arg = alloc_symbol(SYM_arg, &n->var_decl.name);
            s_arg->type = &builtin_i64;
            dyn_array_push_p(&res->local_symbols, s_arg);
            n->var_decl.symbol = s_arg;
            break;
        }

        case AST_var_decl: {
            ASSERT(res->current_function, "Found local variable declaration for '%.*s' outside of function\n", SV_prnt(n->var_decl.name));
            Symbol *s_var = alloc_symbol(SYM_local, &n->var_decl.name);
            push_symbol(&res->local_symbols, s_var, n->line_number);
            n->var_decl.symbol = s_var;
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;
        }

        case AST_symbol:
            n->symbol.symbol = resolver_lookup_symbol(res, &n->symbol.name, n->line_number);
            break;

        case AST_call:
            n->call.symbol = resolver_lookup_symbol(res, &n->call.name, n->line_number);
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;

        case AST_return:
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            if (n->ret.return_val)
                res->current_function->num_fn_returns = 1;
            break;

        case AST_scope:
        case AST_program:
        case AST_if:
        case AST_while:
        case AST_for:
        case AST_number:
        case AST_binary:
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;

        default:
            NOT_IMPLEMENTED("Resolver for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}

void resolver(AST_node *root) {
    dyn_array_init(&global_symbols, sizeof(Symbol *), 32);

    Resolver res;
    dyn_array_init(&res.local_symbols, sizeof(Symbol *), 32);

    resolver_visitor(root, &res);
}
