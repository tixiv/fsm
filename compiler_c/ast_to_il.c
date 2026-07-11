
#include "ast_to_il.h"
#include "ast.h"
#include "common.h"
#include "opcodes.h"
#include "sv.h"
#include "tokenizer.h"
#include "type.h"
#include <stdarg.h>

static void il_gen_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM IL Gen] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static int num_ifs;
static int num_whiles;
static int num_strings;

typedef struct {
} IL_gen;

static void il_gen_push_symbol_address(Symbol *s) {
    if (s->kind == SYM_local) {
        push_opcode(OP_push_local_var_address, nullptr, s->offset);
    } else if (s->kind == SYM_arg) {
        push_opcode(OP_push_arg_address, nullptr, s->offset);
    } else {
        NOT_IMPLEMENTED("Pushing address for symbol kind %s is not implemented yet.", symbol_kind_name(s->kind));
    }
}

static void il_gen_push_symbol(Symbol *s) {
    if (s->kind == SYM_arg) {
        push_opcode(OP_push_arg, nullptr, s->offset);
    } else if (s->kind == SYM_local) {
        push_opcode(OP_push_local_var, nullptr, s->offset);
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

        push_opcode(OP_if, nullptr, 0x100000000 | if_num);
        
        if (result_used) gen_value_visitor(n->binary.right, gen); else il_gen_visitor(n->binary.right, gen);

        push_opcode(OP_else, nullptr, if_num);
        
        if (result_used)
            push_opcode(OP_push_literal, &mkSV("0"), 0);

        push_opcode(OP_end_if, nullptr, if_num);
    }
    else if (TOK_boolean_or == n->binary.token_kind) {
        // short circuit logic:
        // bar = a || b; -> bar = if(a) 1 else (bool)b
        int if_num = num_ifs++;
        gen_value_visitor(n->binary.left, gen);

        push_opcode(OP_if, nullptr, 0x100000000 | if_num);
        
        if (result_used)
            push_opcode(OP_push_literal, &mkSV("1"), 0);
        
        push_opcode(OP_else, nullptr, if_num);

        if (result_used) gen_value_visitor(n->binary.right, gen); else il_gen_visitor(n->binary.right, gen);
        
        push_opcode(OP_end_if, nullptr, if_num);
    }
    else if (TOK_equal_assign == n->binary.token_kind || TOK_bind_ref == n->binary.token_kind) {
        if (!n->binary.left->addressable) {
            il_gen_error(n->line_number,"Trying to assign to something that is not addressable. Have %s.\n",
                    ast_kind_name(n->binary.left->kind));
        }
        gen_address_visitor(n->binary.left, gen);
        gen_value_visitor(n->binary.right, gen);

        if (result_used)
            push_opcode(OP_store_and_dup, nullptr, get_storage_size(n->binary.left->type));
        else
            push_opcode(OP_store, nullptr, get_storage_size(n->binary.left->type));
    }
    else {
        if (result_used)
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
        else
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);;

        switch (n->binary.token_kind) {
            case TOK_plus:          if (result_used) push_opcode(OP_add, nullptr, 0); break;
            case TOK_minus:         if (result_used) push_opcode(OP_sub, nullptr, 0); break;
            case TOK_asterisk:      if (result_used) push_opcode(OP_mul, nullptr, 0); break;
            case TOK_slash:         if (result_used) push_opcode(OP_div, nullptr, 0); break;
            case TOK_percent:       if (result_used) push_opcode(OP_mod, nullptr, 0); break;
            case TOK_equal:         if (result_used) push_opcode(OP_equal, nullptr, 0); break;
            case TOK_unequal:       if (result_used) push_opcode(OP_unequal, nullptr, 0); break;
            case TOK_greater:       if (result_used) push_opcode(OP_compare_GT, nullptr, 0); break;
            case TOK_lower:         if (result_used) push_opcode(OP_compare_LT, nullptr, 0); break;
            case TOK_greater_equal: if (result_used) push_opcode(OP_compare_GE, nullptr, 0); break;
            case TOK_lower_equal:   if (result_used) push_opcode(OP_compare_LE, nullptr, 0); break;

            default:
                NOT_IMPLEMENTED("Generating IL for binary operator %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
        }
    }
}

static void gen_cast(AST_node *n) {
    ASSERT(n->kind == AST_cast, "gen_cast() called on wrong kind of AST node\n");

    Type *to = n->type;
    Type *from = n->_cast.right_type;
    
    if (is_boolean_kind(to) && is_integer_kind(from)) {
        if (n->result_used)
            push_opcode(OP_to_bool, nullptr, 0);
    }
    else if (is_integer_kind(to) && is_boolean_kind(from)) {
        // No cast needed. our 64 bit bools can be used as integer directly.
    }
    else if (is_integer_kind(to) && from->kind == T_unsigned_integer &&
             to->integer.num_bits >= from->integer.num_bits)
    {
        // No cast needed.
    }
    else if (to->kind == T_signed_integer && from->kind == T_signed_integer) {
        NOT_IMPLEMENTED("Generating cast for sign extension is not implemented yet.\n")
    }
    else if (is_integer_kind(to) && from->kind == T_signed_integer) {
        // Just put no cast for now, let's fix potential problems later
    }
    else {
        char buf_1[1024], buf_2[1024];
        NOT_IMPLEMENTED("Generating IL for cast to '%s' from '%s' is not implemented yet.\n",
            get_type_name_r(buf_1, to), get_type_name_r(buf_2, from));
    }
}

static void gen_call(AST_node *n, IL_gen *gen, bool result_used) {
    Symbol *s_call = n->call.symbol;
    ASSERT(s_call, "Symbol for called function '%.*s' is not resolved\n", SV_prnt(n->call.name))
    ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
    push_opcode(OP_call, &n->call.name, s_call->type->fun.num_arguments * 8);

    if (result_used)
        push_opcode(OP_push_result, nullptr, 0);
}

static void gen_if(AST_node *n, IL_gen *gen, bool result_used) {
    int if_num = num_ifs++;
    gen_value_visitor(n->_if.condition, gen);
    push_opcode(OP_if, nullptr, (n->_if.else_clause ? 0x100000000 : 0) | if_num);
    if (result_used) gen_value_visitor(n->_if.if_clause, gen); else il_gen_visitor(n->_if.if_clause, gen);
    if (n->_if.else_clause) {
        push_opcode(OP_else, nullptr, if_num);
        if (result_used) gen_value_visitor(n->_if.else_clause, gen); else il_gen_visitor(n->_if.else_clause, gen);
    }
    push_opcode(OP_end_if, nullptr, if_num);
}

static void gen_address_visitor(AST_node *n, IL_gen *gen) {

    // char buf[1024];

    switch (n->kind) {
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "Symbol for variable assignment to '%.*s' is not resolved\n", SV_prnt(n->symbol.name));

            il_gen_push_symbol_address(s);
            break;
        }

        case AST_dereference:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            break;

        default:
            NOT_IMPLEMENTED("il_gen_address_visitor for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}



static void gen_value_visitor(AST_node *n, IL_gen *gen) {
    if (!n) return;
    switch (n->kind) {
        case AST_call:
            gen_call(n, gen, true);
            break;

        case AST_number:
            push_opcode(OP_push_literal, &n->number.value, 0);
            break;
        
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "symbol '%.*s' was not resolved\n", SV_prnt(n->symbol.name));
            il_gen_push_symbol(s);
            break;
        }

        case AST_binary:
            gen_binary_operators(n, gen, true);
            break;

        case AST_if:
            gen_if(n, gen, true);
            break;
        
        case AST_cast:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            gen_cast(n);
            break;
        
        case AST_string:
            push_opcode(OP_push_string_literal, &n->str.value, num_strings++);
            break;

        case AST_dereference:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            push_opcode(OP_load, nullptr, get_storage_size(n->type));
            break;

        case AST_reference:
            ast_visit_children(n, (AstVisitor)gen_address_visitor, gen);
            break;

        case AST_array_access:
            ast_visit_children(n, (AstVisitor)gen_address_visitor, gen);
            push_opcode(OP_array_access, nullptr, get_storage_size(n->_array.array->type->array.element_type));
            break;


        case AST_member_access:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            push_opcode(OP_member_access, nullptr, n->member_access.offset);
            break;
        
        
        default:
            NOT_IMPLEMENTED("gen_value_visitor for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}


static void il_gen_visitor(AST_node *n, IL_gen *gen) {
    switch (n->kind) {
        case AST_function: {
            Symbol *s_fun = n->fun.symbol;
            ASSERT(s_fun, "IL gen tried to generate function '%.*s' with null symbol\n", SV_prnt(n->fun.name));
            push_opcode(OP_begin_fn, &n->fun.name, s_fun->size);
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        }

        case AST_return:
            ast_visit_children(n, (AstVisitor)gen_value_visitor, gen);
            push_opcode(OP_return, nullptr, n->ret.return_val ? 1:0);
            break;
        
        case AST_if:
            gen_if(n, gen, false);
            break;

        case AST_while: {
            int while_num = num_whiles++;
            push_opcode(OP_while_loop, 0, while_num);
            gen_value_visitor(n->_while.condition, gen);
            push_opcode(OP_while_check, 0, while_num);
            il_gen_visitor(n->_while.body, gen);
            push_opcode(OP_while_end, 0, while_num);
            break;
        }

        case AST_for: {
            int while_num = num_whiles++;
            il_gen_visitor(n->_for.initializer, gen);
            push_opcode(OP_while_loop, 0, while_num);
            if (n->_for.condition) {
                gen_value_visitor(n->_for.condition, gen);
                push_opcode(OP_while_check, 0, while_num);
            }
            il_gen_visitor(n->_for.body, gen);
            il_gen_visitor(n->_for.post_action, gen);
            push_opcode(OP_while_end, 0, while_num);
            break;
        }

        case AST_arg_decl:
            // Nothing to do here for an arg decl, as it can't have an initializer
            break;

        case AST_var_decl:
            if (n->var_decl.initializer) {
                il_gen_push_symbol_address(n->var_decl.symbol);                
                gen_value_visitor(n->var_decl.initializer, gen);
                push_opcode(OP_store, nullptr, get_storage_size(n->var_decl.symbol->type));
            }
            break;

        case AST_call:
            gen_call(n, gen, false);
            break;
        
        case AST_binary:
            gen_binary_operators(n, gen, false);
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
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        default:
            NOT_IMPLEMENTED("il_gen_visitor for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}



void ast_to_il(AST_node *root) {
    dyn_array_init(&opcodes_dyn, sizeof(Opcode), 32);

    IL_gen gen;
    il_gen_visitor(root, &gen);
}
