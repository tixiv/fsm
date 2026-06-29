
#include "ast.h"
#include "sv.h"
#include "tokenizer.h"
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

static void ast_dump_tree_node (AST_node *n, int spaces) {
    assert(n);

    const char *spc = "                                                              " // Yeah, these are my spaces
                      "                                                              ";

    for ( ; n; n = n->next) {
        const char *kind_name = ast_kind_name(n->kind);

        switch (n->kind) {
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
                printf("%.*s%s '%.*s'\n", spaces, spc, kind_name, SV_prnt(n->var.name));
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
                    ast_dump_tree_node(n->_if.if_clause, spaces + 4);
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
            case AST_var:
                printf("%.*s%s '%.*s'\n", spaces, spc, kind_name, SV_prnt(n->var.name));
                break;
            case AST_call:
                printf("%.*s%s '%.*s'\n", spaces, spc, kind_name, SV_prnt(n->call.name));
                if (n->call.args) {
                    printf("%.*s  Args:\n", spaces, spc);
                    ast_dump_tree_node(n->call.args, spaces + 4);
                }
                break;
            default:
                fprintf(stderr, "Dumping %s is not implemented yet.\n", kind_name);
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
