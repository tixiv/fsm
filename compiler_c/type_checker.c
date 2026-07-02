
#include "type_checker.h"
#include "ast.h"
#include "tokenizer.h"
#include "type.h"
#include "common.h"
#include <stdarg.h>
#include <stdint.h>

static void type_checker_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Type Checker] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

void annotate_used_visitor(AST_node *n, uint64_t used) {
    if (!n) return;
    n->result_used = used;

    switch (n->kind) {
        case AST_scope:
            if (used) type_checker_error(n->line_number, "Using the result of a scope is not implemented yet.\n");
            ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)false);
            break;

        case AST_var_decl:
        case AST_call:
            ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)true);
            break;
        
        case AST_return:
            if (used) type_checker_error(n->line_number, "The 'return' statement has no value and therefore can't be used in an expression.\n");
            ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)true);
            break;
        
        case AST_if:
            annotate_used_visitor(n->_if.condition, true);
            annotate_used_visitor(n->_if.if_clause, used);
            annotate_used_visitor(n->_if.else_clause, used);
            break;

        case AST_while:
            if (used) type_checker_error(n->line_number, "The 'while' statement has no value and therefore can't be used in an expression.\n");
            annotate_used_visitor(n->_while.condition, true);
            annotate_used_visitor(n->_while.body, false);
            break;

        case AST_for:
            if (used) type_checker_error(n->line_number, "The 'for' statement has no value and therefore can't be used in an expression.\n");
            annotate_used_visitor(n->_for.initializer, false);
            annotate_used_visitor(n->_for.condition, true);
            annotate_used_visitor(n->_for.post_action, false);
            annotate_used_visitor(n->_for.body, false);
            break;
        
        case AST_binary:
            if (n->binary.token_kind == TOK_equal_assign) {
                ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)true);
            }
            else if (n->binary.token_kind == TOK_boolean_and || n->binary.token_kind == TOK_boolean_or) {
                annotate_used_visitor(n->binary.left, true);
                annotate_used_visitor(n->binary.right, used);
            }
            else {
                ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)used);
            }
            break;

        default:
            ast_visit_children(n, (AstVisitor)annotate_used_visitor, (void*)used);
            break;
    }
}

void type_propagate_binary_operator(AST_node *n) {
    NOT_IMPLEMENTED("Type binary operators is not implemented yet.\n");
}

typedef struct {
    Symbol *current_function_symbol;
} PropagationVisitorData;

void type_propagation_visitor(AST_node *n, PropagationVisitorData *prop) {
    // actions while traversing the tree down
    switch (n->kind) {
        case AST_function:
            prop->current_function_symbol = n->fun.symbol;
            ast_visit_children(n, (AstVisitor)type_propagation_visitor, prop);
            prop->current_function_symbol = nullptr;
            break;

        default:
            ast_visit_children(n, (AstVisitor)type_propagation_visitor, prop);
            break;
    }

    // actions while traversing the tree up
    switch (n->kind) {
        case AST_number:
            n->type = &builtin_i64_literal;
            break;
        case AST_var_decl:
        case AST_arg_decl:
            n->var_decl.symbol->type = &builtin_i64;
            break;
        case AST_symbol:
            n->type = n->symbol.symbol->type;
            break;
        case AST_binary:
            type_propagate_binary_operator(n);
            break;
        case AST_call:
            n->type = n->call.symbol->type->fun.return_type;
            break;

        case AST_return:
            ASSERT(prop->current_function_symbol, "Encountered 'return' without active function symbol\n")
            AST_node *ret_expression = n->ret.return_val;
            Type *ret_type;
            if (ret_expression) 
                ret_type = ret_expression->type;
            else
                ret_type = &builtin_void;

            Type **fun_ret_type = &prop->current_function_symbol->type->fun.return_type;

            if (*fun_ret_type == nullptr) {
                *fun_ret_type = ret_type;
            } else {
                if (*fun_ret_type != ret_type) {
                    char buf_1[1024];
                    char buf_2[1024];
                    type_checker_error(n->line_number, "Returning different types from the same function. Have '%s' and '%s'.",
                        get_type_name_r(buf_1, *fun_ret_type), get_type_name_r(buf_2, ret_type));
                }
            }
            break;

        case AST_function:
            // function return type handled in case for AST_return
            break;

        
        case AST_if:
            NOT_IMPLEMENTED("Type propagating if is not implemented yet.\n");
            break;
        
        case AST_while:
            NOT_IMPLEMENTED("Type propagating while is not implemented yet.\n");
            break;

        case AST_for:
            NOT_IMPLEMENTED("Type propagating for is not implemented yet.\n");
            break;

        case AST_string:
            NOT_IMPLEMENTED("Type propagating string literals is not implemented yet.\n");
            break;


        // these have void type
        case AST_program:
        case AST_scope:
            break;
        
    }

}

void run_typechecking(AST_node *root) {
    annotate_used_visitor(root, 0);

    PropagationVisitorData pvd = {0};
    type_propagation_visitor(root, &pvd);
}
