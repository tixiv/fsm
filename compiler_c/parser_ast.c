
#include "common.h"
#include "sv.h"
#include "ast.h"
#include "tokenizer.h"
#include "dyn_array.h"
#include "modules.h"
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

void parser_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Parser] %s:%d Error: ", current_filename, line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static Token *current_token;

#define CT current_token
#define MOVE_NEXT() (current_token++)

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

static AST_node *parse_statement();
static AST_node *parse_expression();
static AST_node *parse_scope_body();


static void link_arr(AST_node *parent, AST_node *child) {
    switch (parent->kind) {
        case AST_type_array: parent->_type_array.body = child; break;
        case AST_type_slice: parent->_type_slice.body = child; break;
        default: NOT_IMPLEMENTED("Linking to %s is not implemented yet.\n", ast_kind_name(parent->kind));
    }
}

static AST_node *parse_typedecl_postifx(AST_node *inner) {
    if (CT->kind == TOK_ampersand) {
        AST_node *ast_ref = ast_alloc(AST_type_ref, CT->line_number);
        MOVE_NEXT();
        ast_ref->_type_ref.body = inner;
        return parse_typedecl_postifx(ast_ref);
    } else if (CT->kind == TOK_lbracket) {
        
        AST_node *first = nullptr;
        AST_node *latest = nullptr;

        while (CT->kind == TOK_lbracket) {
            int line_number = CT->line_number;
            MOVE_NEXT();
            if (CT->kind == TOK_number) {
                AST_node *ast_array = ast_alloc(AST_type_array, line_number);
                ast_array->_type_array.n_elements = strtoul(CT->value.begin, 0, 10);
                MOVE_NEXT();
                take_expected(TOK_rbracket);

                if (!first) {
                    first = ast_array;
                }
                if (latest) {
                    link_arr(latest, ast_array);
                }
                latest = ast_array;
            }
            else if (CT->kind == TOK_rbracket) {
                AST_node *ast_slice = ast_alloc(AST_type_slice, line_number);
                take_expected(TOK_rbracket);

                if (!first) {
                    first = ast_slice;
                }
                if (latest) {
                    link_arr(latest, ast_slice);
                }
                latest = ast_slice;
            }
            else {
                parser_error(CT->line_number, "Expected array size or ']'.\n");
            }
        }
        
        link_arr(latest, inner);
        return parse_typedecl_postifx(first);
    }

    return inner;
}

static AST_node *try_parse_typedecl() {
    if(CT->kind == TOK_colon) {
        MOVE_NEXT();
        AST_node *ast_type = ast_alloc(AST_typename, CT->line_number);
        expect_token(TOK_identifier);
        ast_type->_typename.name = CT->value;
        MOVE_NEXT();

        return parse_typedecl_postifx(ast_type);
    }
    return nullptr;
}

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
    if (CT->kind == TOK_semicolon) {
        MOVE_NEXT();
        n->_for.result = parse_expression();
    }
    take_expected(TOK_rparen);
    n->_for.body = parse_statement();

    debug_log_parser("Leaving %s\n", __func__);

    return n;
}

static AST_node *parse_builder_string() {
    AST_node *builder = ast_alloc(AST_builder_string, CT->line_number);

    builder->builder_string.var_decl_sb = ast_alloc(AST_var_decl, CT->line_number);
    builder->builder_string.var_decl_sb->var_decl._typedecl = ast_alloc(AST_typename, CT->line_number);
    builder->builder_string.var_decl_sb->var_decl._typedecl->_typename.name = mkSV("StringBuilder");

    builder->builder_string.var_decl_arr = ast_alloc(AST_var_decl, CT->line_number);
    builder->builder_string.var_decl_arr->var_decl._typedecl = ast_alloc(AST_type_array, CT->line_number);
    builder->builder_string.var_decl_arr->var_decl._typedecl->_type_array.n_elements = 1024;
    builder->builder_string.var_decl_arr->var_decl._typedecl->_type_array.body = ast_alloc(AST_typename, CT->line_number);
    builder->builder_string.var_decl_arr->var_decl._typedecl->_type_array.body->_typename.name = mkSV("u8");

    MOVE_NEXT();

    while (CT->kind != TOK_builder_string_end) {
        if (CT->kind == TOK_string) {
            AST_node *ast_str = ast_alloc(AST_string, CT->line_number);
            ast_str->str.value = CT->value;
            ast_link_to_chain(&builder->builder_string.body, ast_str);
            MOVE_NEXT();
        } else {
            ast_link_to_chain(&builder->builder_string.body, parse_expression());
        }
    }
    MOVE_NEXT();
    return builder;
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

static AST_node *parse_postfix_operators(AST_node *n) {
    if (CT->kind == TOK_plus_plus) {
        AST_node * plus_plus = ast_alloc(AST_plus_plus, CT->line_number);
        plus_plus->plus_plus.body = n;
        plus_plus->plus_plus.postfix = true;
        MOVE_NEXT();
        return plus_plus;
    }
    else if (CT->kind == TOK_minus_minus) {
        AST_node * minus_minus = ast_alloc(AST_minus_minus, CT->line_number);
        minus_minus->minus_minus.body = n;
        minus_minus->minus_minus.postfix = true;
        MOVE_NEXT();
        return minus_minus;
    }
    return n;
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
    else if (CT->kind == TOK_builder_string_begin) {
        n = parse_builder_string();
    }
    else if (CT->kind == TOK_char_constant) {
        n = ast_alloc(AST_char_constant, CT->line_number);
        n->str.value = CT->value;
        MOVE_NEXT();
    }
    else if (CT->kind == TOK_ampersand) {
        n = ast_alloc(AST_reference, CT->line_number);
        MOVE_NEXT();
        n->reference.body = parse_primary();
    }
    else if (CT->kind == TOK_exclam || CT->kind == TOK_minus) {
        n = ast_alloc(AST_unary, CT->line_number);
        n->unary.token_kind = CT->kind;
        MOVE_NEXT();
        n->unary.body = parse_primary();
    }
    else if (CT->kind == TOK_plus_plus) {
        n = ast_alloc(AST_plus_plus, CT->line_number);
        MOVE_NEXT();
        n->plus_plus.body = parse_primary();
    }
    else if (CT->kind == TOK_minus_minus) {
        n = ast_alloc(AST_minus_minus, CT->line_number);
        MOVE_NEXT();
        n->minus_minus.body = parse_primary();
    }
    else if (CT->kind == TOK_identifier) {
        SV* name = &CT->value;
        int line_number = CT->line_number;
        MOVE_NEXT();

        if (CT->kind == TOK_lparen) {
            n = ast_alloc(AST_call, CT->line_number);
            n->call.name = *name;
            n->call.args = parse_call_arguments();
        }
        else {
            n = ast_alloc(AST_symbol, line_number);
            n->symbol.name = *name;

            while (CT->kind == TOK_lbracket || CT->kind == TOK_dot || CT->kind == TOK_colon_colon) {
                if (CT->kind == TOK_lbracket) {
                    AST_node *ast_arr = ast_alloc(AST_array_access, CT->line_number);
                    MOVE_NEXT();
                    ast_arr->_array.array = n;
                    ast_arr->_array.index = parse_expression();

                    AST_node *ast_deref = ast_alloc(AST_dereference, CT->line_number);
                    ast_deref->deref.body = ast_arr;
                    
                    n = ast_deref;
                    take_expected(TOK_rbracket);
                } 
                if (CT->kind == TOK_dot) {
                    AST_node *ast_member_access = ast_alloc(AST_member_access, CT->line_number);
                    MOVE_NEXT();
                    expect_token(TOK_identifier);
                    ast_member_access->member_access.body = n;
                    ast_member_access->member_access.name = CT->value;
                    n = ast_member_access;
                    MOVE_NEXT();
                }
                if (CT->kind == TOK_colon_colon) {
                    AST_node *ast_namespace_access = ast_alloc(AST_namespace_access, CT->line_number);
                    MOVE_NEXT();
                    expect_token(TOK_identifier);
                    ast_namespace_access->namespace_access.body = n;
                    ast_namespace_access->namespace_access.name = CT->value;
                    MOVE_NEXT();
                    n = ast_namespace_access;
                }
            }
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

    n = parse_postfix_operators(n);

    debug_log_parser("Leaving %s\n", __func__);
    return n;
}

#define NUM_PRIOS 7
#define NUM_MAX_OPERATORS_IN_PRIO 4

const uint8_t operator_table[NUM_PRIOS][NUM_MAX_OPERATORS_IN_PRIO] =  {
    { TOK_equal_assign, TOK_bind_ref, 0, 0},
    { TOK_boolean_or, 0, 0, 0},
    { TOK_boolean_and, 0, 0, 0},
    { TOK_greater, TOK_lower, TOK_greater_equal, TOK_lower_equal},
    { TOK_equal, TOK_unequal, TOK_or_equal_to, TOK_and_not_equal_to},
    { TOK_plus, TOK_minus, 0, 0},
    { TOK_asterisk, TOK_slash, TOK_percent, 0},
};

static bool is_in_prio(int op, int prio) {
    for (int i = 0; i < NUM_MAX_OPERATORS_IN_PRIO; i++) {
        if (operator_table[prio][i] == op) return true;
    }
    return false;
}

bool is_comparison_operator(int op) {
    return op == TOK_greater || op == TOK_lower || op == TOK_greater_equal || op == TOK_lower_equal;
}

static bool is_chainable(int left, int right) {
    return (is_comparison_operator(left) && is_comparison_operator(right))
    || ((left == TOK_equal || left == TOK_or_equal_to) && right == TOK_or_equal_to)
    || ((left == TOK_unequal || left == TOK_and_not_equal_to) && right == TOK_and_not_equal_to);
}

static AST_node *parse_binary_operators(int prio) {
    if (prio == NUM_PRIOS) return parse_primary();
    
    AST_node *left = parse_binary_operators(prio + 1);

    AST_node *last = nullptr;
    while (is_in_prio(CT->kind, prio)) {
        if (!last) {
            if (CT->kind == TOK_or_equal_to) parser_error(CT->line_number, "Operator '|==' requires initial '=='.");
            if (CT->kind == TOK_and_not_equal_to) parser_error(CT->line_number, "Operator '&!=' requires initial '!='.");
        }

        if (last && is_chainable(last->binary.token_kind, CT->kind)) {
            Dyn_array members;
            #define MEMBERS ((VariadicOperatorMember*)members.data)
            dyn_array_init(&members, sizeof(VariadicOperatorMember), 16);
            VariadicOperatorMember *member = dyn_array_push(&members);
            member->token_kind = last->binary.token_kind;
            member->right = last->binary.right;
            last->kind = AST_variadic_operator;
            
            while (is_chainable(MEMBERS[members.count-1].token_kind, CT->kind)) {
                member = dyn_array_push(&members);
                member->token_kind = CT->kind;
                MOVE_NEXT();
                member->right = parse_binary_operators(prio + 1);
            }

            last->variadic_operator.members = members.data;
            last->variadic_operator.num_members = members.count;
            left = last;
            last = nullptr;
        }
        else
        {
            AST_node *n = ast_alloc(AST_binary, CT->line_number);
            n->binary.left = left;
            n->binary.token_kind = CT->kind;
            MOVE_NEXT();
            n->binary.right = parse_binary_operators(prio + 1);

            left = n;
            last = n;
        }
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
            n->ret.body = parse_expression();
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

        n->var_decl._typedecl = try_parse_typedecl();

        if (CT->kind == TOK_equal_assign || CT->kind == TOK_bind_ref) {
            n->var_decl.initializer_operator = CT->kind;
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
    else if (CT->kind == TOK_semicolon) {
        n = nullptr;
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
    if (!scope->scope.body) { // empty function
        AST_node *ast_return = ast_alloc(AST_return, scope->line_number);
        ast_return->ret.implicit = true;
        scope->scope.body = ast_return;
    }
    else {
        AST_node *last = get_last_in_chain(scope->scope.body);
        if (last->kind != AST_return) {
            AST_node *ast_return = ast_alloc(AST_return, last->line_number);
            ast_return->ret.implicit = true;
            last->next = ast_return;
        }
    }
}

static AST_node *parse_function() {
    debug_log_parser("Entering %s\n", __func__);

    expect_token(TOK_keyword_fn);
    AST_node *ast_fn = ast_alloc(AST_function, CT->line_number);
    MOVE_NEXT();

    expect_token(TOK_identifier);
    ast_fn->fun.name = CT->value;
    MOVE_NEXT();

    take_expected(TOK_lparen);

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
                ast_arg->arg_decl.name = CT->value;
                MOVE_NEXT();

                ast_arg->arg_decl._typedecl = try_parse_typedecl();
                
                if (latest_arg) {
                    latest_arg->next = ast_arg;
                } else {
                    ast_arglist->arg_list.body = ast_arg;
                }
                latest_arg = ast_arg;

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

    ast_fn->fun.ret_typedecl = try_parse_typedecl();

    expect_token(TOK_lbrace);
    ast_fn->fun.body = parse_scope_body();

    insert_implicit_return(ast_fn->fun.body);

    debug_log_parser("Leaving %s\n", __func__);
    return ast_fn;
}

static AST_node *parse_struct() {
    debug_log_parser("Entering %s\n", __func__);

    expect_token(TOK_keyword_struct);
    AST_node *ast_struct = ast_alloc(AST_struct, CT->line_number);
    MOVE_NEXT();

    expect_token(TOK_identifier);
    ast_struct->_struct.name = CT->value;
    MOVE_NEXT();

    take_expected(TOK_lbrace);

    // Members
    {
        AST_node *latest = nullptr;

        while (1) {
            if (CT->kind == TOK_rbrace) {
                MOVE_NEXT();
                break;
            }
            else if(CT->kind == TOK_identifier) {
                AST_node *member = ast_alloc(AST_member_def, CT->line_number);
                member->member_def.name = CT->value;
                MOVE_NEXT();
                member->member_def._typedef = try_parse_typedecl();

                if (latest) {
                    latest->next = member;
                } else {
                    ast_struct->_struct.body = member;
                }
                latest = member;

                take_expected(TOK_semicolon);
            }
            else {
                parser_error(CT->line_number, "Expected '}' or struct member definition but got %s",
                    token_kind_name(CT->kind));
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);
    return ast_struct;
}

static AST_node *parse_enum() {
    debug_log_parser("Entering %s\n", __func__);

    expect_token(TOK_keyword_enum);
    AST_node *ast_enum = ast_alloc(AST_enum, CT->line_number);
    MOVE_NEXT();

    expect_token(TOK_identifier);
    ast_enum->_enum.name = CT->value;
    MOVE_NEXT();

    take_expected(TOK_lbrace);

    // Members
    {
        AST_node *latest = nullptr;
        int64_t latest_value = 0;

        while (1) {
            if (CT->kind == TOK_rbrace) {
                MOVE_NEXT();
                break;
            }
            else if(CT->kind == TOK_identifier) {
                AST_node *member = ast_alloc(AST_enum_member, CT->line_number);
                member->_enum_member.name = CT->value;
                MOVE_NEXT();
                if (CT->kind == TOK_equal_assign) {
                    MOVE_NEXT();
                    bool neg = CT->kind == TOK_minus;
                    if (neg) MOVE_NEXT();
                    expect_token(TOK_number);
                    latest_value = strtoul(CT->value.begin, nullptr, 0);
                    if (neg) latest_value = -latest_value;
                    MOVE_NEXT();
                }
                member->_enum_member.value = latest_value++;

                if (latest) {
                    latest->next = member;
                } else {
                    ast_enum->_enum.body = member;
                }
                latest = member;

                if (CT->kind == TOK_semicolon || CT->kind == TOK_komma) {
                    MOVE_NEXT();
                }
            }
            else {
                parser_error(CT->line_number, "Expected '}' or struct member definition but got %s",
                    token_kind_name(CT->kind));
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);
    return ast_enum;
}

AST_node *parse_program_ast() {
    debug_log_parser("Entering %s\n", __func__);

    AST_node *root = ast_alloc(AST_program, 0);
    AST_node *last = nullptr;

    current_token = tokens;
    while (1) {
        if (CT->kind == TOK_keyword_import) {
            MOVE_NEXT();
            take_expected(TOK_asterisk);
            take_expected(TOK_identifier); // from
            expect_token(TOK_string);
            Token *saved_token = current_token;
            if (!resolve_import(CT->value)) parser_error(CT->line_number, "Couldn't resolve import '%.*s'.\n", SV_prnt(CT->value));
            current_token = saved_token;
            MOVE_NEXT();
            take_expected(TOK_semicolon);
        }
        else if (CT->kind == TOK_keyword_fn) {
            AST_node *fun = parse_function();
            if (last) 
                last->next = fun;
            else
                root->program.body = fun;
            last = fun;
        }
        else if (CT->kind == TOK_keyword_struct) {
            AST_node *n = parse_struct();
            if (last) 
                last->next = n;
            else
                root->program.body = n;
            last = n;
        }
        else if (CT->kind == TOK_keyword_enum) {
            AST_node *n = parse_enum();
            if (last) 
                last->next = n;
            else
                root->program.body = n;
            last = n;
        }
        else if (CT->kind == TOK_eof) {
            break;
        } else {
            parser_error(CT->line_number, "Expected 'fn' or 'struct' or EOF but got %s",
                token_kind_printable(CT->kind));
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
    return root;
}
