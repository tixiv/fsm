
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
    if (TOK_boolean_and == n->binary.token_kind) {
        // short circuit logic:
        // bar = a && b; -> bar = if(a) (bool)b else 0
        int if_num = num_ifs++;
        il_gen_visitor(n->binary.left, gen);

        push_opcode(OP_if, nullptr, 0x100000000 | if_num);
        
        il_gen_visitor(n->binary.right, gen);
        //if (n->result_used)
        //    push_opcode(OP_to_bool, nullptr, 0);

        push_opcode(OP_else, nullptr, if_num);
        if (n->result_used)
            push_opcode(OP_push_literal, &mkSV("0"), 0);

        push_opcode(OP_end_if, nullptr, if_num);
    }
    else if (TOK_boolean_or == n->binary.token_kind) {
        // short circuit logic:
        // bar = a || b; -> bar = if(a) 1 else (bool)b
        int if_num = num_ifs++;
        il_gen_visitor(n->binary.left, gen);

        push_opcode(OP_if, nullptr, 0x100000000 | if_num);
        if (n->result_used)
            push_opcode(OP_push_literal, &mkSV("1"), 0);
        push_opcode(OP_else, nullptr, if_num);

        il_gen_visitor(n->binary.right, gen);
        //if (n->result_used)
        //    push_opcode(OP_to_bool, nullptr, 0);

        push_opcode(OP_end_if, nullptr, if_num);
    }
    else if (TOK_equal_assign == n->binary.token_kind) {
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

        if (n->result_used) {
            switch (n->binary.token_kind) {
                case TOK_plus: push_opcode(OP_add, nullptr, 0); break;
                case TOK_minus: push_opcode(OP_sub, nullptr, 0); break;
                case TOK_asterisk: push_opcode(OP_mul, nullptr, 0); break;
                case TOK_slash: push_opcode(OP_div, nullptr, 0); break;
                case TOK_percent: push_opcode(OP_mod, nullptr, 0); break;
                case TOK_equal: push_opcode(OP_equal, nullptr, 0); break;
                case TOK_unequal: push_opcode(OP_unequal, nullptr, 0); break;
                case TOK_greater: push_opcode(OP_compare_GT, nullptr, 0); break;
                case TOK_lower: push_opcode(OP_compare_LT, nullptr, 0); break;
                case TOK_greater_equal: push_opcode(OP_compare_GE, nullptr, 0); break;
                case TOK_lower_equal: push_opcode(OP_compare_LE, nullptr, 0); break;

                default:
                    NOT_IMPLEMENTED("Generating IL for binary operator %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
            }
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
    else {
        char buf_1[1024], buf_2[1024];
        NOT_IMPLEMENTED("Generating IL for cast to '%s' from '%s' is not implemented yet.\n",
            get_type_name_r(buf_1, to), get_type_name_r(buf_2, from));
    }
}

static void il_gen_visitor(AST_node *n, IL_gen *gen) {
    if (!n) return;
    switch (n->kind) {
        case AST_function: {
            Symbol *s_fun = n->fun.symbol;
            ASSERT(s_fun, "IL gen tried to generate function '%.*s' with null symbol\n", SV_prnt(n->fun.name));
            push_opcode(OP_begin_fn, &n->fun.name, s_fun->size);
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;
        }

        case AST_return:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_return, nullptr, n->ret.return_val ? 1:0);
            break;

        case AST_call: {
            Symbol *s_call = n->call.symbol;
            ASSERT(s_call, "Symbol for called function '%.*s' is not resolved\n", SV_prnt(n->call.name))
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_call, &n->call.name, s_call->type->fun.num_arguments);

            // printf("IL Gen AST_call: num_fn_returns = %d, result_used = %d, name = '%.*s'\n", s_call->num_fn_returns, n->result_used, SV_prnt(n->call.name));

            if (s_call->type->fun.return_type != &builtin_void && n->result_used)
                push_opcode(OP_push_result, nullptr, 0);
            break;
        }

        case AST_number:
            if (n->result_used)
                push_opcode(OP_push_literal, &n->number.value, 0);
            break;
        
        case AST_symbol: {
            Symbol *s = n->symbol.symbol;
            ASSERT(s, "symbol '%.*s' was not resolved\n", SV_prnt(n->symbol.name));
            if (s->kind == SYM_arg) {
                if (n->result_used)
                    push_opcode(OP_push_arg, nullptr, s->offset);
            } else if (s->kind == SYM_local) {
                if (n->result_used)
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

        case AST_for: {
            int while_num = num_whiles++;
            il_gen_visitor(n->_for.initializer, gen);
            push_opcode(OP_while_loop, 0, while_num);
            if (n->_for.condition) {
                il_gen_visitor(n->_for.condition, gen);
                push_opcode(OP_while_check, 0, while_num);
            }
            il_gen_visitor(n->_for.body, gen);
            il_gen_visitor(n->_for.post_action, gen);
            push_opcode(OP_while_end, 0, while_num);
            break;
        }

        case AST_var_decl:
            if (n->var_decl.initializer) {
                il_gen_visitor(n->var_decl.initializer, gen);
                il_gen_assign_var(n->var_decl.symbol);
            }
            break;

        case AST_arg_list:
        case AST_program:
        case AST_scope:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            break;

        case AST_arg_decl:
            // Nothing to do here for an arg decl, as it can't have an initializer
            break;
        
        case AST_cast:
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            gen_cast(n);
            break;
        
        case AST_string:
            push_opcode(OP_push_string_literal, &n->str.value, num_strings++);
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
