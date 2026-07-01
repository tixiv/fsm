
#include "ast_to_il.h"
#include "ast.h"
#include "common.h"
#include "opcodes.h"
#include "sv.h"
#include "tokenizer.h"
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

typedef struct {
} IL_gen;

void il_gen_assign_var(Symbol *s) {
    ASSERT(s, "IL gen tried to assign to null symbol\n");
    if (s->kind == SYM_local) {
        push_opcode(OP_assign_local_var, nullptr, s->offset);
    } else {
        NOT_IMPLEMENTED("Generating IL for assigning to %s '%.*s' is not implemented yet.\n", symbol_kind_name(s->kind), SV_prnt(s->name));
    }
}

static void il_gen_visitor(AST_node *n, IL_gen *gen);

static void gen_binary_operators(AST_node *n, IL_gen *gen) {
    if (TOK_equal_assign == n->binary.token_kind) {
        if(n->binary.left->kind == AST_symbol) {
            Symbol *s = n->binary.left->symbol.symbol;
            ASSERT(s, "Symbol for variable assignment to '%.*s' is not resolved\n", SV_prnt(n->binary.left->symbol.name))
            
            il_gen_visitor(n->binary.right, gen);
            il_gen_assign_var(s);
        }
        else {
            il_gen_error(n->line_number,"Trying to assign to something that is not a symbol\n");
        }
    } else {
        ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);

        switch (n->binary.token_kind) {
            case TOK_plus: push_opcode(OP_add, nullptr, 0); break;
            case TOK_minus: push_opcode(OP_sub, nullptr, 0); break;
            case TOK_asterisk: push_opcode(OP_mul, nullptr, 0); break;
            case TOK_equal: push_opcode(OP_equal, nullptr, 0); break;
            case TOK_unequal: push_opcode(OP_unequal, nullptr, 0); break;
            case TOK_greater: push_opcode(OP_compare_GT, nullptr, 0); break;
            case TOK_lower: push_opcode(OP_compare_LT, nullptr, 0); break;
            case TOK_greater_equal: push_opcode(OP_compare_GE, nullptr, 0); break;
            case TOK_lower_equal: push_opcode(OP_compare_LE, nullptr, 0); break;
            case TOK_boolean_and: push_opcode(OP_mul, nullptr, 0); break;

            default:
                NOT_IMPLEMENTED("Generating IL for binary operator %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
        }
    }
}

static int num_ifs;
static int num_whiles;

static void il_gen_visitor(AST_node *n, IL_gen *gen) {
    switch (n->kind) {
        case AST_function: {
            Symbol *s_fun = n->fun.symbol;
            ASSERT(s_fun, "IL gen tried to generate function '%.*s' with null symbol\n", SV_prnt(n->fun.name));
            push_opcode(OP_begin_fn, &n->fun.name, s_fun->size);
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_return, nullptr, 1);
            break;
        }

        case AST_return:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_return, nullptr, 1);
            break;

        case AST_call: {
            Symbol *s_call = n->call.symbol;
            ASSERT(s_call, "Symbol for called function '%.*s' is not resolved\n", SV_prnt(n->call.name))
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_call, &n->call.name, s_call->num_fn_args);
            if (s_call->num_fn_returns)
                push_opcode(OP_push_result, nullptr, 0);
            break;
        }

        case AST_number:
            push_opcode(OP_number, &n->number.value, 0);
            break;
        
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "symbol '%.*s' was not resolved\n", SV_prnt(n->symbol.name));
            if (s->kind == SYM_arg) {
                push_opcode(OP_push_arg, nullptr, s->offset);
            } else if (s->kind == SYM_local) {
                push_opcode(OP_push_local_var, nullptr, s->offset);
            } else {
                NOT_IMPLEMENTED("Generating IL for AST_symbol with %s is not implemented yet.\n", symbol_kind_name(s->kind));
            }
            break;
        }

        case AST_binary:
            gen_binary_operators(n, gen);
            break;

        case AST_if: {
            int if_num = num_ifs++;
            il_gen_visitor(n->_if.condition, gen);
            push_opcode(OP_if, nullptr, (n->_if.else_clause ? 0x100000000 : 0) | if_num);
            il_gen_visitor(n->_if.if_clause, gen);
            if (n->_if.else_clause) {
                push_opcode(OP_else, nullptr, if_num);
                il_gen_visitor(n->_if.else_clause, gen);
            }
            push_opcode(OP_end_if, nullptr, if_num);
            break;
        }

        case AST_while: {
            int while_num = num_whiles++;
            push_opcode(OP_while_loop, 0, while_num);
            il_gen_visitor(n->_while.condition, gen);
            push_opcode(OP_while_check, 0, while_num);
            il_gen_visitor(n->_while.body, gen);
            push_opcode(OP_while_end, 0, while_num);
            break;
        }

        case AST_var_decl:
            if (n->var_decl.initializer) {
                il_gen_visitor(n->var_decl.initializer, gen);
                il_gen_assign_var(n->var_decl.symbol);
            }
            break;

        case AST_program:
        case AST_scope:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;

        case AST_arg_decl:
            break;

        default:
            NOT_IMPLEMENTED("Generating IL for %s is not implemented yet.\n", ast_kind_name(n->kind));
            break;
    }
}

void ast_to_il(AST_node *root) {
    dyn_array_init(&opcodes_dyn, sizeof(Opcode), 32);

    IL_gen gen;
    il_gen_visitor(root, &gen);
}
