
#include "ast.h"
#include "sv.h"
#include "tokenizer.h"
#include "common.h"
#include "type.h"
#include <malloc.h>
#include <stdint.h>
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

AST_node *get_last_in_chain(AST_node *n) {
    while (n->next) n = n->next;
    return n;
}

void ast_insert_node(AST_node **at, AST_node *new_node) {
    AST_node *child = *at;
    switch (new_node->kind) {
        case AST_cast:
            new_node->_cast.body = child;
            break;
        
        default:
            NOT_IMPLEMENTED("Inserting AST node of kind %s is not implemented yet.\n", ast_kind_name(new_node->kind));

    }

    new_node->line_number = child->line_number;
    new_node->next = child->next;
    child->next = 0;
    *at = new_node;
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
        case AST_symbol:
        case AST_arg_decl:
        case AST_number:
            break;

        case AST_if:
            if (n->_if.condition) visit(n->_if.condition, arg);
            if (n->_if.if_clause) visit (n->_if.if_clause, arg);
            if (n->_if.if_clause) visit (n->_if.if_clause, arg);
            break;
        case AST_while:
            if (n->_while.condition) visit(n->_while.condition, arg);
            if (n->_while.body) visit(n->_while.body, arg);
            break;
        case AST_for:
            if (n->_for.initializer) visit(n->_for.initializer, arg);
            if (n->_for.condition) visit(n->_for.condition, arg);
            if (n->_for.post_action) visit(n->_for.post_action, arg);
            if (n->_for.body) visit(n->_for.body, arg);
            break;
        case AST_binary:
            if (n->binary.left) visit(n->binary.left, arg);
            if (n->binary.left) visit(n->binary.right, arg);
            break;
        case AST_call:
            ast_visit_chain(n->call.args, visit, arg);
            break;
        case AST_cast:
            if (n->_cast.body) visit(n->_cast.body, arg);
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

static void ast_dump_visitor (AST_node *n, uint64_t spaces) {
    const char *spc = "                                                              " // Yeah, these are my spaces
                      "                                                              ";

    const char *kind_name = ast_kind_name(n->kind);

    switch (n->kind) {
        case AST_program:
        case AST_scope:
        case AST_return:
        case AST_while:
        case AST_for:
            printf("%.*s%s\n", (int)spaces, spc, kind_name);
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_function:
            printf("%.*s%s '%.*s'\n", (int)spaces, spc, kind_name, SV_prnt(n->fun.name));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_var_decl:
        case AST_arg_decl:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->var_decl.name));
            print_symbol(n->var_decl.symbol);
            printf("\n");
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_number:
            printf("%.*s%s '%.*s'\n", (int)spaces, spc, kind_name, SV_prnt(n->number.value));
            break;
        case AST_if:
            printf("%.*s%s\n", (int)spaces, spc, kind_name);
            if (n->_if.condition) {
                printf("%.*s  Condition:\n", (int)spaces, spc);
                ast_dump_visitor(n->_if.condition, spaces + 4);
            }
            if (n->_if.if_clause) {
                printf("%.*s  If clause:\n", (int)spaces, spc);
                ast_dump_visitor(n->_if.if_clause, spaces + 4);
            }
            if (n->_if.else_clause) {
                printf("%.*s  Else clause:\n", (int)spaces, spc);
                ast_dump_visitor(n->_if.else_clause, spaces + 4);
            }
            break;
        case AST_binary:
            printf("%.*s%s %s\n", (int)spaces, spc, kind_name, token_kind_name(n->binary.token_kind));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_symbol:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->symbol.name));
            print_symbol(n->symbol.symbol);
            printf("\n");
            break;
        case AST_call:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->call.name));
            print_symbol(n->call.symbol);
            printf("\n");
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_cast: {
                char buf_1[1024], buf_2[1024];
                printf("%.*s%s '%s' <- '%s'\n", (int)spaces, spc, kind_name,
                    get_type_name_r(buf_1, n->type),
                    get_type_name_r(buf_2, n->_cast.right_type));
                ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
                break;
            }
        default:
            NOT_IMPLEMENTED("Dumping %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}

void ast_dump_tree (AST_node *root) {
    if (!root) {
        printf("ast_dump_tree: 'null' root node\n");
        return;
    }

    ast_dump_visitor(root, 0);
}
