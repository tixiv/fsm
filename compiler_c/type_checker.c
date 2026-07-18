
#include "type_checker.h"
#include "ast.h"
#include "dyn_array.h"
#include "sv.h"
#include "tokenizer.h"
#include "type.h"
#include "common.h"
#include "string_builder.h"
#include "resolver.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

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


void insert_dereference(AST_node **n) {
    ASSERT (is_reference_kind((*n)->type), "Trying to insert dereference for something that is not a reference.\n")
    AST_node *deref = ast_alloc(AST_dereference, 0);
    deref->type = dereferenced_type((*n)->type);
    deref->addressable = true;
    ast_insert_node(n, deref);
}

void auto_dereference(AST_node **n) {
    if (!is_reference_kind((*n)->type)) return;
    insert_dereference(n);
}

void insert_take_reference(AST_node **n) {
    AST_node *ref = ast_alloc(AST_reference, 0);
    ref->type = get_ref_type_for((*n)->type);
    ref->addressable = false;
    ast_insert_node(n, ref);
}

void try_convert_to_type_if_necessary(AST_node **n, Type *target_type, const char *desc) {
    char buf_1[1024], buf_2[1024];

    Type *original_type = (*n)->type;

    if (target_type == original_type) return;

    if (!is_reference_kind(target_type) && is_reference_kind(original_type)) {
        insert_dereference(n);
    }
    
    if (is_slice_type(target_type) && is_array_kind((*n)->type)) {
        
        if (get_slice_element_type(target_type) == (*n)->type->_array.element_type) {
            AST_node *conv = ast_alloc(AST_array_to_slice, (*n)->line_number);
            conv->type = get_sclice_type((*n)->type->_array.element_type);
            ast_insert_node(n, conv);
            return;
        }
        else {
            type_checker_error((*n)->line_number, "Can't convert %s of type '%s' to '%s'.\n",
                desc, get_type_name_r(buf_1, original_type), get_type_name_r(buf_2, target_type));
        }
    }

    if (target_type == get_ref_type_for(&builtin_any)) {
        if (!is_reference_kind(original_type)) {
            if ((*n)->addressable == false) type_checker_error((*n)->line_number, "Can't convert %s of type '%s' to '%s' because it is not addressable.\n",
                    desc, get_type_name_r(buf_1, original_type), get_type_name_r(buf_2, target_type));
            
            insert_take_reference(n);
        }
        return; // any& can accept any reference type.
    }

    if (is_reference_kind(target_type) && !is_reference_kind(original_type)) {
        if ((*n)->addressable == false) type_checker_error((*n)->line_number, "Can't convert %s of type '%s' to '%s' because it is not addressable.\n",
                desc, get_type_name_r(buf_1, original_type), get_type_name_r(buf_2, target_type));
        insert_take_reference(n);
    }

    if ((*n)->type == target_type) return;

    if (is_integer_kind(target_type) && is_enum_kind((*n)->type)) return;
    if (is_enum_kind(target_type) && is_integer_kind((*n)->type)) return;

    const char *warning;
    if (is_castable_to(target_type, (*n)->type, &warning)) {
        ast_insert_node(n, make_cast(target_type, (*n)->type));
        if (warning) type_checker_warning((*n)->line_number, warning);
    }
    else {
        type_checker_error((*n)->line_number, "Can't convert %s of type '%s' to '%s'.\n",
            desc, get_type_name_r(buf_1, original_type), get_type_name_r(buf_2, target_type));
    }
}

void type_propagate_binary_operator(AST_node *n) {
    char buf_1[1024];
    char buf_2[1024];
    TokenKind tk = n->binary.token_kind;

    if (tk == TOK_bind_ref) {
        if (!is_reference_kind(n->binary.left->type)) {
            type_checker_error(n->line_number, "Operator %s requires a reference type on the left side. Have '%s' and '%s'.\n",
                token_kind_printable(tk), get_type_name_r(buf_1, n->binary.left->type), get_type_name_r(buf_2, n->binary.right->type));
        }
        if (!is_reference_kind(n->binary.right->type)) {
            if (!n->binary.right->addressable) type_checker_error(n->line_number, "Operator %s requires either another reference or something addressable on the right side. Have '%s' and '%s'.\n",
                token_kind_printable(tk), get_type_name_r(buf_1, n->binary.left->type), get_type_name_r(buf_2, n->binary.right->type));
            insert_take_reference(&n->binary.right);
        }

        Type *any_ref = get_ref_type_for(&builtin_any);

        if (n->binary.left->type != n->binary.right->type && n->binary.left->type != any_ref && n->binary.right->type != any_ref) {
            type_checker_error(n->line_number, "Operator %s requires the type on the right side to be referenceable by the left type. Have '%s' and '%s'.\n",
                token_kind_printable(tk), get_type_name_r(buf_1, n->binary.left->type), get_type_name_r(buf_2, n->binary.right->type));
        }
        
        n->type = n->binary.right->type;
    }
    else {
        SB sb_left; sb_init (&sb_left, buf_1, 1024);
        SB sb_right; sb_init (&sb_right, buf_2, 1024);

        sb_printf(&sb_left, "left argument of binary operator %s", token_kind_printable(tk));
        sb_printf(&sb_right, "right argument of binary operator %s", token_kind_printable(tk));

        switch(tk) {
            case TOK_plus:
            case TOK_minus:
            case TOK_asterisk:
            case TOK_slash:
            case TOK_percent:
                try_convert_to_type_if_necessary(&n->binary.left, &builtin_i64, sb_left.buffer);
                try_convert_to_type_if_necessary(&n->binary.right, &builtin_i64, sb_right.buffer);

                n->type = &builtin_i64;
                break;

            case TOK_greater:
            case TOK_lower:
            case TOK_greater_equal:
            case TOK_lower_equal:
                try_convert_to_type_if_necessary(&n->binary.left, &builtin_i64, sb_left.buffer);
                try_convert_to_type_if_necessary(&n->binary.right, &builtin_i64, sb_right.buffer);

                n->type = &builtin_bool;
                break;
            
            case TOK_boolean_and:
            case TOK_boolean_or:
                try_convert_to_type_if_necessary(&n->binary.left, &builtin_bool, sb_left.buffer);
                try_convert_to_type_if_necessary(&n->binary.right, &builtin_bool, sb_right.buffer);
                n->type = &builtin_bool;            
                break;
            
            case TOK_equal:
            case TOK_unequal: {
                auto_dereference(&n->binary.left);
                auto_dereference(&n->binary.right);
                bool okay = n->binary.left->type == n->binary.right->type || (is_integer_kind(n->binary.left->type) && is_integer_kind(n->binary.right->type));
                if (!okay) {
                    type_checker_error(n->line_number, "Operator %s can't accept arguments with different types. Have '%s' and '%s'.\n",
                        token_kind_printable(tk), get_type_name_r(buf_1, n->binary.left->type), get_type_name_r(buf_2, n->binary.right->type));
                }
                n->type = &builtin_bool;
                break;
            }

            case TOK_equal_assign:
                auto_dereference(&n->binary.left);
                auto_dereference(&n->binary.right);

                if (types_are_equivalent(n->binary.left->type, n->binary.right->type)) {
                }
                else if (is_integer_kind(n->binary.left->type) && is_integer_kind(n->binary.right->type)) {
                    // TODO: Warn if sizes / signs are different
                }
                else {
                    type_checker_error(n->line_number, "Operator %s can't accept arguments with different types. Have '%s' and '%s'.\n",
                        token_kind_printable(tk), get_type_name_r(buf_1, n->binary.left->type), get_type_name_r(buf_2, n->binary.right->type));
                }
                n->type = n->binary.right->type;
                break;

        

            default:
                NOT_IMPLEMENTED("Type checking binary operator of kind %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
                break;
        }
    }
}

void type_check_variadic_operator (AST_node *n){
    switch (n->variadic_operator.members[0].token_kind) {
        case TOK_greater:
        case TOK_lower:
        case TOK_greater_equal:
        case TOK_lower_equal:
            for (int i = 0; i < n->variadic_operator.num_members; i++) {
                try_convert_to_type_if_necessary(&n->variadic_operator.left, &builtin_i64, "Argument of variadic operator");
                try_convert_to_type_if_necessary(&n->variadic_operator.members[i].right, &builtin_i64, "Argument of variadic operator");                
            }
            n->type = &builtin_bool;
            n->addressable = false;
            break;
        
        case TOK_equal:
        case TOK_unequal:
            auto_dereference(&n->variadic_operator.left);
            
            Type *left_type = n->variadic_operator.left->type;
            bool all_same_type = true;
            bool all_integer = is_integer_kind(left_type);

            for (int i = 0; i < n->variadic_operator.num_members; i++) {
                auto_dereference(&n->variadic_operator.members[i].right);
                if (left_type != n->variadic_operator.members[i].right->type) all_same_type = false;
                if (!is_integer_kind(n->variadic_operator.members[i].right->type)) all_integer = false;
            }

            if (!all_same_type && !all_integer) {
                type_checker_error(n->line_number, "Chained operator %s can't accept arguments with different types.",
                    token_kind_printable(n->variadic_operator.members[0].token_kind));
            }
            n->type = &builtin_bool;
            n->addressable = false;
            break;

        default: NOT_IMPLEMENTED("Type checking variadic operator %s is not implemented yet.",
            token_kind_printable(n->variadic_operator.members[0].token_kind))
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
    Type *current_cuntion_type_decl;
    Dyn_array arg_symbols;
} PropagationVisitorData;

void type_propagation_visitor(AST_node *n, PropagationVisitorData *prop) {
    char buf_1[1024]; char buf_2[1024];

    // actions while traversing the tree down
    switch (n->kind) {
        case AST_function:
            prop->current_function_symbol = n->fun.symbol;
            prop->current_cuntion_type_decl = nullptr;
            n->fun.symbol->type = type_alloc(T_function);

            if (n->fun.ret_typedecl) {
                type_propagation_visitor(n->fun.ret_typedecl, prop);
                n->fun.symbol->type->fun.return_type = n->fun.ret_typedecl->type;
                prop->current_cuntion_type_decl = n->fun.ret_typedecl->type;
            }
            
            if (n->fun.args) type_propagation_visitor(n->fun.args, prop);
            if (n->fun.body) type_propagation_visitor(n->fun.body, prop);
            prop->current_function_symbol = nullptr;
            break;

        case AST_arg_list: {
            Symbol *s_fun = prop->current_function_symbol;
            ASSERT(s_fun, "Encountered arglist without active function symbol.\n");
            prop->arg_symbols.count = 0;

            // visiting the children of the arg list will fill up 'arg_symbols'
            ast_visit_children(n, (AstVisitor)type_propagation_visitor, prop);

            Type **arg_types = malloc(sizeof(Type*) * prop->arg_symbols.count);
            for (int i = 0; i < prop->arg_symbols.count; i++) {
                arg_types[i] = get_symbol(&prop->arg_symbols, i)->type;
            }

            s_fun->type->fun.num_arguments = prop->arg_symbols.count;
            s_fun->type->fun.argument_types = arg_types;
            break;
        }

        default:
            ast_visit_children(n, (AstVisitor)type_propagation_visitor, prop);
            break;
    }

    // actions while traversing the tree up
    switch (n->kind) {
        case AST_number:
            n->type = &builtin_i64;
            n->addressable = false;
            break;
        case AST_var_decl: {
            Type *t;
            if (n->var_decl._typedecl) {
                t = n->var_decl._typedecl->type;
            }
            else if (n->var_decl.initializer) {
                if (n->var_decl.initializer_operator == TOK_equal_assign) {
                    t = n->var_decl.initializer->type;
                    if (t == &builtin_void) {
                        type_checker_error(n->line_number, "Can't initialize variable with 'void' type.\n");
                    }
                    int ord = get_ref_order(t);
                    if (ord >= 2) {
                        type_checker_error(n->line_number, "Can't dereference references of order >1 in value initialization. Have '%'",
                            get_type_name_r(buf_1, t));
                    }
                    else if (ord) {
                        insert_dereference(&n->var_decl.initializer);
                        t = dereferenced_type(t);
                    }
                } else if (n->var_decl.initializer_operator == TOK_bind_ref) {
                    if (n->var_decl.initializer->type == &builtin_void) {
                        type_checker_error(n->line_number, "Can't bind a reference to 'void' type target.\n");
                    }
                    if (!is_reference_kind(n->var_decl.initializer->type)) {
                        if (!n->var_decl.initializer->addressable) type_checker_error(n->line_number, "Can't bind a reference to a non addressable target.\n");
                        t = get_ref_type_for(n->var_decl.initializer->type);
                        insert_take_reference(&n->var_decl.initializer);
                    }
                    else {
                        t = n->var_decl.initializer->type;
                    }
                } else {
                    NOT_IMPLEMENTED("Initialization with operator %s is not implemented.\n", token_kind_name(n->var_decl.initializer_operator));
                }
            }
            else {
                t = &builtin_i64;
            }
            n->var_decl.symbol->type = t;

            if (n->var_decl.initializer)
                try_convert_to_type_if_necessary(&n->var_decl.initializer, t, "Initializer");

            n->type = &builtin_void; // the declaration itself has no value.
            break;
        }
        case AST_arg_decl: {
            Type *t;
            if (n->arg_decl._typedecl)
                t = n->arg_decl._typedecl->type;
            else
                t = &builtin_i64;

            n->arg_decl.symbol->type = t;
            dyn_array_push_p(&prop->arg_symbols, n->arg_decl.symbol);
            n->type = &builtin_void; // the declaration itself has no value.
            break;
        }
        case AST_symbol: {
            Type *t = n->symbol.symbol->type;
            n->type = t;
            n->addressable = true;
            break;
        }

        case AST_binary:
            type_propagate_binary_operator(n);
            n->addressable = false;
            break;

        case AST_call:
            type_check_call_args(n);
            n->type = n->call.symbol->type->fun.return_type;
            if (!n->type) type_checker_error(n->line_number,
                "The return type for recursive function '%.*s' can't be determined automatically, please specify it!",
                SV_prnt(n->call.symbol->name));
            n->addressable = false;
            break;
        
        case AST_cast:
            ASSERT(n->type, "Cast without a type. It should have been set at construction.\n");
            n->addressable = n->_cast.body->addressable;
            break;

        case AST_return:
            ASSERT(prop->current_function_symbol, "Encountered 'return' without active function symbol\n")
            Type *ret_type;
            if (n->ret.body) 
                ret_type = n->ret.body->type;
            else
                ret_type = &builtin_void;

            Type **fun_ret_type = &prop->current_function_symbol->type->fun.return_type;

            if (*fun_ret_type == nullptr) {
                *fun_ret_type = ret_type;
            } else {
                if (prop->current_cuntion_type_decl && prop->current_cuntion_type_decl != &builtin_void) {
                    if (ret_type == &builtin_void) type_checker_error(n->line_number, "'return' without a value in function returning non void.\n");
                    try_convert_to_type_if_necessary(&n->ret.body, prop->current_cuntion_type_decl, "Return argument");
                } else if (!types_are_equivalent(*fun_ret_type, ret_type)) {
                    type_checker_error(n->line_number, "Returning different types from the same function. Have '%s' and '%s'. Please declare a return type!\n",
                        get_type_name_r(buf_1, *fun_ret_type), get_type_name_r(buf_2, ret_type));
                }
            }
            n->type = &builtin_void; // The return itself always has 'void' type.
            n->addressable = false;
            break;

        case AST_function:
            // function return type is handled in the 'case' for 'AST_return'.
            n->type = n->fun.symbol->type;
            n->addressable = false;
            break;

        case AST_while:
            try_convert_to_type_if_necessary(&n->_while.condition, &builtin_bool, "'while' loop condition");
            n->type = &builtin_void;
            n->addressable = false;
            break;

        case AST_for:
            n->type = &builtin_void;
            if (n->_for.condition)
                try_convert_to_type_if_necessary(&n->_for.condition, &builtin_bool, "'for' loop condition");
            if (n->_for.result)
                n->type = n->_for.result->type;

            n->addressable = false;
            break;
        
        case AST_if:
            try_convert_to_type_if_necessary(&n->_if.condition, &builtin_bool, "'if' condition");
            Type *_if_type = n->_if.if_clause->type;
            Type *_else_type = n->_if.else_clause ? n->_if.else_clause->type : &builtin_void;
            if (n->result_used && _if_type != _else_type) {
                type_checker_error(n->line_number,
                    "When the result of 'if' is used then it has to have an 'else', and both branches must have the same type. Have '%s' and '%s'.\n",
                    get_type_name_r(buf_1, _if_type), get_type_name_r(buf_2, _else_type));
            }
            n->type = _if_type;
            n->addressable = false;
            break;

        case AST_string:
            n->type = &builtin_u8_slice;
            n->addressable = false;
            break;

        case AST_char_constant:
            n->type = &builtin_i32;
            n->addressable = false;
            break;

        case AST_array_access:
            if (is_reference_kind(n->_array.array->type))
                insert_dereference(&n->_array.array);

            if (is_array_kind(n->_array.array->type)) {
                n->type = get_ref_type_for(n->_array.array->type->_array.element_type);    
            }
            else if (is_slice_type(n->_array.array->type)) {
                n->type = get_ref_type_for(get_slice_element_type(n->_array.array->type));
            }
            else {
                type_checker_error(n->line_number,
                    "Invalid use of [] operator on something that is not an array or a slice. Have '%s'.\n",
                    get_type_name_r(buf_1, n->_array.array->type));
            }


            if (!is_integer_kind(n->_array.index->type)) {
                type_checker_error(n->line_number,
                    "Invalid use of non integer type '%s' as an array index.\n",
                    get_type_name_r(buf_1, n->_array.index->type));

            }
            
            n->addressable = true;
            break;

        case AST_dereference:
            if (!is_reference_kind(n->deref.body->type)) {
                    type_checker_error(n->line_number,
                        "Dereferencing something that is not a reference. Have '%s'.\n",
                        get_type_name_r(buf_1, n->deref.body->type));
            } else {
                n->type = dereferenced_type(n->deref.body->type);
                n->addressable = true;
            }
            break;

        case AST_reference:
            if (!n->reference.body->addressable) type_checker_error(n->line_number, "Can't take reference to something that is not addressable.\n",
                get_type_name_r(buf_1, n->reference.body->type));
            
            n->type = get_ref_type_for(n->reference.body->type);
            n->addressable = false;
            break;

        case AST_unary:
            if (n->unary.token_kind == TOK_exclam) {
                try_convert_to_type_if_necessary(&n->unary.body, &builtin_bool, "Argument of unary '!'");
                n->type = &builtin_bool;
            }
            else if (n->unary.token_kind == TOK_minus) {
                try_convert_to_type_if_necessary(&n->unary.body, &builtin_i64, "Argument of unary '-'");
                n->type = &builtin_i64;
            }
            else NOT_IMPLEMENTED("Type checking unary operator %s is not implemented yet.\n", token_kind_printable(n->unary.token_kind));
            n->addressable = false;
            break;
        
        case AST_plus_plus: {
            Type * original_type = n->plus_plus.body->type;
            auto_dereference(&n->plus_plus.body);
            if (is_slice_type(n->plus_plus.body->type)) {
                if (n->result_used) type_checker_error(n->line_number, "Operator '++' on slice type argument has no value which can be used.\n");
                n->type = &builtin_void;
            }
            else if (is_integer_kind(n->plus_plus.body->type)) {
                n->type = n->plus_plus.body->type;
            }
            else{
                type_checker_error(n->line_number, "Operator '++' needs integer or slice type argument. Have '%s'",
                    get_type_name_r(buf_1, original_type));
            }
            
            break;
        }

        case AST_minus_minus: {
            Type * original_type = n->minus_minus.body->type;
            auto_dereference(&n->minus_minus.body);
            if (!is_integer_kind(n->minus_minus.body->type)) {
                type_checker_error(n->line_number, "Operator '--' needs integer type argument. Have '%s'",
                    get_type_name_r(buf_1, original_type));
            }
            n->type = n->minus_minus.body->type;
            break;
        }

        case AST_member_access: {
            ASSERT(n->member_access.body->type, "Unresolved type encountered in member access.\n");
            Type *container = n->member_access.body->type;
            if (is_reference_kind(container)) container = dereferenced_type(container);
            
            if (!type_can_have_members(container))
                type_checker_error(n->line_number, "Tried to access a member in something that can't have members. Have '%s'.\n",
                        get_type_name_r(buf_1, container));

            if (is_struct_kind(container)) {
                size_t offset;
                Type *t = get_member_type_and_offset(container, &n->member_access.name, &offset);
                if (!t) type_checker_error(n->line_number, "Member '%.*s' not found in '%s'.\n", SV_prnt(n->member_access.name),
                        get_type_name_r(buf_1, container));

                if (!is_reference_kind(n->member_access.body->type))
                    insert_take_reference(&n->member_access.body);

                n->type = get_ref_type_for(t);
                n->member_access.offset = offset;
                n->addressable = true;

                // We need to insert a dereference node now at this point in the tree.
                // copy the current node to a new one:
                AST_node *ast_member_access = ast_alloc(AST_member_access, n->line_number);
                memcpy(ast_member_access, n, sizeof(AST_node));
                ast_member_access->next = nullptr; // clear next pointer in case it was used

                // Now we overwrite the current node, making it the dereference node with the member acces in it's body
                n->kind = AST_dereference;
                n->deref.body = ast_member_access;
                n->type = dereferenced_type(ast_member_access->type);
            }
            else if (is_array_kind(container)) {
                if(sv_compare_cstr(&n->member_access.name, "len")) {
                    n->kind = AST_array_len;
                    n->array_len.len = container->_array.n_elements;
                    n->addressable = false;
                    n->type = &builtin_i64;
                }
                else {
                    type_checker_error(n->line_number, "Tried to access non existent member '%.*s' on array type. Arrays only have the member 'len' available.\n",
                            SV_prnt(n->member_access.name));
                }
            }
            else if (is_enum_kind(container)) {
                if(sv_compare_cstr(&n->member_access.name, "name")) {
                    n->addressable = false;
                    n->type = &builtin_u8_slice;
                }
                else {
                    type_checker_error(n->line_number, "Tried to access non existent member '%.*s' on enum type. Enums only have the member 'name' available.\n",
                            SV_prnt(n->member_access.name));
                }
            }
            else {
                NOT_IMPLEMENTED("accessing members in '%s' is not implemented yet.\n", get_type_name_r(buf_1, container));
            }

            break;
        }

        case AST_namespace_access: {
            Type *t = n->namespace_access.body->type;
            if (t->kind == T_enum) {
                if (!get_enum_member_value (t, &n->namespace_access.name, &n->namespace_access.enum_value))
                {
                    type_checker_error(n->line_number, "Member '%.*s' not found in '%s'.\n", SV_prnt(n->namespace_access.name),
                        get_type_name_r(buf_1, t));
                }
                n->type = t;
                n->addressable = false;
            }
            else NOT_IMPLEMENTED("Namespace access is not implemented yet for typ %s.\n", get_type_name_r(buf_1, t));
            break;
        }

        case AST_variadic_operator:
            type_check_variadic_operator(n);
            break;

        case AST_typename:
        case AST_member_def:
        case AST_struct:
        case AST_enum:
        case AST_enum_member:
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
            // already filled in by type resolver
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
    dyn_array_init(&pvd.arg_symbols, sizeof(void*), 8);
    type_propagation_visitor(root, &pvd);

    annotate_used_visitor(root, 0);
}
