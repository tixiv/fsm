
#include "common.h"
#include "sv.h"
#include "ast.h"
#include "tokenizer.h"
#include "dyn_array.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static const bool enable_debug_parser = false;

static void debug_log_parser(const char * fmt, ...) {
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

static AST_node *parse_expression();

static AST_node *parse_if() {
    return nullptr;    
}

static AST_node *parse_while() {
    return nullptr;    
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
            n = ast_alloc(AST_var, CT->line_number);
            n->var.name = *name;
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
        MOVE_NEXT();
        n = parse_if();
    }
    else if (CT->kind == TOK_keyword_while) {
        MOVE_NEXT();
        n = parse_while();
    }
    else {

        parser_error(CT->line_number, "Expected expression but got %s",
                token_kind_name(CT->kind));
    }

    debug_log_parser("Leaving %s\n", __func__);
    return n;
}

static AST_node *parse_expression() {
    return parse_primary();
}

static AST_node *parse_scope_body()
{
    AST_node *ast_begin = nullptr;
    AST_node *ast_last = nullptr;
    
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
        else if (CT->kind == TOK_keyword_return) {
            AST_node *ast_return = ast_alloc(AST_return, CT->line_number);
            MOVE_NEXT();

            if (CT->kind != TOK_semicolon)
            {
                ast_return->ret.return_val = parse_expression();
            }

             if (!ast_last) {
                ast_begin = ast_return;
                ast_last = ast_return;
            } else {
                ast_last->next = ast_return;
                ast_last = ast_return;
            }
        }
        else {
            AST_node * ast_expr = parse_expression(false);
            assert(ast_expr);

            if (!ast_last) {
                ast_begin = ast_expr;
                ast_last = ast_expr;
            } else {
                ast_last->next = ast_expr;
                ast_last = ast_expr;
            }

            if (CT->kind != TOK_semicolon) {
                if (false) parser_error(CT->line_number, "missing ';'");
            }
        }
    }

    return ast_begin;
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
        AST_node *latest_arg = nullptr;

        while (1) {
            if (CT->kind == TOK_rparen) {
                MOVE_NEXT();
                break;
            }
            else if(CT->kind == TOK_identifier) {
                AST_node *ast_arg = ast_alloc(AST_var_decl, CT->line_number);
                ast_arg->var.name = CT->value;
                
                if (latest_arg) {
                    latest_arg->next = ast_arg;
                } else {
                    ast_fn->fun.args = ast_arg;
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

    if (CT->kind != TOK_lbrace) {
        parser_error(CT->line_number, "Expected '{' but got %s",
            token_kind_name(CT->kind));
    }
    MOVE_NEXT();

    ast_fn->fun.body = parse_scope_body();

    debug_log_parser("Leaving %s\n", __func__);
    return ast_fn;
}

AST_node *parse_program_ast() {
    debug_log_parser("Entering %s\n", __func__);

    AST_node *root = nullptr;
    AST_node *last = nullptr;

    current_token = tokens;
    while (1) {
        if (CT->kind == TOK_keyword_fn) {
            AST_node *fun = parse_function();
            if (last) {
                last->next = fun;
            } else {
                root = fun;
                last = fun;
            }
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
