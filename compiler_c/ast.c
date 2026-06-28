
#include "ast.h"
#include "sv.h"
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
