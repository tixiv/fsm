
#pragma once

#include "sv.h"
#include "dyn_array.h"
#include "modules.h"

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
    X(TOK_keyword_struct) \
    X(TOK_keyword_enum) \
    X(TOK_keyword_import) \
    X(TOK_keyword_fsm_debug) \
    X(TOK_lparen) \
    X(TOK_rparen) \
    X(TOK_lbrace) \
    X(TOK_rbrace) \
    X(TOK_lbracket) \
    X(TOK_rbracket) \
    X(TOK_plus) \
    X(TOK_minus) \
    X(TOK_plus_plus) \
    X(TOK_minus_minus) \
    X(TOK_asterisk) \
    X(TOK_slash) \
    X(TOK_percent) \
    X(TOK_equal_assign) \
    X(TOK_bind_ref) \
    X(TOK_equal) \
    X(TOK_unequal) \
    X(TOK_or_equal_to) \
    X(TOK_and_not_equal_to) \
    X(TOK_greater) \
    X(TOK_lower) \
    X(TOK_greater_equal) \
    X(TOK_lower_equal) \
    X(TOK_boolean_and) \
    X(TOK_boolean_or) \
    X(TOK_komma) \
    X(TOK_colon) \
    X(TOK_semicolon) \
    X(TOK_dot) \
    X(TOK_colon_colon) \
    X(TOK_ampersand) \
    X(TOK_exclam) \
    X(TOK_identifier) \
    X(TOK_string) \
    X(TOK_number) \
    X(TOK_char_constant) \
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

#define num_tokens (current_module->tokens_dyn.count)
#define tokens ((Token*)current_module->tokens_dyn.data)
