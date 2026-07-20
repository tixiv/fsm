
#include "tokenizer.h"
#include "common.h"
#include "dyn_array.h"
#include "modules.h"
#include "sv.h"
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
bool is_hex_digit(char c) { return is_numeric(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
bool is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool is_allowed_at_start_of_identifier(char c) { return is_alpha(c) || c == '_'; }
bool is_allowed_in_identifier(char c) { return is_alpha(c) || is_numeric(c) || c == '_'; }

void skip_whitespace(SV *sv) {
    while (sv->len && is_whitespace(*sv->begin)) {
        sv_pop(sv);
    }
}

typedef struct {
    SV code;
    int line_number;
    bool inside_builder_string;
} Tokenizer;

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
    bool hex = false;
    if (sv_starts_with(num, "0x")) {
        sv_pop(input); sv_pop(input);
        num->len = 2;
        hex = true;
    }
    while(input->len && hex ? is_hex_digit(*input->begin) : is_numeric(*input->begin)) {
        sv_pop(input);
        num->len++;
    }
}

void read_string(SV *str, SV *input, int *line_number) {
    sv_pop(input); // initial '"'
    str->begin = input->begin;
    str->len = 0;
    while(input->len && '"' != *input->begin) {
        if (sv_starts_with(input, "\\\"")) {
            str->len += 2; sv_pop(input); sv_pop(input);
        }
        else {
            if ('\n' == sv_pop(input)) (*line_number)++;
            str->len++;
        }
    }
    if (input->len) {
        sv_pop(input); // closing '"'
    } else {
        tokenizer_error(*line_number,  "Error: unmatched '\"'. Quitting.");
        exit(EXIT_FAILURE);
    }
}

void read_char_constant(SV *str, SV *input, int *line_number) {
    sv_pop(input); // initial '

    str->begin = input->begin;
    str->len = 0;
    while(input->len && '\'' != *input->begin) {
        if ('\n' == sv_pop(input)) (*line_number)++;
        str->len++;
    }
    if (input->len) {
        sv_pop(input); // closing '''
    } else {
        tokenizer_error(*line_number,  "Error: unmatched '\''. Quitting.");
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
        case TOK_keyword_enum: return("'enum'");
        case TOK_keyword_import: return("'import'");
        case TOK_keyword_true: return("'true'");
        case TOK_keyword_false: return("'false'");
        case TOK_keyword_null: return("'null'");
        case TOK_keyword_fsm_debug: return("'fsm_debug'");
        case TOK_lparen: return("'('");
        case TOK_rparen: return("')'");
        case TOK_lbrace: return("'{'");
        case TOK_rbrace: return("'}'");
        case TOK_lbracket: return("'['");
        case TOK_rbracket: return("']'");
        case TOK_plus: return("'+'");
        case TOK_minus: return("'-'");
        case TOK_plus_plus: return("'++'");
        case TOK_minus_minus: return("'--'");
        case TOK_asterisk: return("'*'");
        case TOK_slash: return("'/'");
        case TOK_percent: return("'%'");
        case TOK_up_arrow: return("'^'");
        case TOK_equal_assign: return("'='");
        case TOK_bind_ref: return("'=>'");
        case TOK_equal: return("'=='");
        case TOK_unequal: return("'!='");
        case TOK_or_equal_to: return("'|=='");
        case TOK_and_not_equal_to: return("'&!='");
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
        case TOK_colon_colon: return "'::'";
        case TOK_ampersand: return("'&'");
        case TOK_exclam: return("'!'");
        case TOK_identifier: return("identifier");
        case TOK_string: return("string constant");
        case TOK_number: return("number");
        case TOK_char_constant: return ("char constant");
        case TOK_eof: return("EOF");
        case TOK_builder_string_begin: return "'$\"'";
        case TOK_builder_string_end:   return "'\"'";
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
    else if (sv_compare_cstr(word, "enum")) {
        push_token(TOK_keyword_enum, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "import")) {
        push_token(TOK_keyword_import, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "true")) {
        push_token(TOK_keyword_true, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "false")) {
        push_token(TOK_keyword_false, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "null")) {
        push_token(TOK_keyword_null, nullptr, line_number);
    }
    else if (sv_compare_cstr(word, "fsm_debug")) {
        push_token(TOK_keyword_fsm_debug, nullptr, line_number);
    }
    else {
        push_token(TOK_identifier, word, line_number);
    }
}

void tokenize_fsm (Tokenizer *tok);

void read_builder_string(Tokenizer *tokenizer) {
    sv_pop(&tokenizer->code); sv_pop(&tokenizer->code); // skip $"

    push_token(TOK_builder_string_begin, nullptr, tokenizer->line_number);

    SV str;
    str.begin = tokenizer->code.begin;
    str.len = 0;
    while(tokenizer->code.len && '"' != *tokenizer->code.begin) {
        if (sv_starts_with(&tokenizer->code, "\\\"")
           || sv_starts_with(&tokenizer->code, "\\{")
           || sv_starts_with(&tokenizer->code, "\\}"))
        {
            str.len += 2; sv_pop(&tokenizer->code); sv_pop(&tokenizer->code);
        }
        else {
            char c = sv_pop(&tokenizer->code);

            if (c == '\n') {
                tokenizer->line_number++;
                str.len++;
            }
            else if (c == '{') {
                push_token(TOK_string, &str, tokenizer->line_number);
                tokenizer->inside_builder_string = true;
                tokenize_fsm(tokenizer);
                tokenizer->inside_builder_string = false;
                str.begin = tokenizer->code.begin;
                str.len = 0;
            }
            else
            {
                str.len++;
            }
        }
    }

    if (str.len) push_token(TOK_string, &str, tokenizer->line_number);

    if (tokenizer->code.len) {
        sv_pop(&tokenizer->code); // closing '"'
    } else {
        tokenizer_error(tokenizer->line_number,  "Error: unmatched '\"'. Quitting.");
    }

    push_token(TOK_builder_string_end, nullptr, tokenizer->line_number);
}

void tokenize_fsm (Tokenizer *tok) {
    SV *code = &tok->code;
    while (code->len) {
        char c = *code->begin;
        
        if (is_numeric(c)) {
            SV num;
            read_number(&num, code);
            push_token(TOK_number, &num, tok->line_number);
        }
        else if (is_whitespace(c)) {
            sv_pop(code);
        }
        else if ('\n' == c) {
            sv_pop(code);
            tok->line_number++;
        }
        else if ('(' == c) {
            sv_pop(code);
            push_token(TOK_lparen, nullptr, tok->line_number);
        }
        else if (')' == c) {
            sv_pop(code);
            push_token(TOK_rparen, nullptr, tok->line_number);
        }
        else if ('{' == c) {
            sv_pop(code);
            push_token(TOK_lbrace, nullptr, tok->line_number);
        }
        else if ('}' == c) {
            sv_pop(code);
            if (tok->inside_builder_string) {
                return;
            }
            push_token(TOK_rbrace, nullptr, tok->line_number);
        }
        else if ('[' == c) {
            sv_pop(code);
            push_token(TOK_lbracket, nullptr, tok->line_number);
        }
        else if (']' == c) {
            sv_pop(code);
            push_token(TOK_rbracket, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "::")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_colon_colon, nullptr, tok->line_number);
        }
        else if (':' == c) {
            sv_pop(code);
            push_token(TOK_colon, nullptr, tok->line_number);
        }
        else if (';' == c) {
            sv_pop(code);
            push_token(TOK_semicolon, nullptr, tok->line_number);
        }
        else if ('.' == c) {
            sv_pop(code);
            push_token(TOK_dot, nullptr, tok->line_number);
        }
        else if (',' == c) {
            sv_pop(code);
            push_token(TOK_komma, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "++")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_plus_plus, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "--")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_minus_minus, nullptr, tok->line_number);
        }
        else if ('+' == c) {
            sv_pop(code);
            push_token(TOK_plus, nullptr, tok->line_number);
        }
        else if ('-' == c) {
            sv_pop(code);
            push_token(TOK_minus, nullptr, tok->line_number);
        }
        else if ('*' == c) {
            sv_pop(code);
            push_token(TOK_asterisk, nullptr, tok->line_number);
        }
        else if ('^' == c) {
            sv_pop(code);
            push_token(TOK_up_arrow, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "//")) {
            while(code->len && *code->begin != '\n')
                sv_pop(code);
        }
        else if (sv_starts_with(code, "/*")) {
            while(code->len && !sv_starts_with(code, "*/")) {
                if ('\n' == sv_pop(code)) tok->line_number++;
            }
            sv_pop(code); sv_pop(code);
        }
        else if (sv_starts_with(code, "*/")) {
            tokenizer_error(tok->line_number, "Encountered '*/' outside of comment.\n");
        }
        else if ('/' == c) {
            sv_pop(code);
            push_token(TOK_slash, nullptr, tok->line_number);
        }
        else if ('%' == c) {
            sv_pop(code);
            push_token(TOK_percent, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "=>")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_bind_ref, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "==")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_equal, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "|==")) {
            sv_pop(code);  sv_pop(code); sv_pop(code);
            push_token(TOK_or_equal_to, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "&!=")) {
            sv_pop(code);  sv_pop(code); sv_pop(code);
            push_token(TOK_and_not_equal_to, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "!=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_unequal, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, ">=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_greater_equal, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "<=")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_lower_equal, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "&&")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_boolean_and, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "||")) {
            sv_pop(code);  sv_pop(code);
            push_token(TOK_boolean_or, nullptr, tok->line_number);
        }
        else if (c == '>') {
            sv_pop(code);
            push_token(TOK_greater, nullptr, tok->line_number);
        }
        else if (c == '<') {
            sv_pop(code);
            push_token(TOK_lower, nullptr, tok->line_number);
        }
        else if (c == '=') {
            sv_pop(code);
            push_token(TOK_equal_assign, nullptr, tok->line_number);
        }
        else if (c == '&') {
            sv_pop(code);
            push_token(TOK_ampersand, nullptr, tok->line_number);
        }
        else if (c == '!') {
            sv_pop(code);
            push_token(TOK_exclam, nullptr, tok->line_number);
        }
        else if (sv_starts_with(code, "$\"")) {
            read_builder_string(tok);
        }
        else if ('"' == c) {
            SV str;
            read_string(&str, code, &tok->line_number);
            push_token(TOK_string, &str, tok->line_number);
        }
        else if ('\'' == c) {
            SV str;
            read_char_constant(&str, code, &tok->line_number);
            push_token(TOK_char_constant, &str, tok->line_number);
        }
        else if (is_allowed_at_start_of_identifier(c)) {
            SV word;
            read_word(&word, code);
            handle_word(&word, tok->line_number);
        }
        else {
            tokenizer_error(tok->line_number,  "encountered unhandled character '%c' = 0x%02X. Quitting.\n", c, c);
            exit(EXIT_FAILURE);
        }
    }
    push_token(TOK_eof, nullptr, tok->line_number);
}

void tokenizer(SV *code) {
    dyn_array_init(&current_module->tokens_dyn, sizeof(Token), 16);
    Tokenizer tok;
    tok.code = *code;
    tok.line_number = 1;
    tokenize_fsm(&tok);
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
