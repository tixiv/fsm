
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

const char test_code[] =
"fn main()\n"
"{\n"
"    print(\"Hello, World\");"
"}\n";

typedef struct {
    const char *begin;
    size_t len;
} SV;


SV code = { test_code, sizeof(test_code) };

bool sv_starts_with(SV *sv, const char *start) {
    return memcmp(sv->begin, start, strlen(start)) == 0;
}

char sv_pop(SV *sv)
{
    if (sv->len == 0) return 0;
    
    char c = *sv->begin++;
    sv->len--;
    return c;
}

bool is_whitespace(char c) { return c == ' ' ||  c == '\t' || c == '\r'; }
bool is_numeric(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

void skip_whitespace(SV *sv) {
    while (sv->len && is_whitespace(*sv->begin)) {
        sv_pop(sv);
    }
}

void read_word(SV *word, SV* str) {
    word->begin = str->begin;
    word->len = 0;
    while(str->len && is_alpha(*str->begin)) {
        str->begin++;
        str->len--;
        word->len++;
    }
}

void tokenizer() {
    int line_number;
    while (code.len) {
        char c = *code.begin;
        
        if (is_alpha(c)) {
            SV word;
            read_word(&word, &code);
            printf("%.*s\n", (int)word.len, word.begin);
        }
        else if (is_whitespace(c)) {
            sv_pop(&code);
        }
        else if ('\n' == c) {
            sv_pop(&code);
            line_number++;
        }
        else {
            printf("[FSM LEXER] encountered unhandled character '%c'. Quitting.\n", c);
            exit(EXIT_FAILURE);
        }
    }
}


int main (int argc, const char *argv[]) {
    tokenizer();
}
