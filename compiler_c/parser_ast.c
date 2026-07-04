
#include "common.h"
#include "sv.h"
#include "ast.h"
#include "tokenizer.h"
#include "dyn_array.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static const bool enable_debug_parser = false;

void debug_log_parser(const char * fmt, ...) {
    if (!enable_debug_parser)
        return;

    va_list args;
    va_start(args, fmt);

    printf("[FSM debug parser] ");
    vprintf(fmt, args);

    va_end(args);
}

static void parser_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Parser] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static Token *current_token;

#define CT current_token
#define MOVE_NEXT() (current_token++)

static AST_node *parse_statement();
static AST_node *parse_expression();
static AST_node *parse_scope_body();

static AST_node *parse_if() {
    debug_log_parser("Entering %s\n", __func__);

    AST_node *n = ast_alloc(AST_if, CT->line_number);
    MOVE_NEXT();

    if (CT->kind != TOK_lparen) {
        parser_error(CT->line_number, "Expected '(' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();

    n->_if.condition = parse_expression();

    if (CT->kind != TOK_rparen) {
        parser_error(CT->line_number, "Expected ')' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();

    n->_if.if_clause = parse_statement();

    if (CT->kind == TOK_keyword_else) {
        MOVE_NEXT();

        n->_if.else_clause = parse_statement();
    }

    debug_log_parser("Leaving %s\n", __func__);
    return n;
}

static AST_node *parse_while() {
    debug_log_parser("Entering %s\n", __func__);
    AST_node *n = ast_alloc(AST_while, CT->line_number);
    MOVE_NEXT();

    if (CT->kind != TOK_lparen) {
        parser_error(CT->line_number, "Expected '(' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();

    n->_while.condition = parse_expression();

    if (CT->kind != TOK_rparen) {
        parser_error(CT->line_number, "Expected ')' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();

    n->_while.body = parse_statement();

    debug_log_parser("Leaving %s\n", __func__);

    return n;
}

static void expect_token(TokenKind tok) {
    if (CT->kind != tok) {
        parser_error(CT->line_number, "Expected %s but got %s",
            token_kind_printable(tok), token_kind_printable(CT->kind));
    }
}

static void take_expected(TokenKind tok) {
    expect_token(tok);
    MOVE_NEXT();
}

static AST_node *parse_for() {
    debug_log_parser("Entering %s\n", __func__);
    AST_node *n = ast_alloc(AST_for, CT->line_number);
    MOVE_NEXT();

    take_expected(TOK_lparen);
    if (CT->kind != TOK_semicolon) n->_for.initializer = parse_statement();
    take_expected(TOK_semicolon);
    if (CT->kind != TOK_semicolon) n->_for.condition = parse_expression();
    take_expected(TOK_semicolon);
    if (CT->kind != TOK_rparen) n->_for.post_action = parse_statement();
    take_expected(TOK_rparen);
    n->_for.body = parse_statement();

    debug_log_parser("Leaving %s\n", __func__);

    return n;
}

static AST_node *parse_call_arguments() {
    debug_log_parser("Entering %s\n", __func__);

    MOVE_NEXT();

    AST_node *first = nullptr;
    AST_node *last = nullptr;

    while (1) {
        if (CT->kind == TOK_rparen) {
            MOVE_NEXT();
            break;
        } else {
            last = first = parse_expression();

            while (CT->kind == TOK_komma) {
                MOVE_NEXT();
                AST_node *n = parse_expression(true); 
                last->next = n;
                last = n;
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);

    return first;
}

static AST_node *parse_primary()
{
    debug_log_parser("Entering %s\n", __func__);

    AST_node *n = nullptr;

    if (CT->kind == TOK_number) {
        n = ast_alloc(AST_number, CT->line_number);
        n->number.value = CT->value;
        MOVE_NEXT();
    }
    else if (CT->kind == TOK_string) {
        n = ast_alloc(AST_string, CT->line_number);
        n->str.value = CT->value;
        MOVE_NEXT();
    }
    else if (CT->kind == TOK_identifier) {
        SV* name = &CT->value;
        MOVE_NEXT();

        if (CT->kind == TOK_lparen) {
            n = ast_alloc(AST_call, CT->line_number);
            n->call.name = *name;
            n->call.args = parse_call_arguments(name);
        } else {
            n = ast_alloc(AST_symbol, CT->line_number);
            n->symbol.name = *name;
        }
    }
    else if (CT->kind == TOK_lparen) {
        MOVE_NEXT();
        n = parse_expression();

        if (CT->kind != TOK_rparen)
        {
            parser_error(CT->line_number, "Expected ')' but got %s",
                token_kind_name(CT->kind));
        }
        MOVE_NEXT();
    }
    else if (CT->kind == TOK_keyword_if) {
        n = parse_if();
    }
    else if (CT->kind == TOK_keyword_while) {
        n = parse_while();
    }
    else if (CT->kind == TOK_keyword_for) {
        n = parse_for();
    }
    else {

        parser_error(CT->line_number, "Expected expression but got %s",
                token_kind_name(CT->kind));
    }

    debug_log_parser("Leaving %s\n", __func__);
    return n;
}

#define NUM_PRIOS 7
#define NUM_MAX_OPERATORS_IN_PRIO 4

const uint8_t operator_table[NUM_PRIOS][NUM_MAX_OPERATORS_IN_PRIO] =  {
    { TOK_equal_assign, 0, 0, 0},
    { TOK_boolean_or, 0, 0, 0},
    { TOK_boolean_and, 0, 0, 0},
    { TOK_greater, TOK_lower, TOK_greater_equal, TOK_lower_equal},
    { TOK_equal, TOK_unequal, 0, 0},
    { TOK_plus, TOK_minus, 0, 0},
    { TOK_asterisk, TOK_slash, 0, 0},
};

static bool is_in_prio(int op, int prio) {
    for (int i = 0; i < NUM_MAX_OPERATORS_IN_PRIO; i++) {
        if (operator_table[prio][i] == op) return true;
    }
    return false;
}

static AST_node *parse_binary_operators(int prio) {
    if (prio == NUM_PRIOS) return parse_primary();
    
    AST_node *left = parse_binary_operators(prio + 1);

    while (is_in_prio(CT->kind, prio)) {
        AST_node *n = ast_alloc(AST_binary, CT->line_number);
        n->binary.left = left;
        n->binary.token_kind = CT->kind;
        
        MOVE_NEXT();

        n->binary.right = parse_binary_operators(prio + 1);

        left = n;
    }

    return left;
}

static AST_node *parse_operators() {
    return parse_binary_operators(0);
}

static AST_node *parse_expression() {
    return parse_operators();
}

static AST_node *parse_statement()
{
    debug_log_parser("Entering %s\n", __func__);
    
    AST_node *n = nullptr;

    if (CT->kind == TOK_keyword_return) {
        n = ast_alloc(AST_return, CT->line_number);
        MOVE_NEXT();

        if (CT->kind != TOK_semicolon)
        {
            n->ret.return_val = parse_expression();
        }
    }
    else if (CT->kind == TOK_keyword_let) {
        n = ast_alloc(AST_var_decl, CT->line_number);
        MOVE_NEXT();
        
        if (CT->kind != TOK_identifier) {
            parser_error(CT->line_number, "Expected identifier, but got %s",
                token_kind_name(CT->kind));
        }
        n->var_decl.name = CT->value;
        MOVE_NEXT();

        if (CT->kind == TOK_equal_assign) {
            MOVE_NEXT();
            n->var_decl.initializer = parse_expression();
        }
        else if (CT->kind == TOK_semicolon) {
            // just declaring the variable without assigning it
        } else {
            parser_error(CT->line_number, "Expected '=' or ';', but got %s",
                token_kind_name(CT->kind));
        }
    }
    else if (CT->kind == TOK_lbrace) {
        n = parse_scope_body();
    }
    else {
        return parse_expression();
    }

    debug_log_parser("Leaving %s\n", __func__);
    return n;
}

static AST_node *parse_scope_body()
{
    AST_node *ast_scope = ast_alloc(AST_scope, CT->line_number);
    AST_node *ast_last = nullptr;

    take_expected(TOK_lbrace);

    while(1) {
        if (CT->kind == TOK_eof) {
            parser_error(CT->line_number, "encountered EOF while parsing block");
        }
        else if (CT->kind == TOK_semicolon) {
            MOVE_NEXT();
        }
        else if (CT->kind == TOK_rbrace) {
            MOVE_NEXT();
            break;
        }
        else {
            AST_node * n = parse_statement();
            assert(n);

            if (!ast_last) {
                ast_scope->scope.body = n;
                ast_last = n;
            } else {
                ast_last->next = n;
                ast_last = n;
            }

            if (CT->kind != TOK_semicolon) {
                if (false) parser_error(CT->line_number, "missing ';'");
            }
        }
    }

    return ast_scope;
}

static void insert_implicit_return(AST_node *scope) {
    ASSERT(scope->kind == AST_scope, "Tried to insert return in something that is not a scope\n");
    AST_node *last = get_last_in_chain(scope->scope.body);
    if (last->kind != AST_return) {
        AST_node *ast_return = ast_alloc(AST_return, last->line_number);
        ast_return->ret.implicit = true;
        last->next = ast_return;
    }
}

static AST_node *parse_function() {
    debug_log_parser("Entering %s\n", __func__);

    assert(CT->kind == TOK_keyword_fn);
    AST_node *ast_fn = ast_alloc(AST_function, CT->line_number);
    MOVE_NEXT();

    if (CT->kind != TOK_identifier) {
        parser_error(CT->line_number, "Expected function name but got %s",
            token_kind_name(CT->kind));
    }

    ast_fn->fun.name = CT->value;
    MOVE_NEXT();

    if (CT->kind != TOK_lparen) {
        parser_error(CT->line_number, "Expected '(' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();


    // Function arguments
    {
        AST_node *ast_arglist = ast_alloc(AST_arg_list, CT->line_number);
        ast_fn->fun.args = ast_arglist;

        AST_node *latest_arg = nullptr;

        while (1) {
            if (CT->kind == TOK_rparen) {
                MOVE_NEXT();
                break;
            }
            else if(CT->kind == TOK_identifier) {
                AST_node *ast_arg = ast_alloc(AST_arg_decl, CT->line_number);
                ast_arg->var_decl.name = CT->value;
                
                if (latest_arg) {
                    latest_arg->next = ast_arg;
                } else {
                    ast_arglist->arg_list.body = ast_arg;
                }
                latest_arg = ast_arg;

                MOVE_NEXT();

                if(CT->kind == TOK_komma) {
                    MOVE_NEXT();
                }
            }
            else {
                parser_error(CT->line_number, "Expected ')' or function argument but got %s",
                    token_kind_name(CT->kind));
            }
        }
    }

    expect_token(TOK_lbrace);

    ast_fn->fun.body = parse_scope_body();

    insert_implicit_return(ast_fn->fun.body);

    debug_log_parser("Leaving %s\n", __func__);
    return ast_fn;
}

AST_node *parse_program_ast() {
    debug_log_parser("Entering %s\n", __func__);

    AST_node *root = ast_alloc(AST_program, 0);
    AST_node *last = nullptr;

    current_token = tokens;
    while (1) {
        if (CT->kind == TOK_keyword_fn) {
            AST_node *fun = parse_function();
            if (last) 
                last->next = fun;
            else
                root->program.body = fun;
            last = fun;
        }
        else if (CT->kind == TOK_eof) {
            break;
        } else {
            parser_error(CT->line_number, "Expected 'fn' or EOF but got %s",
                token_kind_name(CT->kind));
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
    return root;
}
