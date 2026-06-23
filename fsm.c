
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define nullptr ((void *)0)
#define FSM_TOKENIZER "[FSM Tokenizer]: "
#define SV_FMT "%.*s"
#define SV_prnt(sv) (int)(sv).len, (sv).begin

const char test_code[] =
"fn main()\n"
"{\n"
"    print(\"Hello, World\");"
"}\n";

typedef struct {
    const char *begin;
    size_t len;
} SV;


SV code = { test_code, sizeof(test_code)-1 };

bool sv_starts_with(SV *sv, const char *start) {
    return memcmp(sv->begin, start, strlen(start)) == 0;
}

bool sv_compare_cstr(const SV *sv, const char *cstr) {
    return sv->len == strlen(cstr) && memcmp(sv->begin, cstr, sv->len) == 0;
}

char sv_pop(SV *sv)
{
    if (sv->len == 0) return 0;
    
    char c = *sv->begin++;
    sv->len--;
    return c;
}

void sv_clone(SV *dst, const SV *src) {
    dst->begin = src->begin;
    dst->len = src->len;
}

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

#define TOKEN_LIST \
    X(TOK_keyword_fn) \
    X(TOK_keyword_print) \
    X(TOK_lparen) \
    X(TOK_rparen) \
    X(TOK_lbrace) \
    X(TOK_rbrace) \
    X(TOK_semicolon) \
    X(TOK_identifier) \
    X(TOK_string) \

enum TokenType {
#define X(name) name,
    TOKEN_LIST
#undef X
};

const char *token_type_name(enum TokenType type) {
    switch (type) {
#define X(name) case name: return #name;
        TOKEN_LIST
#undef X
    }
}

typedef struct {
    int type;
    SV value;
    int line_number;
} Token;


Token tokens[1000];
int num_tokens;


void push_token(int type, SV *value, int line_number) {
    tokens[num_tokens].type = type;
    if (value)
        sv_clone(&tokens[num_tokens].value, value);
    
    tokens[num_tokens].line_number = line_number;
    num_tokens++;
}

void handle_word(SV *word, int line_number) {
    if (sv_compare_cstr(word, "fn")) {
        push_token(TOK_keyword_fn, nullptr, line_number);    
    }
    else {
        push_token(TOK_identifier, word, line_number);
    }
}

void tokenizer() {
    int line_number = 1;
    while (code.len) {
        char c = *code.begin;
        
        if (is_alpha(c)) {
            SV word;
            read_word(&word, &code);
            handle_word(&word, line_number);
        }
        else if (is_whitespace(c)) {
            sv_pop(&code);
        }
        else if ('\n' == c) {
            sv_pop(&code);
            line_number++;
        }
        else if ('(' == c) {
            sv_pop(&code);
            push_token(TOK_lparen, nullptr, line_number);
        }
        else if (')' == c) {
            sv_pop(&code);
            push_token(TOK_rparen, nullptr, line_number);
        }
        else if ('{' == c) {
            sv_pop(&code);
            push_token(TOK_lbrace, nullptr, line_number);
        }
        else if ('}' == c) {
            sv_pop(&code);
            push_token(TOK_rbrace, nullptr, line_number);
        }
        else if (';' == c) {
            sv_pop(&code);
            push_token(TOK_semicolon, nullptr, line_number);
        }
        else if ('"' == c) {
            SV str;
            read_string(&str, &code);
            push_token(TOK_string, &str, line_number);
        }

        else {
            fprintf(stderr, FSM_TOKENIZER "encountered unhandled character '%c' = 0x%02X. Quitting.\n", c, c);
            exit(EXIT_FAILURE);
        }
    }
}

void dump_tokens() {
    for (int i=0; i<num_tokens; i++) {
        Token *t = &tokens[i];
        if (t->value.begin) {
            printf("[%2d: %s, \"" SV_FMT "\"]\n", t->line_number, token_type_name(t->type), SV_prnt(t->value));
        } else {
            printf("[%2d: %s]\n", t->line_number, token_type_name(t->type));
        }
    }
}

int main (int argc, const char *argv[]) {
    tokenizer();
    dump_tokens();
}
