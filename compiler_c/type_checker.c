
#include "type_checker.h"
#include "ast.h"
#include "tokenizer.h"
#include "type.h"
#include "common.h"
#include "string_builder.h"
#include <stdarg.h>
#include <stdint.h>

static void type_checker_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Type Checker] %s:%d Error: ", current_filename, line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static void type_checker_warning(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Type Checker] %s:%d Warning: ", current_filename, line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

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

void try_convert_to_type_if_necessary(AST_node **n, Type *target_type, const char *desc) {
    
    if (!is_reference_kind(target_type) && is_reference_kind((*n)->type)) {
        AST_node *load = ast_alloc(AST_load, 0);
        load->type = dereferenced_type((*n)->type);
        ast_insert_node(n, load);
    }
    
    if ((*n)->type == target_type) return;

    const char *warning;
    if (is_castable_to(target_type, (*n)->type, &warning)) {
        ast_insert_node(n, make_cast(target_type, (*n)->type));
        if (warning) type_checker_warning((*n)->line_number, warning);
    }
    else {
        char buf_1[1024], buf_2[1024];
        type_checker_error((*n)->line_number, "Can't convert %s of type '%s' to '%s'.\n",
            desc, get_type_name_r(buf_1, (*n)->type), get_type_name_r(buf_2, target_type));
    }
}

void type_propagate_binary_operator(AST_node *n) {
    TokenKind tk = n->binary.token_kind;

    char buf_1[1024]; SB sb_left; sb_init (&sb_left, buf_1, 1024);
    char buf_2[1024]; SB sb_right; sb_init (&sb_right, buf_2, 1024);

    sb_printf(&sb_left, "left argument of binary operator %s", token_kind_printable(tk));
    sb_printf(&sb_right, "left argument of binary operator %s", token_kind_printable(tk));

    Type *left_type = n->binary.left->type;
    Type *right_type = n->binary.right->type;

    switch(tk) {
        case TOK_plus:
        case TOK_minus:
        case TOK_asterisk:
        case TOK_slash:
        case TOK_percent:
        case TOK_greater:
        case TOK_lower:
        case TOK_greater_equal:
        case TOK_lower_equal:
            try_convert_to_type_if_necessary(&n->binary.left, &builtin_i64, sb_left.buffer);
            try_convert_to_type_if_necessary(&n->binary.right, &builtin_i64, sb_right.buffer);

            n->type = &builtin_i64;
            break;
        
        case TOK_boolean_and:
        case TOK_boolean_or:
            try_convert_to_type_if_necessary(&n->binary.left, &builtin_bool, sb_left.buffer);
            try_convert_to_type_if_necessary(&n->binary.right, &builtin_bool, sb_right.buffer);
            n->type = &builtin_bool;            
            break;
        
        case TOK_equal:
        case TOK_unequal: {
            // TODO: improve this check
            bool okay = left_type == right_type || (is_integer_kind(left_type) && is_integer_kind(right_type));
            if (!okay) {
                type_checker_error(n->line_number, "Operator %s can't accept arguments with different types. Have '%s' and '%s'.\n",
                    token_kind_printable(tk), get_type_name_r(buf_1, left_type), get_type_name_r(buf_2, right_type));
            }
            n->type = &builtin_bool;
            break;
        }

        case TOK_equal_assign:
            if (types_are_equivalent(left_type, right_type)) {
            }
            else if (is_array_kind(left_type) && is_reference_kind(right_type)) {
                // TODO: check if target type is compatible
            }
            else if (is_integer_kind(left_type) && is_integer_kind(right_type)) {
                // TODO: Warn if sizes / signs are different
            }
            else {
                type_checker_error(n->line_number, "Operator %s can't accept arguments with different types. Have '%s' and '%s'.\n",
                    token_kind_printable(tk), get_type_name_r(buf_1, left_type), get_type_name_r(buf_2, right_type));
            }
            n->type = right_type;
            break;

        default:
            NOT_IMPLEMENTED("Type checking binary operator of kind %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
            break;
    }

}

void type_check_call_args(AST_node *n_call) {
    Type *symbol_type = n_call->call.symbol->type;
    int num_args = symbol_type->fun.num_arguments;

    AST_node **arg = &n_call->call.args;
    for (int i = 0; i < num_args; i++) {
        Type *expected_type = symbol_type->fun.argument_types[i];
        if (*arg == nullptr) type_checker_error(n_call->line_number, "Not enough arguments to function call.\n");
        try_convert_to_type_if_necessary(arg, expected_type, "Function argument");

        arg = &(*arg)->next;
    }

    if (*arg) type_checker_error(n_call->line_number, "Too many arguments to function call.\n");
}

typedef struct {
    Symbol *current_function_symbol;
} PropagationVisitorData;

void type_propagation_visitor(AST_node *n, PropagationVisitorData *prop) {
    char buf_1[1024]; char buf_2[1024];

    // actions while traversing the tree down
    switch (n->kind) {
        case AST_function:
            prop->current_function_symbol = n->fun.symbol;
            n->type = n->fun.symbol->type;
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
            n->type = &builtin_i64;
            break;
        case AST_var_decl:
            if (n->var_decl.initializer)
                n->var_decl.symbol->type = n->var_decl.initializer->type;
            else
                n->var_decl.symbol->type = &builtin_i64;
            n->type = &builtin_void; // the declaration itself has no value.
            break;
        case AST_arg_decl:
            n->var_decl.symbol->type = &builtin_i64;
            n->type = &builtin_void; // the declaration itself has no value.
            break;
        case AST_symbol:
            n->type = n->symbol.symbol->type;
            break;
        case AST_binary:
            type_propagate_binary_operator(n);
            break;
        case AST_call:
            type_check_call_args(n);
            n->type = n->call.symbol->type->fun.return_type;
            break;
        
        case AST_cast:
            ASSERT(n->type, "Cast without a type. It should have been set at construction.\n");
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
                if (!types_are_equivalent(*fun_ret_type, ret_type)) {
                    type_checker_error(n->line_number, "Returning different types from the same function. Have '%s' and '%s'.",
                        get_type_name_r(buf_1, *fun_ret_type), get_type_name_r(buf_2, ret_type));
                }
            }
            n->type = &builtin_void; // The return itself always has 'void' type.
            break;

        case AST_function:
            // function return type is handled in the 'case' for 'AST_return'.
            break;

        case AST_while:
            try_convert_to_type_if_necessary(&n->_while.condition, &builtin_bool, "'while' loop condition");
            n->type = &builtin_void;
            break;

        case AST_for:
            try_convert_to_type_if_necessary(&n->_for.condition, &builtin_bool, "'for' loop condition");
            n->type = &builtin_void;
            break;
        
        case AST_if:
            try_convert_to_type_if_necessary(&n->_if.condition, &builtin_bool, "'if' condition");
            Type *_if_type = n->_if.if_clause->type;
            Type *_else_type = n->_if.else_clause ? n->_if.else_clause->type : &builtin_void;
            if (_if_type != _else_type) {
                type_checker_error(n->line_number,
                    "When the result of 'if' is used then it has to have an 'else', and both branches must have the same type. Have '%s' and '%s'.\n",
                    get_type_name_r(buf_1, _if_type), get_type_name_r(buf_2, _else_type));
            }
            n->type = _if_type;
            break;

        case AST_string:
            n->type = &builtin_u8_array;
            break;

        case AST_array_access:
            if (!is_array_kind(n->_array.array->type)) {
                type_checker_error(n->line_number,
                    "Invalid use of [] operator on something that is not an array. Have '%s'.\n",
                    get_type_name_r(buf_1, n->_array.array->type));
            }
            if (!is_integer_kind(n->_array.index->type)) {
                type_checker_error(n->line_number,
                    "Invalid use of non integer type '%s' as an array index.\n",
                    get_type_name_r(buf_1, n->_array.index->type));

            }
            n->type = get_ref_type_for_array_type(n->_array.array->type);
            break;

        case AST_dereference:
            if (!is_reference_kind(n->deref.body->type)) {
                type_checker_error(n->line_number,
                    "Dereferencing something that is not a reference. Have '%s'.\n",
                    get_type_name_r(buf_1, n->deref.body->type));
            }
            n->type = dereferenced_type(n->deref.body->type);
            break;

        // these have void type
        case AST_arg_list:
        case AST_program:
        case AST_scope:
            n->type = &builtin_void;
            break;
        default:
            NOT_IMPLEMENTED("Type checking %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }

    ASSERT(n->type, "All nodes should have a type after the type checker. Have %s with null type.\n", ast_kind_name(n->kind));
}

void run_typechecking(AST_node *root) {
    PropagationVisitorData pvd = {0};
    type_propagation_visitor(root, &pvd);

    annotate_used_visitor(root, 0);
}
