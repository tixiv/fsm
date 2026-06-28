
#include "tokenizer.h"
#include "common.h"
#include "dyn_array.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define FSM_TOKENIZER "[FSM Tokenizer]: "

bool is_whitespace(char c) { return c == ' ' ||  c == '\t' || c == '\r'; }
bool is_numeric(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

void skip_whitespace(SV *sv) {
    while (sv->len && is_whitespace(*sv->begin)) {
        sv_pop(sv);
    }
}

void read_word(SV *word, SV *input) {
    word->begin = input->begin;
    word->len = 0;
    while(input->len && is_alpha(*input->begin)) {
        sv_pop(input);
        word->len++;
    }
}

void read_number(SV *num, SV *input) {
    num->begin = input->begin;
    num->len = 0;
    while(input->len && is_numeric(*input->begin)) {
        sv_pop(input);
        num->len++;
    }
}

void read_string(SV *str, SV *input) {
    sv_pop(input); // initial '"'
    str->begin = input->begin;
    str->len = 0;
    while(input->len && '"' != *input->begin) {
        sv_pop(input);
        str->len++;
    }
    if (input->len) {
        sv_pop(input); // closing '"'
    } else {
        fprintf(stderr, FSM_TOKENIZER "Error: unmatched '\"'. Quitting.");
        exit(EXIT_FAILURE);
    }
}

const char *token_kind_name(enum TokenKind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        TOKEN_LIST
#undef X
    }

    return "Undefined Token kind";
}

Dyn_array tokens_dyn;

void push_token(int kind, SV *value, int line_number) {
    Token *token = (Token*) dyn_array_push(&tokens_dyn);

    token->kind = kind;
    if (value)
        token->value = *value;
    
    token->line_number = line_number;
}

void handle_word(SV *word, int line_number) {
    if (sv_compare_cstr(word, "fn")) {
        push_token(TOK_keyword_fn, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "let")) {
        push_token(TOK_keyword_let, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "return")) {
        push_token(TOK_keyword_return, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "if")) {
        push_token(TOK_keyword_if, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "else")) {
        push_token(TOK_keyword_else, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "while")) {
        push_token(TOK_keyword_while, nullptr, line_number);
    }
    else {
        push_token(TOK_identifier, word, line_number);
    }
}

void tokenizer(SV *code) {
    dyn_array_init(&tokens_dyn, sizeof(Token), 16);
    int line_number = 1;
    
    while (code->len) {
        char c = *code->begin;
        
        if (is_alpha(c)) {
            SV word;
            read_word(&word, code);
            handle_word(&word, line_number);
        }
        else if (is_numeric(c)) {
            SV num;
            read_number(&num, code);
            push_token(TOK_number, &num, line_number);
        }
        else if (is_whitespace(c)) {
            sv_pop(code);
        }
        else if ('\n' == c) {
            sv_pop(code);
            line_number++;
        }
        else if ('(' == c) {
            sv_pop(code);
            push_token(TOK_lparen, nullptr, line_number);
        }
        else if (')' == c) {
            sv_pop(code);
            push_token(TOK_rparen, nullptr, line_number);
        }
        else if ('{' == c) {
            sv_pop(code);
            push_token(TOK_lbrace, nullptr, line_number);
        }
        else if ('}' == c) {
            sv_pop(code);
            push_token(TOK_rbrace, nullptr, line_number);
        }
        else if (';' == c) {
            sv_pop(code);
            push_token(TOK_semicolon, nullptr, line_number);
        }
        else if (',' == c) {
            sv_pop(code);
            push_token(TOK_komma, nullptr, line_number);
        }
        else if ('+' == c) {
            sv_pop(code);
            push_token(TOK_plus, nullptr, line_number);
        }
        else if ('-' == c) {
            sv_pop(code);
            push_token(TOK_minus, nullptr, line_number);
        }
        else if ('*' == c) {
            sv_pop(code);
            push_token(TOK_asterisk, nullptr, line_number);
        }
        else if (sv_starts_with(code, "//")) {
            while(code->len && *code->begin != '\n')
                sv_pop(code);
        }
        else if ('/' == c) {
            sv_pop(code);
            push_token(TOK_slash, nullptr, line_number);
        }
        else if (sv_starts_with(code, "==")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_equal, nullptr, line_number);
        }
        else if (sv_starts_with(code, "!=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_unequal, nullptr, line_number);
        }
        else if (sv_starts_with(code, ">=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_greater_equal, nullptr, line_number);
        }
        else if (sv_starts_with(code, "<=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_lower_equal, nullptr, line_number);
        }
        else if (c == '>') {
            sv_pop(code);
            push_token(TOK_greater, nullptr, line_number);
        }
        else if (c == '<') {
            sv_pop(code);
            push_token(TOK_lower, nullptr, line_number);
        }
        else if (c == '=') {
            sv_pop(code);
            push_token(TOK_equal_assign, nullptr, line_number);
        }
        else if ('"' == c) {
            SV str;
            read_string(&str, code);
            push_token(TOK_string, &str, line_number);
        }
        else {
            fprintf(stderr, FSM_TOKENIZER "encountered unhandled character '%c' = 0x%02X. Quitting.\n", c, c);
            exit(EXIT_FAILURE);
        }
    }
    push_token(TOK_eof, nullptr, line_number);
}

void dump_tokens() {
    for (int i=0; i<num_tokens; i++) {
        Token *t = &tokens[i];
        if (t->value.begin) {
            printf("[%2d: %s, \"" SV_FMT "\"]\n", t->line_number, token_kind_name(t->kind), SV_prnt(t->value));
        } else {
            printf("[%2d: %s]\n", t->line_number, token_kind_name(t->kind));
        }
    }
}
