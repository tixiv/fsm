
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#define nullptr ((void *)0)
#define FSM_TOKENIZER "[FSM Tokenizer]: "
#define SV_FMT "%.*s"
#define SV_prnt(sv) (int)(sv).len, (sv).begin

typedef struct {
    const char *begin;
    size_t len;
} SV;

void read_file(SV *contents, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[FSM Compiler] Error opening file '%s':", path);
        perror("\n");
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);

    fread(buffer, 1, size, f);
    buffer[size] = '\0';

    contents->begin = buffer;
    contents->len = size;

    fclose(f);
}

bool sv_starts_with(SV *sv, const char *start) {
    return memcmp(sv->begin, start, strlen(start)) == 0;
}

bool sv_compare_cstr(const SV *sv, const char *cstr) {
    return sv->len == strlen(cstr) && memcmp(sv->begin, cstr, sv->len) == 0;
}

bool sv_equal(const SV *sv1, const SV *sv2) {
    return sv1->len == sv2->len && memcmp(sv1->begin, sv2->begin, sv1->len) == 0;
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

#define TOKEN_LIST \
    X(TOK_keyword_fn) \
    X(TOK_keyword_let) \
    X(TOK_keyword_return) \
    X(TOK_keyword_if) \
    X(TOK_keyword_while) \
    X(TOK_lparen) \
    X(TOK_rparen) \
    X(TOK_lbrace) \
    X(TOK_rbrace) \
    X(TOK_plus) \
    X(TOK_minus) \
    X(TOK_asterisk) \
    X(TOK_slash) \
    X(TOK_equal_assign) \
    X(TOK_equal) \
    X(TOK_unequal) \
    X(TOK_komma) \
    X(TOK_semicolon) \
    X(TOK_identifier) \
    X(TOK_string) \
    X(TOK_number) \
    X(TOK_eof) \

enum TokenKind {
#define X(name) name,
    TOKEN_LIST
#undef X
};

const char *token_kind_name(enum TokenKind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        TOKEN_LIST
#undef X
    }
}

typedef struct {
    int kind;
    SV value;
    int line_number;
} Token;


Token tokens[1000];
int num_tokens;

void push_token(int kind, SV *value, int line_number) {
    tokens[num_tokens].kind = kind;
    if (value)
        sv_clone(&tokens[num_tokens].value, value);
    
    tokens[num_tokens].line_number = line_number;
    num_tokens++;
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
    else if (sv_compare_cstr(word, "while")) {
        push_token(TOK_keyword_while, nullptr, line_number);
    }
    else {
        push_token(TOK_identifier, word, line_number);
    }
}

void tokenizer(SV *code) {
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

const bool enable_debug_parser = false;

void debug_log_parser(const char * fmt, ...) {
    if (!enable_debug_parser)
        return;

    va_list args;
    va_start(args, fmt);

    printf("[FSM debug parser] ");
    vprintf(fmt, args);

    va_end(args);
}

#define OPCODE_LIST \
    X(OP_begin_fn) \
    X(OP_return) \
    X(OP_add) \
    X(OP_sub) \
    X(OP_mul) \
    X(OP_div) \
    X(OP_equal) \
    X(OP_unequal) \
    X(OP_number) \
    X(OP_string) \
    X(OP_call) \
    X(OP_push_result) \
    X(OP_push_arg) \
    X(OP_push_local_var) \
    X(OP_assign_local_var) \
    X(OP_if) \
    X(OP_end_if) \
    X(OP_while_loop) \
    X(OP_while_check) \
    X(OP_while_end) \

enum Op_Kind {
#define X(name) name,
    OPCODE_LIST
#undef X
};

const char *opcode_name(enum Op_Kind kind) {
    switch (kind) {
#define X(name) case name: return #name;
        OPCODE_LIST
#undef X
    }
}

typedef struct {
    int kind;
    SV string_value;
    uint64_t u64_value;
} Opcode;

Opcode opcodes[1000];
size_t num_opcodes;

Opcode *push_opcode(int kind, SV *value, uint64_t u64_value) {
    if (value)
        debug_log_parser("Push opcode %s, " SV_FMT ", %lu\n", opcode_name(kind), SV_prnt(*value), u64_value);
    else
        debug_log_parser("Push opcode %s, null, %lu\n", opcode_name(kind), u64_value);
    opcodes[num_opcodes].kind = kind;
    opcodes[num_opcodes].u64_value = u64_value;
    if (value) sv_clone(&opcodes[num_opcodes].string_value, value);
    num_opcodes++;

    return &opcodes[num_opcodes-1];
}

void dump_opcodes() {
    for (int i=0; i<num_opcodes; i++) {
        Opcode *t = &opcodes[i];
        if (t->string_value.begin) {
            printf("[%s, \"" SV_FMT "\"]\n", opcode_name(t->kind), SV_prnt(t->string_value));
        } else {
            printf("[%s, %lu]\n", opcode_name(t->kind), t->u64_value);
        }
    }
}

typedef struct {
    SV name;
    int num_arguments;
} Function;


Function functions[1000] = {
    {{"print", 5}, 1},
};

size_t num_functions = 1;

void push_function(const SV *name, int num_arguments) {
    sv_clone(&functions[num_functions].name, name);
    functions[num_functions].num_arguments = num_arguments;
    num_functions++;
}

Function *get_function_by_name(const SV *name)
{
    for (int i=0; i<num_functions; i++) {
        if(sv_equal(name, &functions[i].name)) {
            return &functions[i];
        }
    }
    
    return nullptr;
}

void parser_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Parser] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

Token *current_token = tokens;

#define CURRENT_TOKEN current_token
#define MOVE_NEXT() (current_token++)

int num_args = 0;
SV arg_names[100];

void reset_args() {
    num_args = 0;
}

void push_arg(SV *name) {
    sv_clone(&arg_names[num_args], name);
    num_args++;
}

int get_arg_by_name(SV *name) {
    for (int i=0; i<num_args; i++) {
        if (sv_equal(&arg_names[i], name)) {
            return i;
        }
    }
    return -1;
}


int num_local_vars = 0;
SV local_var_names[100];

void reset_local_vars() {
    num_local_vars = 0;
}

void push_local_var(SV *name) {
    local_var_names[num_local_vars++] = *name;
}

int get_local_var_by_name(SV *name) {
    for (int i=0; i<num_local_vars; i++) {
        if (sv_equal(&local_var_names[i], name)) {
            return i;
        }
    }
    return -1;
}

void parse_expression(bool result_used);

int num_while_loops;
void parse_while(bool result_used) {
    debug_log_parser("Entering %s\n", __func__);

    if (result_used) parser_error(CURRENT_TOKEN->line_number, "'void' result of 'while' used");

    if (CURRENT_TOKEN->kind != TOK_lparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '(' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    int while_num = num_while_loops++;
    push_opcode(OP_while_loop, nullptr, while_num);

    // parse conditional
    parse_expression(true);

    push_opcode(OP_while_check, nullptr, while_num);

    if (CURRENT_TOKEN->kind != TOK_rparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected ')' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    // parse body
    parse_expression(false);

    push_opcode(OP_while_end, nullptr, while_num);
    
    debug_log_parser("Leaving %s\n", __func__);
}

int num_ifs;

void parse_if(bool result_used) {
    debug_log_parser("Entering %s\n", __func__);
    if (CURRENT_TOKEN->kind != TOK_lparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '(' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    // parse if clause
    parse_expression(true);

    if (CURRENT_TOKEN->kind != TOK_rparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected ')' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    int if_num = num_ifs++;
    push_opcode(OP_if, nullptr, if_num);

    parse_expression(result_used);

    push_opcode(OP_end_if, nullptr, if_num);

    debug_log_parser("Leaving %s\n", __func__);
}

void parse_call(bool result_used) {
    debug_log_parser("Entering %s\n", __func__);
    Function *fun = get_function_by_name(&CURRENT_TOKEN->value);
    assert(fun && "Error: parse_call called without current token pointing to fn");

    MOVE_NEXT();

    if (CURRENT_TOKEN->kind != TOK_lparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '(' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    int num_args = 0;

    while (1) {
        if (CURRENT_TOKEN->kind == TOK_rparen) {
            if (num_args != fun->num_arguments) {
                parser_error(CURRENT_TOKEN->line_number, "Incorrect number of function arguments to '" SV_FMT "'. Expected %d but got %d",
                    SV_prnt(fun->name), fun->num_arguments, num_args);
            }
            MOVE_NEXT();
            push_opcode(OP_call, &fun->name, num_args);
            if (result_used) push_opcode(OP_push_result, nullptr, 0);
            debug_log_parser("Leaving %s\n", __func__);
            return;
        } else {
            parse_expression(true);
            num_args++;
            while (CURRENT_TOKEN->kind == TOK_komma) {
                MOVE_NEXT();
                parse_expression(true);
                num_args++;
            }
        }
    }
}

void parse_primary(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    if (CURRENT_TOKEN->kind == TOK_number) {
        if (result_used) push_opcode(OP_number, &CURRENT_TOKEN->value, 0);
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->kind == TOK_string) {
        if (result_used) push_opcode(OP_string, &CURRENT_TOKEN->value, 0);
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->kind == TOK_identifier) {
        if (get_function_by_name(&CURRENT_TOKEN->value)) {
            parse_call(result_used);
        }
        else if (get_local_var_by_name(&CURRENT_TOKEN->value) != -1) {
            int offset = get_local_var_by_name(&CURRENT_TOKEN->value);
            MOVE_NEXT();
            if (CURRENT_TOKEN->kind == TOK_equal_assign) {
                MOVE_NEXT();
                parse_expression(true);
                push_opcode(OP_assign_local_var, nullptr, offset);
            }

            if (result_used) push_opcode(OP_push_local_var, nullptr, offset);
        }
        else if (get_arg_by_name(&CURRENT_TOKEN->value) != -1) {
            int offset = get_arg_by_name(&CURRENT_TOKEN->value);
            if (result_used) push_opcode(OP_push_arg, &CURRENT_TOKEN->value, num_args -1 - offset);
            MOVE_NEXT();
        }
        else {
            parser_error(CURRENT_TOKEN->line_number, "Undefined identifier '" SV_FMT "'",
                SV_prnt(CURRENT_TOKEN->value));
        }
    }
    else if (CURRENT_TOKEN->kind == TOK_lparen) {
        MOVE_NEXT();
        parse_expression(result_used);

        if (CURRENT_TOKEN->kind != TOK_rparen)
        {
            parser_error(CURRENT_TOKEN->line_number, "Expected ')' but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
        }
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->kind == TOK_keyword_if) {
        MOVE_NEXT();
        parse_if(result_used);
    }
    else if (CURRENT_TOKEN->kind == TOK_keyword_while) {
        MOVE_NEXT();
        parse_while(result_used);
    }
    else {

        parser_error(CURRENT_TOKEN->line_number, "Expected expression but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
    }
    debug_log_parser("Leaving %s\n", __func__);
}

void parse_multiplicative(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_primary(result_used);

    while (CURRENT_TOKEN->kind == TOK_asterisk || CURRENT_TOKEN->kind == TOK_slash) {
        int kind = CURRENT_TOKEN->kind;

        MOVE_NEXT();
        parse_primary(result_used);

        if (result_used) {
            if (kind == TOK_asterisk) {
                push_opcode(OP_mul, nullptr, 0);
            } else {
                push_opcode(OP_div, nullptr, 0);
            }
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
}

void parse_additive(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_multiplicative(result_used);

    while (CURRENT_TOKEN->kind == TOK_plus || CURRENT_TOKEN->kind == TOK_minus) {
        int kind = CURRENT_TOKEN->kind;

        MOVE_NEXT();
        parse_multiplicative(result_used);

        if (result_used) {
            if (kind == TOK_plus) {
                push_opcode(OP_add, nullptr, 0);
            } else {
                push_opcode(OP_sub, nullptr, 0);
            }
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
}

void parse_equality(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_additive(result_used);

    while (CURRENT_TOKEN->kind == TOK_equal || CURRENT_TOKEN->kind == TOK_unequal) {
        int kind = CURRENT_TOKEN->kind;

        MOVE_NEXT();
        parse_additive(result_used);

        if (result_used) {
            if (kind == TOK_equal) {
                push_opcode(OP_equal, nullptr, 0);
            } else {
                push_opcode(OP_unequal, nullptr, 0);
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);
}

SV current_function_name;

void parse_scope_body(bool result_used)
{
    // function body
    while(1) {
        if (CURRENT_TOKEN->kind == TOK_eof) {
            parser_error(CURRENT_TOKEN->line_number, "encountered EOF while parsing function body");
        }
        else if (CURRENT_TOKEN->kind == TOK_semicolon) {
            MOVE_NEXT();
        }
        else if (CURRENT_TOKEN->kind == TOK_rbrace) {
            MOVE_NEXT();
            break;
        }
        else if (CURRENT_TOKEN->kind == TOK_keyword_return) {
            MOVE_NEXT();
            if (CURRENT_TOKEN->kind == TOK_semicolon)
            {
                push_opcode(OP_return, &current_function_name, 0);
            }
            else{
                parse_expression(true);
                push_opcode(OP_return, &current_function_name, 1);
            }
        }
        else {
            bool require_semicolon = CURRENT_TOKEN->kind != TOK_lbrace;
            parse_expression(false);
            if (CURRENT_TOKEN->kind != TOK_semicolon) {
                if (require_semicolon) parser_error(CURRENT_TOKEN->line_number, "missing ';'");
            } else {
                MOVE_NEXT();
            }
        }
    }
}

void parse_return_and_assignment(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    
    if (CURRENT_TOKEN->kind == TOK_keyword_return) {
        MOVE_NEXT();
        if (CURRENT_TOKEN->kind == TOK_semicolon)
        {
            push_opcode(OP_return, nullptr, 0);
        }
        else{
            parse_equality(true);
            push_opcode(OP_return, nullptr, 1);
        }
    }
    else if (CURRENT_TOKEN->kind == TOK_keyword_let) {
        MOVE_NEXT();
        
        if (CURRENT_TOKEN->kind != TOK_identifier) {
            parser_error(CURRENT_TOKEN->line_number, "Expected identifier, but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
        }
        push_local_var(&CURRENT_TOKEN->value);
        MOVE_NEXT();

        if (CURRENT_TOKEN->kind == TOK_equal_assign) {
            MOVE_NEXT();
            parse_equality(true);
            push_opcode(OP_assign_local_var, nullptr, num_local_vars-1);
        }
        else if (CURRENT_TOKEN->kind == TOK_semicolon) {
            // just declaring the variable without assigning it
        } else {
            parser_error(CURRENT_TOKEN->line_number, "Expected '=' or ';', but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
        }
    }
    else if (CURRENT_TOKEN->kind == TOK_lbrace) {
        MOVE_NEXT();
        parse_scope_body(result_used);
    }
    else {
        parse_equality(result_used);
    }

    debug_log_parser("Leaving %s\n", __func__);
}

void parse_expression(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_return_and_assignment(result_used);
    debug_log_parser("Leaving %s\n", __func__);
}

void parse_function() {
    debug_log_parser("Entering %s\n", __func__);
    
    reset_args();
    reset_local_vars();

    assert(CURRENT_TOKEN->kind == TOK_keyword_fn);
    MOVE_NEXT();

    if (CURRENT_TOKEN->kind != TOK_identifier) {
        parser_error(CURRENT_TOKEN->line_number, "Expected function name but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }

    SV *function_name = &CURRENT_TOKEN->value;
    MOVE_NEXT();

    if (CURRENT_TOKEN->kind != TOK_lparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '(' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    // Function arguments
    while (1) {
        if (CURRENT_TOKEN->kind == TOK_rparen) {
            MOVE_NEXT();
            break;
        }
        else if(CURRENT_TOKEN->kind == TOK_identifier) {
            push_arg(&CURRENT_TOKEN->value);
            MOVE_NEXT();
            if(CURRENT_TOKEN->kind == TOK_komma) {
                MOVE_NEXT();
            }
        }
        else {
            parser_error(CURRENT_TOKEN->line_number, "Expected ')' or function argument but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
        }
    }
    
    push_function(function_name, num_args);
    Opcode *begin_fn_opcode = push_opcode(OP_begin_fn, function_name, 0);

    if (CURRENT_TOKEN->kind != TOK_lbrace) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '{' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    current_function_name = *function_name;

    parse_scope_body(0);
    push_opcode(OP_return, function_name, 0);

    begin_fn_opcode->u64_value = num_local_vars;

    debug_log_parser("Leaving %s\n", __func__);
}

void parse_program() {
    debug_log_parser("Entering %s\n", __func__);
    while (1) {
        if (CURRENT_TOKEN->kind == TOK_keyword_fn) {
            parse_function();
        }
        else if (CURRENT_TOKEN->kind == TOK_eof) {
            return;
        } else {
            parser_error(CURRENT_TOKEN->line_number, "Expected 'fn' or EOF but got %s",
                token_kind_name(CURRENT_TOKEN->kind));
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
}

void output_asm(const char *asm_file_name) {
    FILE *file = fopen(asm_file_name, "w");

    const char* header =
    "format ELF64 executable\n"
    "segment readable executable\n"
    "entry _start\n"
    "_start:\n"
    "call fn_main\n"
    "mov rax, 60\n"
    "mov rdi, 0\n"
    "syscall\n";

    // The print function and some other assembly snippets are copied
    // from the Porth compiler https://gitlab.com/tsoding/porth which
    // was also the main inspiration for me to start this project.

    const char *print_function =
    "fn_print:\n"
    "mov rdi, [rsp+8]\n"
    "mov     r9, -3689348814741910323\n"
    "sub     rsp, 40\n"                 
    "mov     BYTE [rsp+31], 10\n"       
    "lea     rcx, [rsp+30]\n"           
    ".L2:\n"                            
    "mov     rax, rdi\n"                
    "lea     r8, [rsp+32]\n"            
    "mul     r9\n"                      
    "mov     rax, rdi\n"                
    "sub     r8, rcx\n"                 
    "shr     rdx, 3\n"                  
    "lea     rsi, [rdx+rdx*4]\n"        
    "add     rsi, rsi\n"                
    "sub     rax, rsi\n"                
    "add     eax, 48\n"                 
    "mov     BYTE [rcx], al\n"          
    "mov     rax, rdi\n"                
    "mov     rdi, rdx\n"                
    "mov     rdx, rcx\n"                
    "sub     rcx, 1\n"                  
    "cmp     rax, 9\n"                  
    "ja      .L2\n"                     
    "lea     rax, [rsp+32]\n"           
    "mov     edi, 1\n"                  
    "sub     rdx, rax\n"                
    "xor     eax, eax\n"                
    "lea     rsi, [rsp+32+rdx]\n"       
    "mov     rdx, r8\n"                 
    "mov     rax, 1\n"                  
    "syscall\n"                         
    "add     rsp, 40\n"                 
    "ret\n";

    fprintf(file, "%s", header);
    fprintf(file, "%s", print_function);

    for (int i=0; i<num_opcodes; i++) {
        Opcode *t = &opcodes[i];

        switch(t->kind) {
            case OP_begin_fn:
                fprintf(file,"; ------- begin_fn ---------\n");
                fprintf(file,"fn_" SV_FMT ":\n", SV_prnt(t->string_value));
                fprintf(file,"\t" "push rbp\n");
                fprintf(file,"\t" "mov rbp, rsp\n");
                fprintf(file,"\t" "sub rsp, %lu\n", t->u64_value * 8);
                break;
            case OP_return:
                fprintf(file,"; -------- return ----------\n");
                if (t->u64_value) fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "mov rsp, rbp\n");
                fprintf(file,"\t" "pop rbp\n");
                fprintf(file,"\t" "ret\n");
                break;
            case OP_add:
                fprintf(file,"; ---------- add -----------\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "add [rsp], rax\n");
                break;
            case OP_sub:
                fprintf(file,"; ---------- sub -----------\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "sub [rsp], rax\n");
                break;
            case OP_mul:
                fprintf(file,"; ---------- mul -----------\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "mul QWORD [rsp]\n");
                fprintf(file,"\t" "mov [rsp], rax\n");
                break;
            case OP_div:
                fprintf(file,"; ---------- div -----------\n");
                fprintf(stderr, "%s:%d Generating OP_div not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_equal:
                fprintf(file,"; ---------- == ------------\n");
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmove rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_unequal:
                fprintf(file,"; ---------- != ------------\n");
                fprintf(file,"\t" "mov rcx, 1\n");
                fprintf(file,"\t" "mov rdx, 0\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmove rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_number:
                fprintf(file,"; --------- number ---------\n");
                fprintf(file,"\t" "mov rax,%lu\n", strtoul(t->string_value.begin, 0, 10));
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_string:
                fprintf(file,"; --------- string ---------\n");
                fprintf(stderr, "%s:%d Generating OP_string not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_call:
                fprintf(file,"; ---------- call ----------\n");
                fprintf(file,"\t" "call fn_" SV_FMT "\n", SV_prnt(t->string_value));
                fprintf(file,"\t" "add rsp, %lu\n", t->u64_value * 8);
                break;
            case OP_push_result:
                fprintf(file,"; ------- push_result ------\n");
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_arg:
                fprintf(file,"; --------- push_arg -------\n");
                fprintf(file,"\t" "mov rax, [rbp+%lu]\n", 16 + 8 * t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_local_var:
                fprintf(file,"; ------ push_local_var ----\n");
                fprintf(file,"\t" "mov rax, [rbp-%lu]\n", 8 + 8 * t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_assign_local_var:
                fprintf(file,"; ----- assign_local_var ---\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "mov [rbp-%lu],rax\n", 8 + 8 * t->u64_value);
                break;
            case OP_if:
                fprintf(file,"; ------------ if ----------\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "or rax, rax\n");
                fprintf(file,"\t" "jne @f\n");
                fprintf(file,"\t" "jmp end_if_%lu\n", t->u64_value);
                fprintf(file,"@@:\n");
                break;
            case OP_while_loop:
                fprintf(file,"; -------- while loop ------\n");
                fprintf(file,"while_loop_%lu:\n", t->u64_value);
                break;
            case OP_while_check:
                fprintf(file,"; -------- while check -----\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "or rax, rax\n");
                fprintf(file,"\t" "jne @f\n");
                fprintf(file,"\t" "jmp end_while_%lu\n", t->u64_value);
                fprintf(file,"@@:\n");
                break;
            case OP_while_end:
                fprintf(file,"; --------- while_end -----\n");
                fprintf(file,"\t" "jmp while_loop_%lu\n", t->u64_value);
                fprintf(file,"end_while_%lu:\n", t->u64_value);
                break;
            case OP_end_if:
                fprintf(file,"; --------- end_if ---------\n");
                fprintf(file,"end_if_%lu:\n", t->u64_value);
                break;
            default:
                fprintf(stderr, "%s:%d Generating %s opcode is not implemented yet.\n", __FILE__, __LINE__, opcode_name(t->kind));
                exit(EXIT_FAILURE);
                break;
        }
    }



    fclose(file);
}

bool debug_tokens = false;
bool debug_opcodes = false;

int main (int argc, const char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: fsm <input.fsm>\n");
        exit(EXIT_FAILURE);
    }
    
    SV input;
    read_file(&input, argv[1]);

    tokenizer(&input);

    if(debug_tokens)
        dump_tokens();

    parse_program();
    
    if (debug_opcodes)
        dump_opcodes();

    const char *asm_file_name = "out.asm";
    output_asm(asm_file_name);

    printf ("Compilation of '%s' to '%s' was succesfull. You can now run 'fasm %s' to generate the executable.\n",
        argv[1], asm_file_name, asm_file_name);
}
