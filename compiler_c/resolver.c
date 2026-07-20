
#include "ast.h"
#include "common.h"
#include "dyn_array.h"
#include "sv.h"
#include "type.h"
#include "symbol.h"
#include <stdlib.h>
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

typedef struct {
    Dyn_array local_symbols;
    Symbol *current_function;
    size_t locals_stack[100];
    size_t locals_stack_pointer;
} Resolver;

static Symbol *resolver_lookup_symbol(Resolver *res, SV *name, int line_number, bool do_undefined_error);

static void push_symbol(Resolver *res, Dyn_array *arr, Symbol *s, int line_number) {
    if (s->name.len) {
        Symbol *conflicting = resolver_lookup_symbol(res,  &s->name, line_number, false);
        if (conflicting) {
            resolver_error(line_number, "Symbol '%.*s' redefined\n", SV_prnt(s->name));
            return;
        }
    }

    dyn_array_push_p(arr, s);
}

Dyn_array builtin_functions; // Dyn_array<Symbol*>

void declare_builtin_fn(SV name, Type *return_type, size_t num_args, Type *arg_types[]) {
    Symbol *s = alloc_symbol(SYM_global, name);
    s->type = get_function_type(return_type, arg_types, num_args);
    dyn_array_push_p(&builtin_functions, s);
}

void init_builtin_functions() {
    dyn_array_init(&builtin_functions, sizeof(Symbol*), 8);

    declare_builtin_fn(mkSV("print"), &builtin_void, 1, (Type*[]){&builtin_i64});
    declare_builtin_fn(mkSV("putc"), &builtin_void, 1, (Type*[]){&builtin_u8});
    declare_builtin_fn(mkSV("puts"), &builtin_void, 1, (Type*[]){&builtin_u8_slice});
    declare_builtin_fn(mkSV("open"), &builtin_i64, 2, (Type*[]){&builtin_u8_reference, &builtin_i64 });// open(filename: u8 &, flags: i64) : i64
    declare_builtin_fn(mkSV("mmap"), &builtin_u8_reference, 2, (Type*[]){&builtin_i64, &builtin_i64});// mmap(lenght: i64, fd: i64) : u8 &
    declare_builtin_fn(mkSV("fsize"), &builtin_i64, 1, (Type*[]){&builtin_i64});// fsize(fd: i64) : i64

    // TODO: These should act like operators actually, and not like builtin functions, because we can then better handle different types of arguments
    declare_builtin_fn(mkSV("bittest"), &builtin_bool, 2, (Type*[]){get_ref_type_for(&builtin_any), &builtin_i64});
    declare_builtin_fn(mkSV("setbit"), &builtin_void, 3, (Type*[]){get_ref_type_for(&builtin_any), &builtin_i64, &builtin_bool});
    declare_builtin_fn(mkSV("bitshift"), &builtin_u64, 2, (Type*[]){get_ref_type_for(&builtin_any), &builtin_i64});
    declare_builtin_fn(mkSV("bitand"), &builtin_u64, 2, (Type*[]){&builtin_u64, &builtin_u64});
    declare_builtin_fn(mkSV("bitor"), &builtin_u64, 2, (Type*[]){&builtin_u64, &builtin_u64});
    declare_builtin_fn(mkSV("bitxor"), &builtin_u64, 2, (Type*[]){&builtin_u64, &builtin_u64});
    declare_builtin_fn(mkSV("bitnot"), &builtin_u64, 1, (Type*[]){&builtin_u64});
}

static Symbol *resolver_lookup_symbol(Resolver *res, SV *name, int line_number, bool do_undefined_error) {
    // locals
    Symbol *s = get_symbol_by_name(&res->local_symbols, name);
    if (s) return s;

    // globals
    s = get_symbol_by_name(&global_symbols, name);
    if (s) return s;

    // builtin
    s = get_symbol_by_name(&builtin_functions, name);
    if (s) return s;

    if(do_undefined_error)
        resolver_error(line_number, "Undefined symbol %.*s", SV_prnt(*name));
    
    return nullptr;
}

static void resolver_enter_function(Resolver *res, Symbol *s_fun) {
    res->current_function = s_fun;
    res->local_symbols.count = 0;
    res->locals_stack_pointer = 0;
}

static void resolver_leave_function(Resolver *res) {
    ASSERT(res->current_function, "Resolver leave function called without active function\n");
    res->current_function = nullptr;
    res->local_symbols.count = 0;
}

static void resolver_enter_scope(Resolver *res, int line_number) {
    if (res->locals_stack_pointer >= 99) resolver_error(line_number, "Scope stack overflow. More than 100 levels nested. What are you doing?.\n");
    res->locals_stack[res->locals_stack_pointer++] = res->local_symbols.count;
}

static void resolver_leave_scope(Resolver *res) {
    ASSERT(res->locals_stack_pointer >= 1, "Resolver locals stack underflow.\n")
    res->local_symbols.count = res->locals_stack[--res->locals_stack_pointer];
}

static void resolver_visitor(AST_node *n, Resolver *res) {
    switch (n->kind) {

        case AST_function: {
            Symbol *s_fun = alloc_symbol(SYM_global, n->fun.name);
            push_symbol(res, &global_symbols, s_fun, n->line_number);
            n->fun.symbol = s_fun;
            resolver_enter_function(res, s_fun);
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            resolver_leave_function(res);
            break;
        }

        case AST_arg_decl: {
            ASSERT(res->current_function, "Found function argument declaration for '%.*s' outside of function\n", SV_prnt(n->arg_decl.name));
            Symbol *s_arg = alloc_symbol(SYM_arg, n->arg_decl.name);
            push_symbol(res, &res->local_symbols, s_arg, n->line_number);
            n->arg_decl.symbol = s_arg;
            break;
        }

        case AST_var_decl: {
            ASSERT(res->current_function, "Found local variable declaration for '%.*s' outside of function\n", SV_prnt(n->var_decl.name));
            Symbol *s_var = alloc_symbol(SYM_local, n->var_decl.name);
            push_symbol(res, &res->local_symbols, s_var, n->line_number);
            n->var_decl.symbol = s_var;
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;
        }

        case AST_symbol:
            n->symbol.symbol = resolver_lookup_symbol(res, &n->symbol.name, n->line_number, true);
            break;

        case AST_call:
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;

        case AST_return:
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;

        case AST_scope:
            resolver_enter_scope(res, n->line_number);
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            resolver_leave_scope(res);
            break;

        case AST_for:
            resolver_enter_scope(res, n->line_number);
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            resolver_leave_scope(res);
            break;

        case AST_arg_list:
        case AST_program:
        case AST_if:
        case AST_while:
        case AST_number:
        case AST_bool:
        case AST_null:
        case AST_binary:
        case AST_variadic_operator:
        case AST_unary:
        case AST_string:
        case AST_char_constant:
        case AST_array_access:
        case AST_dereference:
        case AST_reference:
        case AST_plus_plus:
        case AST_minus_minus:
        case AST_struct:
        case AST_member_def:
        case AST_member_access:
        case AST_namespace_access:
        case AST_enum:
        case AST_enum_member:
        case AST_typename:
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
        case AST_builder_string:
            ast_visit_children(n, (AstVisitor)resolver_visitor, res);
            break;

        default:
            NOT_IMPLEMENTED("Resolver for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}

void resolver(AST_node *root) {
    Resolver res;
    dyn_array_init(&res.local_symbols, sizeof(Symbol *), 32);
    resolver_visitor(root, &res);
}

void init_resolver () {
    dyn_array_init(&global_symbols, sizeof(Symbol *), 32);
    init_builtin_functions();
}
