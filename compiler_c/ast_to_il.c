
#include "ast_to_il.h"
#include "ast.h"
#include "common.h"
#include "opcodes.h"
#include "tokenizer.h"

typedef struct {
    int arg_count;
} IL_gen;

static void il_gen_visitor(AST_node *n, IL_gen *gen);

static void gen_binary_operators(AST_node *n, IL_gen *gen) {
    ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);

    switch (n->binary.token_kind) {
        case TOK_minus: push_opcode(OP_sub, nullptr, 0); break;

        default:
            NOT_IMPLEMENTED("Generating IL for binary operator %s is not implemented yet.\n", token_kind_name(n->binary.token_kind));
    }
}

static void il_gen_visitor(AST_node *n, IL_gen *gen) {
    switch (n->kind) {
        case AST_function:
            push_opcode(OP_begin_fn, &n->fun.name, 0);
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_return, nullptr, 1);
            break;

        case AST_call:
            gen->arg_count = 0;
            ast_visit_children(n, (AstVisitor)il_gen_visitor, gen);
            push_opcode(OP_call, &n->call.name, gen->arg_count);
            break;

        case AST_number:
            push_opcode(OP_number, &n->number.value, 0);
            break;

        case AST_binary:
            gen_binary_operators(n, gen);
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
