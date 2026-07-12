
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

    fprintf(stderr, "[FSM Resolver] %s:%d Error: ", current_filename, line_number);
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

Symbol *get_symbol(Dyn_array *arr, size_t index) {
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

Type *builtin_print_argument_types[] = { &builtin_i64 };

Type builtin_print_type = (Type){T_function, .fun.num_arguments = 1, .fun.argument_types = builtin_print_argument_types, .fun.return_type = &builtin_void};

Symbol builtin_print = {
    .kind = SYM_global,
    .name.begin = "print",
    .name.len = 5,
    .type = &builtin_print_type,
};

Type *builtin_puts_argument_types[] = { &builtin_u8_slice };

Type builtin_puts_type = (Type){T_function, .fun.num_arguments = 1, .fun.argument_types = builtin_puts_argument_types, .fun.return_type = &builtin_void};

Symbol builtin_puts = {
    .kind = SYM_global,
    .name.begin = "puts",
    .name.len = 4,
    .type = &builtin_puts_type,
};

Type *builtin_open_argument_types[] = { &builtin_u8_reference, &builtin_i64 }; // open(filename: u8 &, flags: i64) : i64

Type builtin_open_type = (Type){T_function, .fun.num_arguments = 2, .fun.argument_types = builtin_open_argument_types, .fun.return_type = &builtin_i64};

Symbol builtin_open = {
    .kind = SYM_global,
    .name.begin = "open",
    .name.len = 4,
    .type = &builtin_open_type,
};

Type *builtin_mmap_argument_types[] = { &builtin_i64, &builtin_i64 }; // mmap(lenght: i64, fd: i64) : u8 &

Type builtin_mmap_type = (Type){T_function, .fun.num_arguments = 2, .fun.argument_types = builtin_mmap_argument_types, .fun.return_type = &builtin_u8_reference};

Symbol builtin_mmap = {
    .kind = SYM_global,
    .name.begin = "mmap",
    .name.len = 4,
    .type = &builtin_mmap_type,
};

Type *builtin_fsize_argument_types[] = { &builtin_i64 }; // fsize(fd: i64) : i64

Type builtin_fsize_type = (Type){T_function, .fun.num_arguments = 1, .fun.argument_types = builtin_fsize_argument_types, .fun.return_type = &builtin_i64};

Symbol builtin_fsize = {
    .kind = SYM_global,
    .name.begin = "fsize",
    .name.len = 5,
    .type = &builtin_fsize_type,
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
    if (sv_equal(name, &builtin_puts.name)) {
        return &builtin_puts;
    }
    if (sv_equal(name, &builtin_open.name)) {
        return &builtin_open;
    }
    if (sv_equal(name, &builtin_mmap.name)) {
        return &builtin_mmap;
    }
    if (sv_equal(name, &builtin_fsize.name)) {
        return &builtin_fsize;
    }

    resolver_error(line_number, "Undefined symbol %.*s", SV_prnt(*name));
    return nullptr;
}

static void resolver_enter_function(Resolver *res, Symbol *s_fun) {
    res->current_function = s_fun;
    res->local_symbols.count = 0;
}

static void resolver_leave_function(Resolver *res) {
    ASSERT(res->current_function, "Resolver leave function called without active function\n");
    res->current_function = nullptr;
    res->local_symbols.count = 0;
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
            ASSERT(res->current_function, "Found function argument declaration for '%.*s' outside of function\n", SV_prnt(n->arg_decl.name));
            Symbol *s_arg = alloc_symbol(SYM_arg, &n->arg_decl.name);
            push_symbol(&res->local_symbols, s_arg, n->line_number);
            n->arg_decl.symbol = s_arg;
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
            break;

        case AST_scope:
        case AST_arg_list:
        case AST_program:
        case AST_if:
        case AST_while:
        case AST_for:
        case AST_number:
        case AST_binary:
        case AST_string:
        case AST_array_access:
        case AST_dereference:
        case AST_reference:
        case AST_struct:
        case AST_member_def:
        case AST_member_access:
        case AST_typename:
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
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
