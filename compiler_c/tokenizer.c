
#include "tokenizer.h"
#include "common.h"
#include "dyn_array.h"
#include "modules.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static void tokenizer_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Tokenizer] %s:%d Error: ", current_filename, line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

bool is_whitespace(char c) { return c == ' ' ||  c == '\t' || c == '\r'; }
bool is_numeric(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool is_allowed_at_start_of_identifier(char c) { return is_alpha(c) || c == '_'; }
bool is_allowed_in_identifier(char c) { return is_alpha(c) || is_numeric(c) || c == '_'; }

void skip_whitespace(SV *sv) {
    while (sv->len && is_whitespace(*sv->begin)) {
        sv_pop(sv);
    }
}

void read_word(SV *word, SV *input) {
    word->begin = input->begin;
    word->len = 0;
    while(input->len && is_allowed_in_identifier(*input->begin) ) {
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

void read_string(SV *str, SV *input, int *line_number) {
    sv_pop(input); // initial '"'
    str->begin = input->begin;
    str->len = 0;
    while(input->len && '"' != *input->begin) {
        if ('\n' == sv_pop(input)) (*line_number)++;
        str->len++;
    }
    if (input->len) {
        sv_pop(input); // closing '"'
    } else {
        tokenizer_error(*line_number,  "Error: unmatched '\"'. Quitting.");
        exit(EXIT_FAILURE);
    }
}

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        TOKEN_LIST
#undef X
    }

    return "Undefined Token kind";
}

const char *token_kind_printable(TokenKind kind) {
    switch (kind) {
        case TOK_null: return("(null token)");
        case TOK_keyword_fn: return("'fn'");
        case TOK_keyword_let: return("'let'");
        case TOK_keyword_return: return("'return'");
        case TOK_keyword_if: return("'if'");
        case TOK_keyword_else: return("'else'");
        case TOK_keyword_while: return("'while'");
        case TOK_keyword_for: return("'for'");
        case TOK_keyword_struct: return("'struct'");
        case TOK_keyword_import: return("'import'");
        case TOK_keyword_fsm_debug: return("'fsm_debug'");
        case TOK_lparen: return("'('");
        case TOK_rparen: return("')'");
        case TOK_lbrace: return("'{'");
        case TOK_rbrace: return("'}'");
        case TOK_lbracket: return("'['");
        case TOK_rbracket: return("']'");
        case TOK_plus: return("'+'");
        case TOK_minus: return("'-'");
        case TOK_asterisk: return("'*'");
        case TOK_slash: return("'/'");
        case TOK_percent: return("'%'");
        case TOK_equal_assign: return("'='");
        case TOK_bind_ref: return("'=>'");
        case TOK_equal: return("'=='");
        case TOK_unequal: return("'!='");
        case TOK_greater: return("'>'");
        case TOK_lower: return("'<'");
        case TOK_greater_equal: return("'>='");
        case TOK_lower_equal: return("'<='");
        case TOK_boolean_and: return("'&&'");
        case TOK_boolean_or: return("'||'");
        case TOK_komma: return("','");
        case TOK_colon: return("':'");
        case TOK_semicolon: return("';'");
        case TOK_dot: return("'.'");
        case TOK_ampersand: return("'&'");
        case TOK_identifier: return("identifier");
        case TOK_string: return("string constant");
        case TOK_number: return("number");
        case TOK_eof: return("EOF");
    }
    return token_kind_name(kind);
}

void push_token(int kind, SV *value, int line_number) {
    Token *token = (Token*) dyn_array_push(&current_module->tokens_dyn);

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
    else if (sv_compare_cstr(word, "for")) {
        push_token(TOK_keyword_for, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "struct")) {
        push_token(TOK_keyword_struct, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "import")) {
        push_token(TOK_keyword_import, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "fsm_debug")) {
        push_token(TOK_keyword_fsm_debug, nullptr, line_number);
    }
    else {
        push_token(TOK_identifier, word, line_number);
    }
}

void tokenizer(SV *code) {
    dyn_array_init(&current_module->tokens_dyn, sizeof(Token), 16);
    int line_number = 1;
    
    while (code->len) {
        char c = *code->begin;
        
        if (is_numeric(c)) {
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
        else if ('[' == c) {
            sv_pop(code);
            push_token(TOK_lbracket, nullptr, line_number);
        }
        else if (']' == c) {
            sv_pop(code);
            push_token(TOK_rbracket, nullptr, line_number);
        }
        else if (':' == c) {
            sv_pop(code);
            push_token(TOK_colon, nullptr, line_number);
        }
        else if (';' == c) {
            sv_pop(code);
            push_token(TOK_semicolon, nullptr, line_number);
        }
        else if ('.' == c) {
            sv_pop(code);
            push_token(TOK_dot, nullptr, line_number);
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
        else if (sv_starts_with(code, "/*")) {
            while(code->len && !sv_starts_with(code, "*/")) {
                if ('\n' == sv_pop(code)) line_number++;
            }
            sv_pop(code); sv_pop(code);
        }
        else if (sv_starts_with(code, "*/")) {
            tokenizer_error(line_number, "Encountered '*/' outside of comment.\n");
        }
        else if ('/' == c) {
            sv_pop(code);
            push_token(TOK_slash, nullptr, line_number);
        }
        else if ('%' == c) {
            sv_pop(code);
            push_token(TOK_percent, nullptr, line_number);
        }
        else if (sv_starts_with(code, "=>")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_bind_ref, nullptr, line_number);
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
        else if (sv_starts_with(code, "&&")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_boolean_and, nullptr, line_number);
        }
        else if (sv_starts_with(code, "||")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_boolean_or, nullptr, line_number);
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
        else if (c == '&') {
            sv_pop(code);
            push_token(TOK_ampersand, nullptr, line_number);
        }
        else if ('"' == c) {
            SV str;
            read_string(&str, code, &line_number);
            push_token(TOK_string, &str, line_number);
        }
        else if (is_allowed_at_start_of_identifier(c)) {
            SV word;
            read_word(&word, code);
            handle_word(&word, line_number);
        }
        else {
            tokenizer_error(line_number,  "encountered unhandled character '%c' = 0x%02X. Quitting.\n", c, c);
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
