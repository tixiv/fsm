
#include "operator_chaining.h"
#include "ast.h"
#include "common.h"
#include "tokenizer.h"

extern void parser_error(int line_number, const char * fmt, ...);

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

typedef struct {
    AST_node *value_to_be_compared;
} OperatorChainingData;

// the irony is not lost on me that I could use exactly this operator to write this nicer ;-)
bool is_equality_operator(TokenKind t) { return t == TOK_equal || t == TOK_or_equal_to; };
bool is_equality_chaining_operator(TokenKind t) { return t == TOK_or_equal_to; };

bool is_unequality_operator(TokenKind t) { return t == TOK_unequal || t == TOK_and_not_equal_to; };
bool is_unequality_chaining_operator(TokenKind t) { return t == TOK_and_not_equal_to; };

void operator_chaining_visitor(AST_node *n, OperatorChainingData *arg) {
    // TODO: make a real multi comparison operator, as this executes
    // the code for the middle element twice. If there is something
    // in there like a function call or the to be implemented unary ++,
    // the execution will not have the expected result.

    if (n->kind == AST_binary && n->binary.left->kind == AST_binary &&
        is_comparison_operator(n->binary.token_kind) &&
        is_comparison_operator(n->binary.left->binary.token_kind))
    {
        AST_node *comp = ast_alloc(AST_binary, n->line_number);

        // insert the additional comparison with the operator
        // from this node and the right hand side of our left hand side.
        
        comp->binary.token_kind = n->binary.token_kind;
        comp->binary.left = n->binary.left->binary.right;
        comp->binary.right = n->binary.right;

        // Change this operator to &&, linking the left and the new comparison
        n->binary.right = comp;
        n->binary.token_kind = TOK_boolean_and;
        ast_visit_children(n, (AstVisitor)operator_chaining_visitor, arg);
    }
    else if (n->kind == AST_binary && n->binary.left->kind == AST_binary &&
        is_equality_chaining_operator(n->binary.token_kind))
    {
        if (!is_equality_operator(n->binary.left->binary.token_kind))
            parser_error(n->line_number, "Operator '|==' used without corresponding '==' ");

        OperatorChainingData child_arg;
        ast_visit_children(n, (AstVisitor)operator_chaining_visitor, &child_arg);

        if (n->binary.left->binary.token_kind == TOK_equal) {
            // orginal left hand side '=='. Grab the value node.
            if (arg) arg->value_to_be_compared = n->binary.left->binary.left;
        }
        else {
            if (arg) arg->value_to_be_compared = child_arg.value_to_be_compared;
        }

        AST_node *comp = ast_alloc(AST_binary, n->line_number);

        // insert the additional equality check of the right hand side
        // of this node and the left hand side of our left hand side operator.
        
        comp->binary.token_kind = TOK_equal;
        comp->binary.left = arg->value_to_be_compared;
        comp->binary.right = n->binary.right;

        // Change this operator to ||, linking the left and the new comparison
        n->binary.right = comp;
        n->binary.token_kind = TOK_boolean_or;
    }else if (n->kind == AST_binary && n->binary.left->kind == AST_binary &&
        is_unequality_chaining_operator(n->binary.token_kind))
    {
        if (!is_unequality_operator(n->binary.left->binary.token_kind))
            parser_error(n->line_number, "Operator '&!=' used without corresponding '!=' ");

        OperatorChainingData child_arg;
        ast_visit_children(n, (AstVisitor)operator_chaining_visitor, &child_arg);

        if (n->binary.left->binary.token_kind == TOK_unequal) {
            // orginal left hand side '!='. Grab the value node.
            if (arg) arg->value_to_be_compared = n->binary.left->binary.left;
        }
        else {
            if (arg) arg->value_to_be_compared = child_arg.value_to_be_compared;
        }

        AST_node *comp = ast_alloc(AST_binary, n->line_number);

        // insert the additional unequality check of the right hand side
        // of this node and the left hand side of our left hand side operator.
        
        comp->binary.token_kind = TOK_unequal;
        comp->binary.left = arg->value_to_be_compared;
        comp->binary.right = n->binary.right;

        // Change this operator to &&, linking the left and the new comparison
        n->binary.right = comp;
        n->binary.token_kind = TOK_boolean_and;
    }
    else {
        ast_visit_children(n, (AstVisitor)operator_chaining_visitor, arg);
    }    
}


void chain_operators(AST_node * root) {
    OperatorChainingData d;
    operator_chaining_visitor(root, &d);
}
