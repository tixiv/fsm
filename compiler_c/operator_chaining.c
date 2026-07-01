
#include "operator_chaining.h"
#include "common.h"
#include "tokenizer.h"


bool is_comparison_operator(TokenKind t) {
    switch(t) {
        case TOK_greater:
        case TOK_lower:
        case TOK_greater_equal:
        case TOK_lower_equal:
            return true;
        default:
            return false;
    }
}

void operator_chaining_visitor(AST_node *n, void *arg) {
    if (n->kind == AST_binary && n->binary.left->kind == AST_binary &&
        is_comparison_operator(n->binary.token_kind) &&
        is_comparison_operator(n->binary.left->binary.token_kind))
    {
        AST_node *comp = ast_alloc(AST_binary, n->line_number);

        // insert the additional comparison with the operator
        // from this node and the right hand side of our left hand side.
        // TODO: make a real multi comparison operator, as this executes
        // the code for the middle element twice. If there is something
        // in there like a function call or the to be implemented unary ++,
        // the execution will not have the expected result.
        comp->binary.token_kind = n->binary.token_kind;
        comp->binary.left = n->binary.left->binary.right;
        comp->binary.right = n->binary.right;

        // Change this operator to &&, linking the left and the new comparison
        n->binary.right = comp;
        n->binary.token_kind = TOK_boolean_and;
    }
    ast_visit_children(n, operator_chaining_visitor, nullptr);
}


void chain_operators(AST_node * root) {
    operator_chaining_visitor(root, nullptr);
}
