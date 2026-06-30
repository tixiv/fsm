
#include "ast.h"
#include "sv.h"
#include "tokenizer.h"
#include "common.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

AST_node *ast_alloc(AST_kind kind, int line_number) {
    AST_node *node = malloc(sizeof(AST_node));
    memset(node, 0, sizeof(AST_node));
    node->kind = kind;
    node->line_number = line_number;

    return node;
}

const char *ast_kind_name(AST_kind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        AST_LIST
#undef X
    }
    return "Undefined AST kind";
}

const char *symbol_kind_name(SymbolKind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        SYM_LIST
#undef X
    }
    return "Undefined Symbol kind";
}

void ast_visit_chain(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg) {
    while (n) {
        visit(n, arg);
        n = n->next;
    }
}

void ast_visit_children(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg)
{
    switch (n->kind) {
        case AST_program:
            ast_visit_chain(n->program.body, visit, arg);
            break;
        case AST_scope:
            ast_visit_chain(n->scope.body, visit, arg);
            break;
        case AST_function:
            ast_visit_chain(n->fun.args, visit, arg);
            ast_visit_chain(n->fun.body, visit, arg);            
            break;
        case AST_return:
            if (n->ret.return_val) visit(n->ret.return_val, arg);
            break;
        case AST_var_decl:
            if (n->var_decl.initializer) visit(n->var_decl.initializer, arg);
            break;
        case AST_arg_decl:
        case AST_number:
            break;

        case AST_if:
            if (n->_if.condition) visit(n->_if.condition, arg);
            ast_visit_chain(n->_if.if_clause, visit, arg);
            ast_visit_chain(n->_if.else_clause, visit, arg);
            break;
        case AST_while:
            if (n->_while.condition) visit(n->_while.condition, arg);
            ast_visit_chain(n->_while.body, visit, arg);
            break;
        case AST_binary:
            if (n->binary.left) visit(n->binary.left, arg);
            if (n->binary.left) visit(n->binary.right, arg);
            break;
        case AST_call:
            ast_visit_chain(n->call.args, visit, arg);
            break;
        default:
            NOT_IMPLEMENTED("Visiting %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}


static void print_symbol(Symbol *s) {
    if(!s)
        printf("(null symbol)");
    else
        printf("(%s '%.*s')", symbol_kind_name(s->kind), SV_prnt(s->name));
}

static void ast_dump_tree_node (AST_node *n, int spaces) {
    assert(n);

    const char *spc = "                                                              " // Yeah, these are my spaces
                      "                                                              ";

    for ( ; n; n = n->next) {
        const char *kind_name = ast_kind_name(n->kind);

        switch (n->kind) {
            case AST_program:
                printf("%.*s%s\n", spaces, spc, kind_name);
                ast_dump_tree_node(n->program.body, spaces + 2);
                break;
            case AST_scope:
                printf("%.*s%s\n", spaces, spc, kind_name);
                ast_dump_tree_node(n->scope.body, spaces + 2);
                break;
            case AST_function:
                printf("%.*s%s '%.*s'\n", spaces, spc, kind_name, SV_prnt(n->fun.name));
                if (n->fun.args) {
                    printf("%.*s  Args:\n", spaces, spc);
                    ast_dump_tree_node(n->fun.args, spaces + 4);
                }
                if (n->fun.body) {
                    printf("%.*s  Body:\n", spaces, spc);
                    ast_dump_tree_node(n->fun.body, spaces + 4);
                }
                
                break;
            case AST_return:
                printf("%.*s%s\n", spaces, spc, kind_name);
                if (n->ret.return_val) {
                    ast_dump_tree_node(n->ret.return_val, spaces + 2);
                }
                break;
            case AST_var_decl:
            case AST_arg_decl:
                printf("%.*s%s '%.*s' ", spaces, spc, kind_name, SV_prnt(n->var_decl.name));
                print_symbol(n->var_decl.symbol);
                printf("\n");
                if (n->var_decl.initializer)
                    ast_dump_tree_node(n->var_decl.initializer, spaces + 2);
                break;
            case AST_number:
                printf("%.*s%s '%.*s'\n", spaces, spc, kind_name, SV_prnt(n->number.value));
                break;
            case AST_if:
                printf("%.*s%s\n", spaces, spc, kind_name);
                if (n->_if.condition) {
                    printf("%.*s  Condition:\n", spaces, spc);
                    ast_dump_tree_node(n->_if.condition, spaces + 4);
                }
                if (n->_if.if_clause) {
                    printf("%.*s  If clause:\n", spaces, spc);
                    ast_dump_tree_node(n->_if.if_clause, spaces + 4);
                }
                if (n->_if.else_clause) {
                    printf("%.*s  Else clause:\n", spaces, spc);
                    ast_dump_tree_node(n->_if.else_clause, spaces + 4);
                }
                break;
            case AST_while:
                printf("%.*s%s\n", spaces, spc, kind_name);
                if (n->_while.condition) {
                    printf("%.*s  Condition:\n", spaces, spc);
                    ast_dump_tree_node(n->_if.condition, spaces + 4);
                }
                if (n->_while.body) {
                    printf("%.*s  Body:\n", spaces, spc);
                    ast_dump_tree_node(n->_while.body, spaces + 4);
                }
                break;
            case AST_binary:
                printf("%.*s%s %s\n", spaces, spc, kind_name, token_kind_name(n->binary.token_kind));
                if (n->binary.left) {
                    printf("%.*s  left:\n", spaces, spc);
                    ast_dump_tree_node(n->binary.left, spaces + 4);
                }
                if (n->binary.right) {
                    printf("%.*s  right:\n", spaces, spc);
                    ast_dump_tree_node(n->binary.right, spaces + 4);
                }
                break;
            case AST_symbol:
                printf("%.*s%s '%.*s' ", spaces, spc, kind_name, SV_prnt(n->symbol.name));
                print_symbol(n->symbol.symbol);
                printf("\n");
                break;
            case AST_call:
                printf("%.*s%s '%.*s' ", spaces, spc, kind_name, SV_prnt(n->call.name));
                print_symbol(n->call.symbol);
                printf("\n");
                
                if (n->call.args) {
                    printf("%.*s  Args:\n", spaces, spc);
                    ast_dump_tree_node(n->call.args, spaces + 4);
                }
                break;
            default:
                NOT_IMPLEMENTED("Dumping %s is not implemented yet.\n", ast_kind_name(n->kind));
                break;
        }
    }
}

void ast_dump_tree (AST_node *root) {
    if (!root) {
        printf("ast_dump_tree: 'null' root node\n");
        return;
    }

    ast_dump_tree_node(root, 0);
}
