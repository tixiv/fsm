
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

const char test_code[] =
"fn main()\n"
"{\n"
//"   puts(\"Hello, World\");\n"
//"   print(5 + 4*3 + (7 + 8)*9);\n"
"   print(42);\n"
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
    X(TOK_lparen) \
    X(TOK_rparen) \
    X(TOK_lbrace) \
    X(TOK_rbrace) \
    X(TOK_plus) \
    X(TOK_minus) \
    X(TOK_asterisk) \
    X(TOK_slash) \
    X(TOK_komma) \
    X(TOK_semicolon) \
    X(TOK_identifier) \
    X(TOK_string) \
    X(TOK_number) \
    X(TOK_eof) \

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
        else if (is_numeric(c)) {
            SV num;
            read_number(&num, &code);
            push_token(TOK_number, &num, line_number);
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
        else if (',' == c) {
            sv_pop(&code);
            push_token(TOK_komma, nullptr, line_number);
        }
        else if ('+' == c) {
            sv_pop(&code);
            push_token(TOK_plus, nullptr, line_number);
        }
        else if ('-' == c) {
            sv_pop(&code);
            push_token(TOK_minus, nullptr, line_number);
        }
        else if ('*' == c) {
            sv_pop(&code);
            push_token(TOK_asterisk, nullptr, line_number);
        }
        else if ('/' == c) {
            sv_pop(&code);
            push_token(TOK_slash, nullptr, line_number);
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
    push_token(TOK_eof, nullptr, line_number);
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

#define IL_LIST \
    X(OP_begin_fn) \
    X(OP_end_fn) \
    X(OP_add) \
    X(OP_sub) \
    X(OP_mul) \
    X(OP_div) \
    X(OP_number) \
    X(OP_string) \
    X(OP_call) \

enum Op_Type {
#define X(name) name,
    IL_LIST
#undef X
};

const char *opcode_type_name(enum Op_Type type) {
    switch (type) {
#define X(name) case name: return #name;
        IL_LIST
#undef X
    }
}

typedef struct {
    int type;
    SV string_value;
    uint64_t u64_value;
} Opcode;

Opcode opcodes[1000];
size_t num_opcodes;

void push_opcode(int type, SV *value, uint64_t u64_value) {
    opcodes[num_opcodes].type = type;
    opcodes[num_opcodes].u64_value = u64_value;
    if (value) sv_clone(&opcodes[num_opcodes].string_value, value);
    num_opcodes++;
}

void dump_opcodes() {
    for (int i=0; i<num_opcodes; i++) {
        Opcode *t = &opcodes[i];
        if (t->string_value.begin) {
            printf("[%s, \"" SV_FMT "\"]\n", opcode_type_name(t->type), SV_prnt(t->string_value));
        } else {
            printf("[%s, %lu]\n", opcode_type_name(t->type), t->u64_value);
        }
    }
}

typedef struct {

} Function;

Function builtin_print;

Function *get_function_by_name(SV *name)
{
    if(sv_compare_cstr(name, "print")) {
        return &builtin_print;
    } else if (sv_compare_cstr(name, "puts")) {
        return &builtin_print;
    }
    return nullptr;
}

Token *current_token = tokens;

#define CURRENT_TOKEN current_token
#define MOVE_NEXT() (current_token++)

void parse_expression();

void parse_call() {
    printf("Entering %s\n", __func__);
    SV function_name;
    sv_clone(&function_name, &CURRENT_TOKEN->value);
    MOVE_NEXT();

    if (CURRENT_TOKEN->type != TOK_lparen) {
        fprintf(stderr, "[FSM Parser] Line %d Error: Expected '(' but got %s\n",
        CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        exit(EXIT_FAILURE);
    }
    MOVE_NEXT();

    int num_vars = 0;

    while (1) {
        if (CURRENT_TOKEN->type == TOK_rparen) {
            push_opcode(OP_call, &function_name, num_vars);
            MOVE_NEXT();
            return;
        } else {
            parse_expression();
            num_vars++;
            while (CURRENT_TOKEN->type == TOK_komma) {
                MOVE_NEXT();
                parse_expression();
                num_vars++;
            }
        }
    }
}

void parse_primary()
{
    printf("Entering %s\n", __func__);
    if (CURRENT_TOKEN->type == TOK_number) {   
        push_opcode(OP_number, &CURRENT_TOKEN->value, 0);
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->type == TOK_string) {
        push_opcode(OP_string, &CURRENT_TOKEN->value, 0);
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->type == TOK_identifier) {
        if (get_function_by_name(&CURRENT_TOKEN->value)) {
            parse_call();
        } else {
            fprintf(stderr, "[FSM Parser] Line %d Error: Undefined identifier '" SV_FMT "'\n",
                CURRENT_TOKEN->line_number, SV_prnt(CURRENT_TOKEN->value));
            exit(EXIT_FAILURE);
        }
    }
    else if (CURRENT_TOKEN->type == TOK_lparen) {
        MOVE_NEXT();
        parse_expression();

        if (CURRENT_TOKEN->type != TOK_rparen)
        {
            fprintf(stderr, "[FSM Parser] Line %d Error: Expected ')' but got %s\n",
                CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
            exit(EXIT_FAILURE);
        }
        MOVE_NEXT();
    }
    else {

        fprintf(stderr, "[FSM Parser] Line %d Error: Expected expression but got %s\n",
                CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
            exit(EXIT_FAILURE);
    }
    printf("Leaving %s\n", __func__);
}

void parse_multiplicative(void)
{
    printf("Entering %s\n", __func__);
    parse_primary();

    while (CURRENT_TOKEN->type == TOK_asterisk || CURRENT_TOKEN->type == TOK_slash) {
        int type = CURRENT_TOKEN->type;

        MOVE_NEXT();
        parse_primary();

        if (type == TOK_asterisk) {
            push_opcode(OP_mul, nullptr, 0);
        } else {
            push_opcode(OP_div, nullptr, 0);
        }
    }
    printf("Leaving %s\n", __func__);
}

void parse_additive()
{
    printf("Entering %s\n", __func__);
    parse_multiplicative();

    while (CURRENT_TOKEN->type == TOK_plus || CURRENT_TOKEN->type == TOK_minus) {
        int type = CURRENT_TOKEN->type;

        MOVE_NEXT();
        parse_multiplicative();

        if (type == TOK_plus) {
            push_opcode(OP_add, nullptr, 0);
        } else {
            push_opcode(OP_sub, nullptr, 0);
        }
    }
    printf("Leaving %s\n", __func__);
}

void parse_expression()
{
    printf("Entering %s\n", __func__);
    parse_additive();
    printf("Leaving %s\n", __func__);
}

void parse_function() {
    printf("Entering %s\n", __func__);
    assert(CURRENT_TOKEN->type == TOK_keyword_fn);
    MOVE_NEXT();

    if (CURRENT_TOKEN->type != TOK_identifier) {
        fprintf(stderr, "[FSM Parser] Line %d Error: Expected function name but got %s\n",
            CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        exit(EXIT_FAILURE);
    }
    SV *function_name = &CURRENT_TOKEN->value;
    MOVE_NEXT();

    if (CURRENT_TOKEN->type != TOK_lparen) {
        fprintf(stderr, "[FSM Parser] Line %d Error: Expected '(' but got %s\n",
            CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        exit(EXIT_FAILURE);
    }
    MOVE_NEXT();

    if (CURRENT_TOKEN->type != TOK_rparen) {
        fprintf(stderr, "[FSM Parser] Line %d Error: Expected ')' but got %s\n",
            CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        exit(EXIT_FAILURE);
    }
    MOVE_NEXT();

    int num_args = 0;
    // TODO: Parse function arguments
    
    if (CURRENT_TOKEN->type != TOK_lbrace) {
        fprintf(stderr, "[FSM Parser] Line %d Error: Expected '{' but got %s\n",
            CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        exit(EXIT_FAILURE);
    }
    MOVE_NEXT();

    push_opcode(OP_begin_fn, function_name, num_args);

    // function body
    while(1) {
        if (CURRENT_TOKEN->type == TOK_eof) {
            fprintf(stderr, "[FSM Parser] Line %d Error: encountered EOF while parsing function body\n", CURRENT_TOKEN->line_number);
        }
        else if (CURRENT_TOKEN->type == TOK_semicolon) {
            MOVE_NEXT();
        }
        else if (CURRENT_TOKEN->type == TOK_rbrace) {
            MOVE_NEXT();
            push_opcode(OP_end_fn, function_name, 0);
            break;
        }
        else {
            parse_expression();
            if (CURRENT_TOKEN->type != TOK_semicolon) {
                fprintf(stderr, "[FSM Parser] Line %d Error: missing ';'\n", CURRENT_TOKEN->line_number);
                exit(EXIT_FAILURE);
            } else {
                MOVE_NEXT();
            }
        }
    }

    printf("Leaving %s\n", __func__);
}

void parse_program() {
    printf("Entering %s\n", __func__);
    while (1) {
        if (CURRENT_TOKEN->type == TOK_keyword_fn) {
            parse_function();
        }
        else if (CURRENT_TOKEN->type == TOK_eof) {
            return;
        } else {
            fprintf(stderr, "[FSM Parser] Line %d Error: Expected 'fn' or EOF but got %s\n",
                CURRENT_TOKEN->line_number, token_type_name(CURRENT_TOKEN->type));
        }
    }
    printf("Leaving %s\n", __func__);
}




void output_asm() {
    FILE *file = fopen("out.asm", "w");

    const char* header =
    "format ELF64 executable"
    "segment readable executable"
    "entry main";

    // Print function borrowed from Porth compiler
    const char *print_function =
    "print:\n"
    "mov rdi, [rsp+8]"
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

        switch(t->type) {
            case OP_begin_fn:
                fprintf(file,"; ------- begin_fn ---------\n");
                fprintf(file,"fn_" SV_FMT ":\n", SV_prnt(t->string_value));
                break;
            case OP_end_fn:
                fprintf(file,"; -------- end_fn ----------\n");
                fprintf(file,"\t" "ret\n");
                break;
            case OP_add:
                fprintf(file,"; ---------- add -----------\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "add rax, rbx\n");
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_sub:
                fprintf(file,"; ---------- sub -----------\n");
                fprintf(stderr, "%s:%d Generating OP_sub not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_mul:
                fprintf(file,"; ---------- mul -----------\n");
                fprintf(stderr, "%s:%d Generating OP_mul not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_div:
                fprintf(file,"; ---------- div -----------\n");
                fprintf(stderr, "%s:%d Generating OP_div not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_number:
                fprintf(file,"; --------- number ---------\n");
                fprintf(file,"\t" "mov rax,%lu\n", strtoul(t->string_value.begin, 0, 10));
                fprintf(file,"\t" "push rax\n");
                exit(EXIT_FAILURE);
                break;
            case OP_string:
                fprintf(file,"; --------- string ---------\n");
                fprintf(stderr, "%s:%d Generating OP_string not implemented yet.\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
                break;
            case OP_call:
                fprintf(file,"; ---------- call ----------\n");
                fprintf(file,"\t" "call fn_" SV_FMT "\n", SV_prnt(t->string_value));
                break;
        }
    }

    fclose(file);
}


int main (int argc, const char *argv[]) {
    tokenizer();
    dump_tokens();
    parse_program();
    dump_opcodes();
    output_asm();
}
