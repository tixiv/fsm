
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
        
        case AST_dereference:
            new_node->deref.body = child;
            break;

        case AST_reference:
            new_node->reference.body = child;
            break;
        
        case AST_array_to_slice:
            new_node->_array_to_slice.body = child;
            break;
        
        default:
            NOT_IMPLEMENTED("Inserting AST node of kind %s is not implemented yet.\n", ast_kind_name(new_node->kind));

    }

    new_node->line_number = child->line_number;
    new_node->next = child->next;
    child->next = 0;
    *at = new_node;
}

void ast_remove_node(AST_node *n) {
    AST_node *child = nullptr;
    switch (n->kind) {        
        case AST_dereference:
            child = n->deref.body;
            break;
        default:
            NOT_IMPLEMENTED("Removing AST node of kind %s is not implemented yet.\n", ast_kind_name(n->kind));
    }

    ASSERT(child->next == nullptr, "Can't remove node when child is linked in a chain.\n")
    AST_node *saved_next = n->next;

    // just overwrite this node with the contents of the child. This will effectively move
    // The child to where the old node was.
    memcpy(n, child, sizeof(AST_node));

    // restore the next pointer in case it is used
    n->next = saved_next;
}

void ast_visit_chain(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg) {
    while (n) {
        visit(n, arg);
        n = n->next;
    }
}

void visit_non_null(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg) {
    if (n) visit(n, arg);
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
        case AST_arg_list:
            ast_visit_chain(n->arg_list.body, visit, arg);
            break;
        case AST_function:
            visit_non_null(n->fun.args, visit, arg);
            visit_non_null(n->fun.ret_typedecl, visit, arg);
            ast_visit_chain(n->fun.body, visit, arg);            
            break;
        case AST_return:
            visit_non_null(n->ret.return_val, visit, arg);
            break;
        case AST_var_decl:
            visit_non_null(n->var_decl._typedecl, visit, arg);
            visit_non_null(n->var_decl.initializer, visit, arg);
            break;
        case AST_arg_decl:
            visit_non_null(n->arg_decl._typedecl, visit, arg);
            break;
        case AST_if:
            visit_non_null(n->_if.condition, visit, arg);
            visit_non_null(n->_if.if_clause, visit, arg);
            visit_non_null(n->_if.else_clause, visit, arg);
            break;
        case AST_while:
            visit_non_null(n->_while.condition, visit, arg);
            visit_non_null(n->_while.body, visit, arg);
            break;
        case AST_for:
            visit_non_null(n->_for.initializer, visit, arg);
            visit_non_null(n->_for.condition, visit, arg);
            visit_non_null(n->_for.post_action, visit, arg);
            visit_non_null(n->_for.body, visit, arg);
            break;
        case AST_binary:
            visit_non_null(n->binary.left, visit, arg);
            visit_non_null(n->binary.right, visit, arg);
            break;
        case AST_call:
            ast_visit_chain(n->call.args, visit, arg);
            break;
        case AST_cast:
            visit_non_null(n->_cast.body, visit, arg);
            break;
        case AST_array_to_slice:
            visit_non_null(n->_array_to_slice.body, visit, arg);
            break;
        case AST_array_access:
            visit_non_null(n->_array.array, visit, arg);
            visit_non_null(n->_array.index, visit, arg);
            break;
        case AST_dereference:
            visit_non_null(n->deref.body, visit, arg);
            break;
        case AST_reference:
            visit_non_null(n->reference.body, visit, arg);
            break;
        case AST_struct:
            ast_visit_chain(n->_struct.body, visit, arg);
            break;
        case AST_member_def:
            visit_non_null(n->member_def._typedef, visit, arg);
            break;
        case AST_member_access:
            visit_non_null(n->member_access.body, visit, arg);
            break;
        case AST_typename:
            break;
        case AST_type_ref:
            visit_non_null(n->_type_ref.body, visit, arg);
            break;
        case AST_type_array:
            visit_non_null(n->_type_array.body, visit, arg);
            break;
        case AST_type_slice:
            visit_non_null(n->_type_slice.body, visit, arg);
            break;

        case AST_symbol:
        case AST_number:
        case AST_string:
        case AST_array_len:
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

    char buf[1024];

    switch (n->kind) {
        case AST_program:
        case AST_scope:
        case AST_arg_list:
        case AST_return:
        case AST_while:
        case AST_for:
            printf("%.*s%s \n", (int)spaces, spc, kind_name);
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
        case AST_dereference:
        case AST_reference:
        case AST_array_access:
        case AST_array_len:
        case AST_array_to_slice:
            printf("%.*s%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_struct:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_struct.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_member_def:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->member_def.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_member_access:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->member_access.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_typename:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_typename.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_function:
            printf("%.*s%s '%.*s', (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->fun.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_var_decl:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->var_decl.name));
            print_symbol(n->var_decl.symbol);
            printf("\n");
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_arg_decl:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->arg_decl.name));
            print_symbol(n->arg_decl.symbol);
            printf("\n");
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_number:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->number.value), get_type_name_r(buf, n->type));
            break;
        case AST_string:
            printf("%.*s%s \"%.*s\" (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->str.value), get_type_name_r(buf, n->type));
            break;
        case AST_if:
            printf("%.*s%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf, n->type));
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
            printf("%.*s%s %s (%s)\n", (int)spaces, spc, kind_name, token_kind_name(n->binary.token_kind), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_symbol:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->symbol.name));
            print_symbol(n->symbol.symbol);
            printf(" (%s)\n", get_type_name_r(buf, n->type));
            break;
        case AST_call:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->call.name));
            print_symbol(n->call.symbol);
            printf(" (%s)\n", get_type_name_r(buf, n->type));
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
