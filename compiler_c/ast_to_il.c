
#include "ast_to_il.h"
#include "ast.h"
#include "common.h"
#include "location.h"
#include "opcodes.h"
#include "string_builder.h"
#include "sv.h"
#include "symbol.h"
#include "tokenizer.h"
#include "type.h"
#include "parser_ast.h"
#include "symbol.h"
#include <stdarg.h>
#include <stdint.h>

static void il_gen_error(const Location *location, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (location)
        fprintf(stderr, "[FSM IL Gen] %s:%d Error: ", location->filename, location->line);
    else
        fprintf(stderr, "[FSM IL Gen] (unknown location) Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static void il_gen_warning(const Location *location, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (location)
        fprintf(stderr, "[FSM IL Gen] %s:%d Warning: ", location->filename, location->line);
    else
        fprintf(stderr, "[FSM IL Gen] (unknown location) Warning: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
}

static int num_ifs;
static int num_whiles;
static int num_strings;

typedef struct {
} IL_gen;

static void il_gen_push_symbol_address(Symbol *s, const Location *location) {
    if (s->kind == SYM_local) {
        push_opcode(OP_push_local_var_address, nullptr, s->offset, nullptr, location);
    }
    else if (s->kind == SYM_arg) {
        push_opcode(OP_push_arg_address, nullptr, s->offset, nullptr, location);
    }
    else if (s->kind == SYM_function) {
        push_opcode(OP_push_global_address, &s->name, 0, nullptr, location);
    }
    else {
        NOT_IMPLEMENTED("Pushing address for symbol kind %s is not implemented yet.\n", symbol_kind_name(s->kind));
    }
}

static void il_gen_push_symbol(Symbol *s, const Location *location) {
    if (s->kind == SYM_arg) {
        push_opcode(OP_push_arg, nullptr, s->offset, s->type, location);
    } else if (s->kind == SYM_local) {
        push_opcode(OP_push_local_var, nullptr,  s->offset, s->type, location);
    } else {
        NOT_IMPLEMENTED("Pushing symbol kind %s is not implemented yet.\n", symbol_kind_name(s->kind));
    }
}

static void gen_value_visitor(AST_node *n, IL_gen *gen);
static void gen_address_visitor(AST_node *n, IL_gen *gen);
static void il_gen_visitor(AST_node *n, IL_gen *gen);

static void gen_binary_operators(AST_node *n, IL_gen *gen, bool result_used) {
    if (TOK_boolean_and == n->binary.token_kind) {
        // short circuit logic:
        // bar = a && b; -> bar = if(a) (bool)b else 0
        int if_num = num_ifs++;
        gen_value_visitor(n->binary.left, gen);

        push_opcode_2(OP_if, nullptr, if_num, 1, nullptr, n->location);
        
        if (result_used) gen_value_visitor(n->binary.right, gen); else il_gen_visitor(n->binary.right, gen);

        push_opcode(OP_else, nullptr, if_num, nullptr, n->location);
        
        if (result_used)
            push_opcode(OP_push_literal, &mkSV("0"), 0, nullptr, n->location);

        push_opcode(OP_end_if, nullptr, if_num, nullptr, n->location);
    }
    else if (TOK_boolean_or == n->binary.token_kind) {
        // short circuit logic:
        // bar = a || b; -> bar = if(a) 1 else (bool)b
        int if_num = num_ifs++;
        gen_value_visitor(n->binary.left, gen);

        push_opcode_2(OP_if, nullptr, if_num, 1, nullptr, n->location);
        
        if (result_used)
            push_opcode(OP_push_literal, &mkSV("1"), 0, nullptr, n->location);
        
        push_opcode(OP_else, nullptr, if_num, nullptr, n->location);

        if (result_used) gen_value_visitor(n->binary.right, gen); else il_gen_visitor(n->binary.right, gen);
        
        push_opcode(OP_end_if, nullptr, if_num, nullptr, n->location);
    }
    else if (TOK_equal_assign == n->binary.token_kind || TOK_bind_ref == n->binary.token_kind) {
        if (!n->binary.left->addressable) {
            il_gen_error(n->location, "Trying to assign to something that is not addressable. Have %s.\n",
                    ast_kind_name(n->binary.left->kind));
        }
        gen_address_visitor(n->binary.left, gen);
        gen_value_visitor(n->binary.right, gen);

        if (result_used)
            push_opcode(OP_store_and_dup, nullptr, 0, n->binary.left->type, n->location);
        else
            push_opcode(OP_store, nullptr, 0, n->binary.left->type, n->location);
    }
    else {
        if (result_used)
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        else
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);;

        switch (n->binary.token_kind) {
            case TOK_plus:          if (result_used) push_opcode(OP_add,                nullptr, 0, nullptr, n->location); break;
            case TOK_minus:         if (result_used) push_opcode(OP_sub,                nullptr, 0, nullptr, n->location); break;
            case TOK_asterisk:      if (result_used) push_opcode(OP_mul,                nullptr, 0, nullptr, n->location); break;
            case TOK_slash:         if (result_used) push_opcode(OP_div,                nullptr, 0, nullptr, n->location); break;
            case TOK_percent:       if (result_used) push_opcode(OP_mod,                nullptr, 0, nullptr, n->location); break;
            case TOK_up_arrow:      if (result_used) push_opcode(OP_ipow,               nullptr, 0, nullptr, n->location); break;
            case TOK_equal:         if (result_used) push_opcode(OP_equal,              nullptr, 0, nullptr, n->location); break;
            case TOK_unequal:       if (result_used) push_opcode(OP_unequal,            nullptr, 0, nullptr, n->location); break;
            case TOK_greater:       if (result_used) push_opcode(OP_compare_GT,         nullptr, 0, nullptr, n->location); break;
            case TOK_lower:         if (result_used) push_opcode(OP_compare_LT,         nullptr, 0, nullptr, n->location); break;
            case TOK_greater_equal: if (result_used) push_opcode(OP_compare_GE,         nullptr, 0, nullptr, n->location); break;
            case TOK_lower_equal:   if (result_used) push_opcode(OP_compare_LE,         nullptr, 0, nullptr, n->location); break;
            case TOK_reference_target_equal:   if (result_used) push_opcode(OP_equal,   nullptr, 0, nullptr, n->location); break;
            case TOK_reference_target_unequal: if (result_used) push_opcode(OP_unequal, nullptr, 0, nullptr, n->location); break;

            default:
                NOT_IMPLEMENTED("Generating IL for binary operator %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
        }
    }
}

static void gen_variadic_operator_members_and(AST_node *n, IL_gen *gen, size_t i) {
    gen_value_visitor(n->variadic_operator.members[i].right, gen);

    switch (n->variadic_operator.members[i].token_kind) {
        case TOK_greater:       push_opcode(OP_compare_GT, nullptr, 1, nullptr, n->location); break;
        case TOK_lower:         push_opcode(OP_compare_LT, nullptr, 1, nullptr, n->location); break;
        case TOK_greater_equal: push_opcode(OP_compare_GE, nullptr, 1, nullptr, n->location); break;
        case TOK_lower_equal:   push_opcode(OP_compare_LE, nullptr, 1, nullptr, n->location); break;
        case TOK_and_not_equal_to:
        case TOK_unequal:       push_opcode(OP_unequal, nullptr, 1, nullptr, n->location); break;

        default: ASSERT(false, "Illegal comparison operator.\n");
    }
    
    // short circuit logic:
    int if_num = num_ifs++;
    push_opcode_2(OP_if, nullptr, if_num, 1, nullptr, n->location);
    if (i+1 < n->variadic_operator.num_members) {
        gen_variadic_operator_members_and(n, gen, i+1);
    }
    else {
        push_opcode(OP_pop, nullptr, 0, nullptr, n->location);
        push_opcode(OP_push_literal, &mkSV("1"), 0, nullptr, n->location);
    }

    push_opcode(OP_else, nullptr, if_num, nullptr, n->location);
    push_opcode(OP_pop, nullptr, 0, nullptr, n->location);
    push_opcode(OP_push_literal, &mkSV("0"), 0, nullptr, n->location);
    push_opcode(OP_end_if, nullptr, if_num, nullptr, n->location);
}

static void gen_variadic_operator_members_or(AST_node *n, IL_gen *gen, size_t i) {
    gen_value_visitor(n->variadic_operator.members[i].right, gen);

    switch (n->variadic_operator.members[i].token_kind) {
        case TOK_equal:
        case TOK_or_equal_to:
            push_opcode(OP_equal, nullptr, 1, nullptr, n->location);
            break;
        default: ASSERT(false, "Illegal comparison operator.\n");
    }
    
    // short circuit logic:
    int if_num = num_ifs++;
    push_opcode_2(OP_if, nullptr, if_num, 1, nullptr, n->location);
    push_opcode(OP_pop, nullptr, 0, nullptr, n->location);
    push_opcode(OP_push_literal, &mkSV("1"), 0, nullptr, n->location);
    push_opcode(OP_else, nullptr, if_num, nullptr, n->location);
    if (i+1 < n->variadic_operator.num_members) {
        gen_variadic_operator_members_or(n, gen, i+1);
    }
    else
    {
        push_opcode(OP_pop, nullptr, 0, nullptr, n->location);
        push_opcode(OP_push_literal, &mkSV("0"), 0, nullptr, n->location);
    }

    push_opcode(OP_end_if, nullptr, if_num, nullptr, n->location);
}

static void gen_variadic_operators(AST_node *n, IL_gen *gen, bool result_used) {
    TokenKind first_token = n->variadic_operator.members[0].token_kind;

    if (is_comparison_operator(first_token) || first_token == TOK_unequal) {
        gen_value_visitor(n->variadic_operator.left, gen);
        gen_variadic_operator_members_and(n, gen, 0);
    }
    else if (first_token == TOK_equal) {
        gen_value_visitor(n->variadic_operator.left, gen);
        gen_variadic_operator_members_or(n, gen, 0);
    }
    else NOT_IMPLEMENTED("Generating IL for variadic operator %s is not implemented yet.\n", token_kind_name(first_token));

    if (!result_used) push_opcode(OP_pop, nullptr, 0, nullptr, n->location);
}

static void gen_cast(AST_node *n, IL_gen *gen, bool result_used) {
    char buf_1[1024], buf_2[1024];

    Type *to = n->type;
    Type *from;

    if (n->kind == AST_cast) {
        from = n->_cast.right_type;
    }
    else if (n->kind == AST_user_cast) {
        from = n->user_cast.body->type;
    }
    else {
        NOT_IMPLEMENTED("gen_cast is not implemented for %s\n", ast_kind_name(n->kind));
    }
    
    if (!result_used) {
        il_gen_error(n->location, "Cast with unused result is not supported.\n");
    }

    if (is_boolean_kind(to) && is_integer_kind(from)) {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        push_opcode(OP_to_bool, nullptr, 0, nullptr, n->location);
    }
    else if (is_boolean_kind(to) && is_reference_kind(from)) {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        push_opcode(OP_to_bool, nullptr, 0, nullptr, n->location);
    }
    else if (is_integer_kind(to) && is_boolean_kind(from)) {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        // No cast needed. our 64 bit bools can be used as integer directly.
    }
    else if (is_integer_kind(to) && from->kind == T_unsigned_integer &&
            to->integer.num_bits >= from->integer.num_bits)
    {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        // No cast needed.
    }
    else if (to->kind == T_signed_integer && from->kind == T_signed_integer) {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        if (to->storage_size > from->storage_size)
            push_opcode(OP_sign_extend, nullptr, 0, from, n->location);
    }
    else if (is_integer_kind(to) && from->kind == T_signed_integer) {
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        // Just put no cast for now, let's fix potential problems later
    }
    else if (to == &builtin_u8_slice && (is_record_kind(from))) {
        push_opcode(OP_push_literal, nullptr, get_storage_size(from), nullptr, n->location);
        ast_visit_children(n, (AstVisitor)gen_address_visitor, gen);
    }
    else if (is_integer_kind(to) && is_reference_kind(from) && n->kind == AST_user_cast) {
        // cast pointer to integer
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    }
    else if (is_integer_kind(to) && is_enumerator_kind(from) && n->kind == AST_user_cast) {
        // cast enumerator to integer
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    }
    else if (is_integer_kind(to) && is_enum_kind(from) && n->kind == AST_user_cast) {
        // cast enum value to integer
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    }
    else if (is_reference_kind(to) && is_reference_kind(from) && n->kind == AST_user_cast) {
        // cast one reference type to another
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    }
    else if (is_reference_kind(to) && is_integer_kind(from) && n->kind == AST_user_cast) {
        // cast integer to reference
        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    }
    else {
        NOT_IMPLEMENTED("Generating IL for cast to '%s' from '%s' is not implemented yet.\n",
            get_type_name_r(buf_1, to), get_type_name_r(buf_2, from));
    }
}

static void gen_call(AST_node *n, IL_gen *gen, bool result_used) {
    if (n->call.target->kind == AST_symbol && is_function_kind(n->call.target->type)) {
        Symbol *s_call = n->call.target->symbol.symbol;
        SV name = n->call.target->symbol.name;
        ASSERT(s_call, "Symbol for called function '%.*s' is not resolved\n", SV_prnt(name));
        ast_visit_chain(n->call.args, (AstVisitor)gen_value_visitor, gen);
        if (sv_compare_cstr(&name, "bittest")) {
            push_opcode(OP_bittest, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "bitshift")) {
            push_opcode(OP_bitshift, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "bitand")) {
            push_opcode(OP_bitand, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "bitor")) {
            push_opcode(OP_bitor, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "bitxor")) {
            push_opcode(OP_bitxor, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "bitnot")) {
            push_opcode(OP_bitnot, nullptr, 0, nullptr, n->location);
        }
        else if (sv_compare_cstr(&name, "setbit")) {
            push_opcode(OP_setbit, nullptr, 0, nullptr, n->location);
        }
        else {
            push_opcode(OP_call, &name, 0, s_call->type, n->location);
            if (result_used)
                push_opcode(OP_push_result, nullptr, 0, n->type, n->location);
        }
    }
    else if (n->call.target->kind == AST_generic_speciation
             && n->call.target->generic_speciation.body->kind == AST_symbol
             && is_function_kind(n->call.target->type))
    {
        Symbol *s_call = n->call.target->generic_speciation.body->symbol.symbol;
        SV name = n->call.target->generic_speciation.body->symbol.name;
        ASSERT(s_call, "Symbol for called function '%.*s' is not resolved\n", SV_prnt(name));
        ast_visit_chain(n->call.args, (AstVisitor)gen_value_visitor, gen);
        push_opcode(OP_call, &name, 0, s_call->type, n->location);
        if (result_used)
            push_opcode(OP_push_result, nullptr, 0, n->type, n->location);
    }
    else if (is_reference_kind(n->call.target->type)
        && is_function_kind(dereferenced_type(n->call.target->type)))
    {
        Type *fn_type = dereferenced_type(n->call.target->type);
        ast_visit_chain(n->call.args, (AstVisitor)gen_value_visitor, gen);
        gen_value_visitor(n->call.target, gen);
        push_opcode(OP_icall, nullptr, 0, fn_type, n->location);
        if (result_used)
            push_opcode(OP_push_result, nullptr, 0, n->type, n->location);
    }
    else {
        NOT_IMPLEMENTED("Calling something that is not implemented.\n");
        //        ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);

    }
}

static void gen_if(AST_node *n, IL_gen *gen, bool result_used) {
    int if_num = num_ifs++;
    gen_value_visitor(n->_if.condition, gen);
    push_opcode_2(OP_if, nullptr, if_num, n->_if.else_clause ? 1:0, nullptr, n->location);
    if (result_used) gen_value_visitor(n->_if.if_clause, gen); else il_gen_visitor(n->_if.if_clause, gen);
    if (n->_if.else_clause) {
        push_opcode(OP_else, nullptr, if_num, nullptr, n->location);
        if (result_used) gen_value_visitor(n->_if.else_clause, gen); else il_gen_visitor(n->_if.else_clause, gen);
    }
    push_opcode(OP_end_if, nullptr, if_num, nullptr, n->location);
}

static void gen_plus_plus(AST_node *n, IL_gen *gen, bool result_used) {
    gen_address_visitor(n->plus_plus.body, gen);
    if (is_integer_kind(n->plus_plus.body->type)) {
        push_opcode(OP_integer_plus_plus, nullptr,
            result_used ? (n->plus_plus.postfix + 1) : 0, n->type, n->location);
    }
    else if (is_slice_kind(n->plus_plus.body->type)) {
        Type *element_type = get_slice_element_type(n->plus_plus.body->type);
        push_opcode(OP_slice_plus_plus, nullptr,
            result_used ? (n->plus_plus.postfix + 1) : 0, element_type, n->location);
    }
    else NOT_IMPLEMENTED("AST_plus_plus is not implemented for anything that is not an integer or a slice.\n")
}

static void gen_for(AST_node *n, IL_gen *gen, bool result_used) {
    int while_num = num_whiles++;
    il_gen_visitor(n->_for.initializer, gen);
    push_opcode(OP_while_loop, 0, while_num, nullptr, n->location);
    if (n->_for.condition) {
        gen_value_visitor(n->_for.condition, gen);
        push_opcode(OP_while_check, 0, while_num, nullptr, n->location);
    }
    il_gen_visitor(n->_for.body, gen);
    il_gen_visitor(n->_for.post_action, gen);
    push_opcode(OP_while_end, 0, while_num, nullptr, n->location);
    if (result_used) gen_value_visitor(n->_for.result, gen);
    else il_gen_visitor(n->_for.result, gen);;
}

static void gen_address_visitor(AST_node *n, IL_gen *gen) {

    // char buf[1024];

    switch (n->kind) {
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "Symbol for variable assignment to '%.*s' is not resolved\n", SV_prnt(n->symbol.name));

            il_gen_push_symbol_address(s, n->location);
            break;
        }

        case AST_dereference:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            break;

        default:
            il_gen_error(n->location, "Trying to take the address of something for which it is not possible. Have %s.\n",
                    ast_kind_name(n->kind));
            break;
    }
}

static void gen_builder_string_put_struct (Symbol *s_sb, AST_node *arg, IL_gen *gen) {
    char buf[1024];

    Symbol *sb_puts_symbol = get_symbol_by_name(&global_symbols, &mkSV("sb_puts"));
    Symbol *sb_puti_symbol = get_symbol_by_name(&global_symbols, &mkSV("sb_puti"));
    ASSERT(sb_puts_symbol, "sb_puts not found.\n");
    ASSERT(sb_puti_symbol, "sb_puti not found.\n");

    push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
    push_opcode(OP_push_string_literal, &mkSV("{\n"), num_strings++, nullptr, arg->location);
    push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);

    Type *t = arg->type;
    for (size_t i = 0; i < t->_struct.num_members; i++) {
        StructMember *member = &t->_struct.members[i];
        if (sv_compare_cstr(&member->name, "_")) continue;
        size_t offset = get_member_offset(t, i);

        push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
        push_opcode(OP_push_string_literal, &mkSV("    "), num_strings++, nullptr, arg->location);
        push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);

        push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
        push_opcode(OP_push_string_literal, &member->name, num_strings++, nullptr, arg->location);
        push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);

        push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
        push_opcode(OP_push_string_literal, &mkSV(" : "), num_strings++, nullptr, arg->location);
        push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);

        if (is_integer_kind(member->type) || is_boolean_kind(member->type)) {
            push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
            gen_address_visitor(arg, gen);
            push_opcode(OP_push_literal, nullptr, offset, nullptr, arg->location);
            push_opcode(OP_add, nullptr, 0, nullptr, arg->location);
            push_opcode(OP_load, nullptr, 0, member->type, arg->location);
            push_opcode(OP_call, &sb_puti_symbol->name, 0, sb_puti_symbol->type, arg->location);
        }
        else if (member->type == &builtin_u8_slice) {
            push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
            gen_address_visitor(arg, gen);
            push_opcode(OP_push_literal, nullptr, offset, nullptr, arg->location);
            push_opcode(OP_add, nullptr, 0, nullptr, arg->location);
            push_opcode(OP_load, nullptr, 0, member->type, arg->location);
            push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);
        }
        else NOT_IMPLEMENTED("Printing %s inside a struct or record in builder string is not implemented yet.\n",
                get_type_name_r(buf, member->type));
        
        push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
        push_opcode(OP_push_string_literal, &mkSV("\n"), num_strings++, nullptr, arg->location);
        push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);
    }

    push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, arg->location);
    push_opcode(OP_push_string_literal, &mkSV("}\n"), num_strings++, nullptr, arg->location);
    push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, arg->location);
}

static void gen_builder_string (AST_node *n, IL_gen *gen) {
    char buf[1024];
    Symbol *s_sb = n->builder_string.var_decl_sb->symbol.symbol;
    Symbol *s_arr = n->builder_string.var_decl_arr->symbol.symbol;
    Symbol *sb_init_symbol = get_symbol_by_name(&global_symbols, &mkSV("sb_init"));
    Symbol *sb_puts_symbol = get_symbol_by_name(&global_symbols, &mkSV("sb_puts"));
    Symbol *sb_puti_symbol = get_symbol_by_name(&global_symbols, &mkSV("sb_puti"));
    ASSERT(sb_puts_symbol, "sb_puts not found.\n");
    ASSERT(sb_puti_symbol, "sb_puti not found.\n");

    push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, n->location);
    push_opcode(OP_push_literal, nullptr, 1024, nullptr, n->location);
    push_opcode(OP_push_local_var_address, nullptr, s_arr->offset, nullptr, n->location);
    push_opcode(OP_call, &sb_init_symbol->name, 0, sb_init_symbol->type, n->location);

    for (AST_node *arg = n->builder_string.body; arg; arg = arg->next) {
        if (arg->type == &builtin_u8_slice) {
            push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, n->location);
            gen_value_visitor(arg, gen);
            push_opcode(OP_call, &sb_puts_symbol->name, 0, sb_puts_symbol->type, n->location);
        }
        else if (is_integer_kind(arg->type)) {
            push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, n->location);
            gen_value_visitor(arg, gen);
            push_opcode(OP_call, &sb_puti_symbol->name, 0, sb_puti_symbol->type, n->location);
        }
        else if (is_struct_kind(arg->type) || is_record_kind(arg->type)){
            gen_builder_string_put_struct(s_sb, arg, gen);
        }
        else NOT_IMPLEMENTED("Generating IL for type %s in builder string is not implemented yet.\n", get_type_name_r(buf, arg->type));
    }

    push_opcode(OP_push_local_var_address, nullptr, s_sb->offset, nullptr, n->location);
    push_opcode(OP_member_access, nullptr, 16, nullptr, n->location);
    push_opcode(OP_load, nullptr, 0, &builtin_u8_slice, n->location);
}

static void gen_value_visitor(AST_node *n, IL_gen *gen) {
    char buf[1024];
    if (!n) return;
    switch (n->kind) {
        case AST_call:
            gen_call(n, gen, true);
            break;

        case AST_number:
            push_opcode(OP_push_literal, &n->number.value, 0, nullptr, n->location);
            break;

        case AST_bool:
            push_opcode(OP_push_literal, nullptr, n->boolean.value, nullptr, n->location);
            break;

        case AST_null:
            push_opcode(OP_push_literal, nullptr, (uint64_t)nullptr, nullptr, n->location);
            break;

        case AST_array_len:
            push_opcode(OP_push_literal, nullptr, n->array_len.len, nullptr, n->location);
            break;
        
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "symbol '%.*s' was not resolved\n", SV_prnt(n->symbol.name));
            il_gen_push_symbol(s, n->location);
            break;
        }

        case AST_binary:
            gen_binary_operators(n, gen, true);
            break;

        case AST_variadic_operator:
            gen_variadic_operators(n, gen, true);
            break;

        case AST_if:
            gen_if(n, gen, true);
            break;

        case AST_for:
            gen_for(n, gen, true);
            break;

        case AST_user_cast:
        case AST_cast:
            gen_cast(n, gen, true);
            break;
        
        case AST_unary:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            if (n->unary.token_kind == TOK_exclam) push_opcode(OP_not, nullptr, 0, nullptr, n->location);
            else if (n->unary.token_kind == TOK_minus) push_opcode(OP_neg, nullptr, 0, nullptr, n->location);
            else NOT_IMPLEMENTED("Generating IL for unary operator %s is not implemented yet.\n", token_kind_printable(n->unary.token_kind));
            break;

        case AST_array_to_slice:
            push_opcode(OP_push_literal, nullptr, n->_array_to_slice.body->type->_array.n_elements, nullptr, n->location);
            gen_address_visitor(n->_array_to_slice.body, gen);
            break;
        
        case AST_string:
            push_opcode(OP_push_string_literal, &n->str.value, num_strings++, nullptr, n->location);
            break;

        case AST_char_constant:
            push_opcode(OP_push_char_literal, &n->str.value, 0, nullptr, n->location);
            break;

        case AST_dereference: {
            size_t size = get_storage_size(n->type);
            if (size > 16) il_gen_error(n->location, "Can't dereference something with storage size > 16. Have %lu.", size);
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            push_opcode(OP_load, nullptr, 0, n->type, n->location);
            break;
        }

        case AST_reference:
            ast_visit_children(n, (AstVisitor)gen_address_visitor, gen);
            break;

        case AST_array_access:
            if (is_array_kind(n->_array.array->type)) {
                gen_address_visitor(n->_array.array, gen);
                gen_value_visitor(n->_array.index, gen);
                push_opcode(OP_array_access, nullptr, 0, n->_array.array->type->_array.element_type, n->location);
            }
            else if (is_slice_kind(n->_array.array->type)) {
                gen_address_visitor(n->_array.array, gen);
                push_opcode(OP_load, nullptr, 0, get_ref_type_for(&builtin_any), n->location); // load the 'begin' member
                gen_value_visitor(n->_array.index, gen);
                push_opcode(OP_array_access, nullptr, 0, get_slice_element_type(n->_array.array->type), n->location);
            }
            else NOT_IMPLEMENTED("AST_array_access is not implemented for anything that is not an array or a slice.\n")

            break;
        
        case AST_plus_plus:
            gen_plus_plus(n, gen, true);
            break;

        case AST_minus_minus:
            gen_address_visitor(n->minus_minus.body, gen);
            push_opcode(OP_integer_minus_minus, nullptr, n->minus_minus.postfix + 1, n->type, n->location);
            break;


        case AST_member_access: {
            Type *t = n->member_access.body->type;
            if (is_reference_kind(t) && (is_structlike_kind(dereferenced_type(t))
                                      || is_union_kind(dereferenced_type(t)))) {
                ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
                push_opcode(OP_member_access, nullptr, n->member_access.offset, t, n->location);
            }
            else if (is_enum_kind(t)) {
                ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
                push_opcode(OP_get_enum_member_name, nullptr, 0, t, n->location);
            }
            else if (is_enumerator_kind(t)) {
                ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
                push_opcode(OP_get_enum_member_name, nullptr, 0, t, n->location);
            }
            else NOT_IMPLEMENTED("Member acces for type %s is not implemented yet.\n",
                get_type_name_r(buf, t));
            
            break;
        }
        case AST_namespace_access:
            if (is_enum_kind(n->type)) {
                push_opcode(OP_push_literal, nullptr, n->namespace_access.enum_value, nullptr, n->location);
            }
            else if (is_enumerator_kind(n->type)) {
                push_opcode(OP_push_literal, nullptr, n->namespace_access.enum_value, nullptr, n->location);
            }
            else NOT_IMPLEMENTED("Generating IL for namespace acces for type %s is not implemented yet\n", get_type_name_r(buf, n->type))
            break;

        case AST_builder_string:
            gen_builder_string(n, gen);
            break;
        
        case AST_typename:
        case AST_type_ref:
            // Nothing to be done
            break;

        default:
            NOT_IMPLEMENTED("gen_value_visitor for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}


static void il_gen_visitor(AST_node *n, IL_gen *gen) {
    if (!n) return;

    switch (n->kind) {
        case AST_function: {
            Symbol *s_fun = n->fun.symbol;
            ASSERT(s_fun, "IL gen tried to generate function '%.*s' with null symbol\n", SV_prnt(n->fun.name));
            push_opcode(OP_begin_fn, &n->fun.name, s_fun->size, s_fun->type, n->location);
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        }

        case AST_return:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            Type *ret_type = n->ret.body ? n->ret.body->type : &builtin_void;
            push_opcode(OP_return, nullptr, 0, ret_type, n->location);
            break;
        
        case AST_if:
            gen_if(n, gen, false);
            break;

        case AST_while: {
            int while_num = num_whiles++;
            push_opcode(OP_while_loop, 0, while_num, nullptr, n->location);
            gen_value_visitor(n->_while.condition, gen);
            push_opcode(OP_while_check, 0, while_num, nullptr, n->location);
            il_gen_visitor(n->_while.body, gen);
            push_opcode(OP_while_end, 0, while_num, nullptr, n->location);
            break;
        }

        case AST_for:
            gen_for(n, gen, false);
            break;

        case AST_arg_decl:
            // Nothing to do here for an arg decl, as it can't have an initializer
            break;

        case AST_var_decl:
            if (n->var_decl.initializer) {
                il_gen_push_symbol_address(n->var_decl.symbol, n->location);                
                gen_value_visitor(n->var_decl.initializer, gen);
                push_opcode(OP_store, nullptr, 0, n->var_decl.symbol->type, n->location);
            }
            break;

        case AST_call:
            gen_call(n, gen, false);
            break;
        
        case AST_binary:
            gen_binary_operators(n, gen, false);
            break;
        
        case AST_plus_plus:
            gen_plus_plus(n, gen, false);
            break;

        case AST_minus_minus:
            gen_address_visitor(n->minus_minus.body, gen);
            push_opcode(OP_integer_minus_minus, nullptr, 0, n->type, n->location);
            break;

        case AST_dereference:
        case AST_member_access:
        case AST_reference:
        case AST_symbol:
            il_gen_warning(n->location, "unused code.");
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        
        case AST_generic:
            // Don't generate code for generic functions.
            // Their implementations for specific types live outside of generic blocks.
            break;


        case AST_cast:
        case AST_number:
        case AST_arg_list:
        case AST_program:
        case AST_scope:
        case AST_struct:
        case AST_member_def:
        case AST_typename:
        case AST_type_ref:
        case AST_type_array:
        case AST_type_slice:
        case AST_enum:
        case AST_enum_member:
        case AST_union:
        case AST_union_member_def:
        case AST_generic_implementation:
        case AST_type_generic_speciation:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        default:
            NOT_IMPLEMENTED("%s:%d il_gen_visitor for %s is not implemented yet.\n", n->location->filename, n->location->line, ast_kind_name(n->kind));
            break;
    }
}

void ast_to_il(AST_node *root) {
    IL_gen gen;
    il_gen_visitor(root, &gen);
}

void ast_to_il_init() {
    dyn_array_init(&opcodes_dyn, sizeof(Opcode), 32);
}
