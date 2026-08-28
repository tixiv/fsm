
#include "ast.h"
#include "sv.h"
#include "tokenizer.h"
#include "common.h"
#include "type.h"
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

AST_node *ast_alloc(AST_kind kind, const Location *location) {
    AST_node *node = malloc(sizeof(AST_node));
    memset(node, 0, sizeof(AST_node));
    node->kind = kind;
    node->location = location;

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

size_t ast_count_chain(AST_node *n) {
    size_t num = 0;
    while (n) {
        num++;
        n = n->next;
    }
    return num;
}

void ast_link_to_chain(AST_node **chain_p, AST_node *n) {
    if (!*chain_p) {
        *chain_p = n;
    }
    else {
        get_last_in_chain(*chain_p)->next = n;
    }
}

void ast_insert_node(AST_node *at, AST_node *new_node) {
    // We overwrite the old node with the new one.
    // make a copy first.
    AST_node *at_copy = ast_alloc(at->kind, at->location);
    memcpy (at_copy, at, sizeof(AST_node));
    at_copy->next = nullptr; // zero chain pointer in case it was used

    new_node->location = at->location;
    new_node->next = at->next;
    new_node->result_used = at->result_used;

    switch (new_node->kind) {
        case AST_cast:           new_node->_cast.body = at_copy; break;
        case AST_dereference:    new_node->deref.body = at_copy; break;
        case AST_reference:      new_node->reference.body = at_copy; break;
        case AST_array_to_slice: new_node->_array_to_slice.body = at_copy; break;
        case AST_call:           new_node->call.args = at_copy; break;

        default:
            NOT_IMPLEMENTED("Inserting AST node of kind %s is not implemented yet.\n", ast_kind_name(new_node->kind));
    }

    // overwrite the old node with the new one that has the old
    // ones copy linked
    memcpy(at, new_node, sizeof(AST_node));
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
            visit_non_null(n->ret.body, visit, arg);
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
            visit_non_null(n->_for.result, visit, arg);
            break;
        case AST_binary:
            visit_non_null(n->binary.left, visit, arg);
            visit_non_null(n->binary.right, visit, arg);
            break;
        case AST_unary:
            visit_non_null(n->unary.body, visit, arg);
            break;
        case AST_call:
            visit_non_null(n->call.target, visit, arg);
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
        case AST_plus_plus:
            visit_non_null(n->plus_plus.body, visit, arg);
            break;
        case AST_minus_minus:
            visit_non_null(n->minus_minus.body, visit, arg);
            break;
        case AST_struct:
            ast_visit_chain(n->_struct.body, visit, arg);
            break;
        case AST_enum:
            ast_visit_chain(n->_enum.body, visit, arg);
            break;
        case AST_union:
            ast_visit_chain(n->_union.body, visit, arg);
            break;
        case AST_member_def:
            visit_non_null(n->struct_member_def._typedef, visit, arg);
            break;
        case AST_union_member_def:
            visit_non_null(n->union_member_def._typedef, visit, arg);
            break;
        case AST_member_access:
            visit_non_null(n->member_access.body, visit, arg);
            break;
        case AST_namespace_access:
            visit_non_null(n->namespace_access.body, visit, arg);
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
        case AST_variadic_operator:
            visit_non_null(n->variadic_operator.left, visit, arg);
            for (size_t i = 0; i < n->variadic_operator.num_members; i++) {
                visit_non_null(n->variadic_operator.members[i].right, visit, arg);
            }
            break;
        case AST_builder_string:
            visit_non_null(n->builder_string.var_decl_sb, visit, arg);
            visit_non_null(n->builder_string.var_decl_arr, visit, arg);
            ast_visit_chain(n->builder_string.body, visit, arg);
            break;
        case AST_function_type:
            ast_visit_chain(n->_function_type.function_args, visit, arg);
            visit_non_null(n->_function_type.function_ret, visit, arg);
            break;
        case AST_user_cast:
            visit_non_null(n->user_cast.typedecl, visit, arg);
            visit_non_null(n->user_cast.body, visit, arg);
            break;
        case AST_generic:
            visit_non_null(n->generic.body, visit, arg);
            break;
        case AST_generic_speciation:
            visit_non_null(n->generic_speciation.typedecl, visit, arg);
            visit_non_null(n->generic_speciation.body, visit, arg);
            break;
        case AST_type_generic_speciation:
            visit_non_null(n->type_generic_speciation.typedecl, visit, arg);
            visit_non_null(n->type_generic_speciation.body, visit, arg);
            break;
        case AST_generic_implementation:
            visit_non_null(n->generic_implementation.body, visit, arg);
            break;

        case AST_enum_member:
        case AST_symbol:
        case AST_number:
        case AST_bool:
        case AST_null:
        case AST_string:
        case AST_char_constant:
        case AST_array_len:
            break;

        default:
            NOT_IMPLEMENTED("Visiting %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}

AST_node *ast_copy_tree(AST_node *n);

AST_node *ast_copy_chain(AST_node *n) {
    AST_node *base = nullptr;
    while(n) {
        ast_link_to_chain(&base, ast_copy_tree(n));
        n = n->next;
    }
    
    return base;
}

AST_node *ast_copy_tree(AST_node *n) {
    if (!n) return nullptr;
    AST_node *nn = ast_alloc(n->kind, n->location);
    memcpy(nn, n, sizeof(AST_node));
    nn->next = nullptr;
    switch (n->kind) {
        case AST_generic:
            nn->generic.body = ast_copy_tree(n->generic.body);
            break;
        case AST_arg_list:
            nn->arg_list.body = ast_copy_chain(n->arg_list.body);
            break;
        case AST_scope:
            nn->scope.body = ast_copy_chain(n->scope.body);
            break;
        case AST_function:
            nn->fun.args = ast_copy_tree(n->fun.args);
            nn->fun.ret_typedecl = ast_copy_tree(n->fun.ret_typedecl);
            nn->fun.body = ast_copy_chain(n->fun.body);
            break;
        case AST_return:
            nn->ret.body = ast_copy_tree(n->ret.body);
            break;
        case AST_var_decl:
            nn->var_decl._typedecl = ast_copy_tree(n->var_decl._typedecl);
            nn->var_decl.initializer = ast_copy_tree(n->var_decl.initializer);
            break;
        case AST_arg_decl:
            nn->arg_decl._typedecl = ast_copy_tree(n->arg_decl._typedecl);
            break;
        case AST_if:
            nn->_if.condition = ast_copy_tree(n->_if.condition);
            nn->_if.if_clause = ast_copy_tree(n->_if.if_clause);
            nn->_if.else_clause = ast_copy_tree(n->_if.else_clause);
            break;
        case AST_while:
            nn->_while.condition = ast_copy_tree(n->_while.condition);
            nn->_while.body = ast_copy_tree(n->_while.body);
            break;
        case AST_for:
            nn->_for.initializer = ast_copy_tree(n->_for.initializer);
            nn->_for.condition = ast_copy_tree(n->_for.condition);
            nn->_for.post_action = ast_copy_tree(n->_for.post_action);
            nn->_for.body = ast_copy_tree(n->_for.body);
            nn->_for.result = ast_copy_tree(n->_for.result);
            break;
        case AST_binary:
            nn->binary.left = ast_copy_tree(n->binary.left);
            nn->binary.right = ast_copy_tree(n->binary.right);
            break;
        case AST_unary:
            nn->unary.body = ast_copy_tree(n->unary.body);
            break;
        case AST_call:
            nn->call.target = ast_copy_tree(n->call.target);
            nn->call.args = ast_copy_chain(n->call.args);
            break;
        case AST_cast:
            nn->_cast.body = ast_copy_tree(n->_cast.body);
            break;
        case AST_array_to_slice:
            nn->_array_to_slice.body = ast_copy_tree(n->_array_to_slice.body);
            break;
        case AST_array_access:
            nn->_array.array = ast_copy_tree(n->_array.array);
            nn->_array.index = ast_copy_tree(n->_array.index);
            break;
        case AST_dereference:
            nn->deref.body = ast_copy_tree(n->deref.body);
            break;
        case AST_reference:
            nn->reference.body = ast_copy_tree(n->reference.body);
            break;
        case AST_plus_plus:
            nn->plus_plus.body = ast_copy_tree(n->plus_plus.body);
            break;
        case AST_minus_minus:
            nn->minus_minus.body = ast_copy_tree(n->minus_minus.body);
            break;
        case AST_member_access:
            nn->member_access.body = ast_copy_tree(n->member_access.body);
            break;
        case AST_namespace_access:
            nn->namespace_access.body = ast_copy_tree(n->namespace_access.body);
            break;
        case AST_typename:
            break;
        case AST_type_ref:
            nn->_type_ref.body = ast_copy_tree(n->_type_ref.body);
            break;
        case AST_type_array:
            nn->_type_array.body = ast_copy_tree(n->_type_array.body);
            break;
        case AST_type_slice:
            nn->_type_slice.body = ast_copy_tree(n->_type_slice.body);
            break;
        case AST_variadic_operator:
            nn->variadic_operator.left = ast_copy_tree(n->variadic_operator.left);
            nn->variadic_operator.members = malloc(sizeof(VariadicOperatorMember) * n->variadic_operator.num_members);
            for (size_t i = 0; i < n->variadic_operator.num_members; i++) {
                memcpy(&nn->variadic_operator.members[i], &n->variadic_operator.members[i], sizeof(VariadicOperatorMember));
                nn->variadic_operator.members[i].right = ast_copy_tree(n->variadic_operator.members[i].right);
            }
            break;
        case AST_builder_string:
            nn->builder_string.var_decl_sb = ast_copy_tree(n->builder_string.var_decl_sb);
            nn->builder_string.var_decl_arr = ast_copy_tree(n->builder_string.var_decl_arr);
            nn->builder_string.body = ast_copy_chain(n->builder_string.body);
            break;
        case AST_function_type:
            nn->_function_type.function_args = ast_copy_chain(n->_function_type.function_args);
            nn->_function_type.function_ret = ast_copy_tree(n->_function_type.function_ret);
            break;
        case AST_user_cast:
            nn->user_cast.typedecl = ast_copy_tree(n->user_cast.typedecl);
            nn->user_cast.body = ast_copy_tree(n->user_cast.body);
            break;
        case AST_generic_speciation:
            nn->generic_speciation.typedecl = ast_copy_tree(n->generic_speciation.typedecl);
            nn->generic_speciation.body = ast_copy_tree(n->generic_speciation.body);
            break;
        case AST_type_generic_speciation:
            nn->type_generic_speciation.typedecl = ast_copy_tree(n->type_generic_speciation.typedecl);
            nn->type_generic_speciation.body = ast_copy_tree(n->type_generic_speciation.body);
            break;
        case AST_enum_member:
        case AST_symbol:
        case AST_number:
        case AST_bool:
        case AST_null:
        case AST_string:
        case AST_char_constant:
        case AST_array_len:
            break;

        default:
            NOT_IMPLEMENTED("Copying %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }

    return nn;
}

static void print_symbol(Symbol *s) {
    if(!s)
        printf("(null symbol)");
    else
        printf("(%s '%.*s')", symbol_kind_name(s->kind), SV_prnt(s->name));
}

static void ast_dump_visitor (AST_node *n, void *spaces_vp) {
    uint64_t spaces = (uint64_t) spaces_vp;
    const char *spc = "                                                              " // Yeah, these are my spaces
                      "                                                              ";

    const char *kind_name = ast_kind_name(n->kind);

    char buf[1024];
    char buf2[1024];

    if (n->location)
        printf("%s:%03d:", n->location->filename, n->location->line);

    switch (n->kind) {
        case AST_program:
        case AST_scope:
        case AST_arg_list:
        case AST_return:
        case AST_while:
        case AST_for:
        case AST_generic_implementation:
            printf("%.*s%s \n", (int)spaces, spc, kind_name);
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_builder_string:
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
        case AST_dereference:
        case AST_reference:
        case AST_plus_plus:
        case AST_minus_minus:
        case AST_array_access:
        case AST_array_len:
        case AST_array_to_slice:
        case AST_function_type:
        case AST_null:
            printf("%.*s%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_struct:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_struct.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_member_def:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->struct_member_def.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_member_access:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->member_access.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_enum:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_enum.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_enum_member:
            printf("%.*s%s '%.*s' %ld (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_enum_member.name), n->_enum_member.value, get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_union:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->_union.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_union_member_def:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->union_member_def.name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_namespace_access:
            printf("%.*s%s '%.*s' (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->namespace_access.name), get_type_name_r(buf, n->type));
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
            if (n->number.is_double) {
                printf("%.*s%s %f (%s)\n", (int)spaces, spc, kind_name, n->number.dvalue, get_type_name_r(buf, n->type));
            }
            else {
                printf("%.*s%s %lu (%s)\n", (int)spaces, spc, kind_name, n->number.value, get_type_name_r(buf, n->type));
            }
            break;
        case AST_bool:
            printf("%.*s%s '%d' (%s)\n", (int)spaces, spc, kind_name, n->boolean.value, get_type_name_r(buf, n->type));
            break;
        case AST_string:
        case AST_char_constant:
            printf("%.*s%s \"%.*s\" (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->str.value), get_type_name_r(buf, n->type));
            break;
        case AST_if:
            printf("%.*s%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_binary:
            printf("%.*s%s %s (%s)\n", (int)spaces, spc, kind_name, token_kind_name(n->binary.token_kind), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_unary:
            printf("%.*s%s %s (%s)\n", (int)spaces, spc, kind_name, token_kind_name(n->unary.token_kind), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_variadic_operator:
            printf("%.*s%s %s (%s)\n", (int)spaces, spc, kind_name, token_kind_name(n->variadic_operator.members[0].token_kind), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_symbol:
            printf("%.*s%s '%.*s' ", (int)spaces, spc, kind_name, SV_prnt(n->symbol.name));
            print_symbol(n->symbol.symbol);
            printf(" (%s)\n", get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_call:
            printf("%.*s%s ", (int)spaces, spc, kind_name);
            printf("(%s)\n", get_type_name_r(buf, n->type));
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
        case AST_user_cast: {
            char buf_1[1024], buf_2[1024];
            printf("%.*s%s '%s' <- '%s'\n", (int)spaces, spc, kind_name,
                get_type_name_r(buf_1, n->type),
                get_type_name_r(buf_2, n->user_cast.body->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        }
        case AST_generic:
            printf("%.*s%s \"%.*s\" (%s)\n", (int)spaces, spc, kind_name, SV_prnt(n->generic.parameter_name), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_generic_speciation:
            printf("%.*s%s :%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf2, n->generic_speciation.typedecl->type), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
        case AST_type_generic_speciation:
            printf("%.*s%s :%s (%s)\n", (int)spaces, spc, kind_name, get_type_name_r(buf2, n->type_generic_speciation.typedecl->type), get_type_name_r(buf, n->type));
            ast_visit_children(n, (AstVisitor)ast_dump_visitor, (void*)(spaces + 4));
            break;
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
