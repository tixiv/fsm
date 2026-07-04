
#pragma once

#include "sv.h"
#include "dyn_array.h"

typedef struct {
    int kind;
    SV value;
    int line_number;
} Token;

#define TOKEN_LIST \
    X(TOK_null) \
    X(TOK_keyword_fn) \
    X(TOK_keyword_let) \
    X(TOK_keyword_return) \
    X(TOK_keyword_if) \
    X(TOK_keyword_else) \
    X(TOK_keyword_while) \
    X(TOK_keyword_for) \
    X(TOK_lparen) \
    X(TOK_rparen) \
    X(TOK_lbrace) \
    X(TOK_rbrace) \
    X(TOK_plus) \
    X(TOK_minus) \
    X(TOK_asterisk) \
    X(TOK_slash) \
    X(TOK_percent) \
    X(TOK_equal_assign) \
    X(TOK_equal) \
    X(TOK_unequal) \
    X(TOK_greater) \
    X(TOK_lower) \
    X(TOK_greater_equal) \
    X(TOK_lower_equal) \
    X(TOK_boolean_and) \
    X(TOK_boolean_or) \
    X(TOK_komma) \
    X(TOK_semicolon) \
    X(TOK_identifier) \
    X(TOK_string) \
    X(TOK_number) \
    X(TOK_eof) \

typedef enum {
#define X(name) name,
    TOKEN_LIST
#undef X
} TokenKind;

const char *token_kind_name(TokenKind kind);
const char *token_kind_printable(TokenKind kind);
void dump_tokens();
void tokenizer(SV *code);

extern Dyn_array tokens_dyn;

#define num_tokens (tokens_dyn.count)
#define tokens ((Token*)tokens_dyn.data)
