
#include "parser.h"
#include "dyn_array.h"
#include "tokenizer.h"
#include "opcodes.h"
#include "common.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static const bool enable_debug_parser = false;

static void debug_log_parser(const char * fmt, ...) {
    if (!enable_debug_parser)
        return;

    va_list args;
    va_start(args, fmt);

    printf("[FSM debug parser] ");
    vprintf(fmt, args);

    va_end(args);
}

static size_t parser_push_opcode(int kind, SV *value, uint64_t u64_value) {
    if (value)
        debug_log_parser("Push opcode %s, " SV_FMT ", %lu\n", opcode_name(kind), SV_prnt(*value), u64_value);
    else
        debug_log_parser("Push opcode %s, null, %lu\n", opcode_name(kind), u64_value);

    return push_opcode(kind, value, u64_value);
}

typedef struct {
    SV name;
    int num_arguments;
} Function;

static Dyn_array functions_dyn;

#define num_functions (functions_dyn.count)
#define functions ((Function *)functions_dyn.data)

static void push_function(const SV *name, int num_arguments) {
    Function *f = (Function *) dyn_array_push(&functions_dyn);

    f->name = *name;
    f->num_arguments = num_arguments;
}

// The names passed to this function must be allocated in static memory
// because the string view inside 'Function' will reference the data.
static void push_builtin_function(const char *name, int num_arguments) {
    Function *f = (Function *) dyn_array_push(&functions_dyn);
    f->name.begin = name;
    f->name.len = strlen(name);
    f->num_arguments = num_arguments;
}

static Function *get_function_by_name(const SV *name)
{
    for (int i=0; i<num_functions; i++) {
        if(sv_equal(name, &functions[i].name)) {
            return &functions[i];
        }
    }
    
    return nullptr;
}

static void parser_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Parser] Line %d Error: ", line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

static Token *current_token;

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
    parser_push_opcode(OP_while_loop, nullptr, while_num);

    // parse conditional
    parse_expression(true);

    parser_push_opcode(OP_while_check, nullptr, while_num);

    if (CURRENT_TOKEN->kind != TOK_rparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected ')' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    // parse body
    parse_expression(false);

    parser_push_opcode(OP_while_end, nullptr, while_num);
    
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

    // parse condition
    parse_expression(true);

    if (CURRENT_TOKEN->kind != TOK_rparen) {
        parser_error(CURRENT_TOKEN->line_number, "Expected ')' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    int if_num = num_ifs++;
    size_t if_opcode = parser_push_opcode(OP_if, nullptr, if_num);

    // parse if block
    parse_expression(result_used);

    if (CURRENT_TOKEN->kind == TOK_keyword_else) {
        MOVE_NEXT();
        parser_push_opcode(OP_else, nullptr, if_num);
        opcodes[if_opcode].u32_value[1] = true; // Mark has 'else'

        // parse else block
        parse_expression(result_used);
    }
    else if (result_used) {
        parser_error(CURRENT_TOKEN->line_number,
            "When using the result of an 'if' then an 'else' clause is mandatory");
    }

    parser_push_opcode(OP_end_if, nullptr, if_num);

    debug_log_parser("Leaving %s\n", __func__);
}

static void parse_call(bool result_used) {
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
            parser_push_opcode(OP_call, &fun->name, num_args);
            if (result_used) parser_push_opcode(OP_push_result, nullptr, 0);
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

static void parse_primary(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    if (CURRENT_TOKEN->kind == TOK_number) {
        if (result_used) parser_push_opcode(OP_push_literal, &CURRENT_TOKEN->value, 0);
        MOVE_NEXT();
    }
    else if (CURRENT_TOKEN->kind == TOK_string) {
        if (result_used) parser_push_opcode(OP_string, &CURRENT_TOKEN->value, 0);
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
                parser_push_opcode(OP_assign_local_var, nullptr, offset);
            }

            if (result_used) parser_push_opcode(OP_push_local_var, nullptr, offset);
        }
        else if (get_arg_by_name(&CURRENT_TOKEN->value) != -1) {
            int offset = get_arg_by_name(&CURRENT_TOKEN->value);
            if (result_used) parser_push_opcode(OP_push_arg, &CURRENT_TOKEN->value, num_args -1 - offset);
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
                parser_push_opcode(OP_mul, nullptr, 0);
            } else {
                parser_push_opcode(OP_div, nullptr, 0);
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
                parser_push_opcode(OP_add, nullptr, 0);
            } else {
                parser_push_opcode(OP_sub, nullptr, 0);
            }
        }
    }
    debug_log_parser("Leaving %s\n", __func__);
}

void parse_comparison(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_additive(result_used);

    while (CURRENT_TOKEN->kind == TOK_greater || CURRENT_TOKEN->kind == TOK_lower ||
           CURRENT_TOKEN->kind == TOK_greater_equal || CURRENT_TOKEN->kind == TOK_lower_equal) {
        int kind = CURRENT_TOKEN->kind;

        MOVE_NEXT();
        parse_additive(result_used);

        if (result_used) {
            if (kind == TOK_greater) {
                parser_push_opcode(OP_compare_GT, nullptr, 0);
            }
            else if (kind == TOK_lower) {
                parser_push_opcode(OP_compare_LT, nullptr, 0);
            }
            else if (kind == TOK_greater_equal) {
                parser_push_opcode(OP_compare_GE, nullptr, 0);
            }
            else {
                parser_push_opcode(OP_compare_LE, nullptr, 0);
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);
}

void parse_equality(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    parse_comparison(result_used);

    while (CURRENT_TOKEN->kind == TOK_equal || CURRENT_TOKEN->kind == TOK_unequal) {
        int kind = CURRENT_TOKEN->kind;

        MOVE_NEXT();
        parse_comparison(result_used);

        if (result_used) {
            if (kind == TOK_equal) {
                parser_push_opcode(OP_equal, nullptr, 0);
            } else {
                parser_push_opcode(OP_unequal, nullptr, 0);
            }
        }
    }

    debug_log_parser("Leaving %s\n", __func__);
}

SV current_function_name;

static void parse_scope_body(bool result_used)
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
                parser_push_opcode(OP_return, &current_function_name, 0);
            }
            else{
                parse_expression(true);
                parser_push_opcode(OP_return, &current_function_name, 1);
            }
        }
        else {
            bool require_semicolon = false;
            parse_expression(false);
            if (CURRENT_TOKEN->kind != TOK_semicolon) {
                if (require_semicolon) parser_error(CURRENT_TOKEN->line_number, "missing ';'");
            } else {
                MOVE_NEXT();
            }
        }
    }
}

static void parse_return_and_assignment(bool result_used)
{
    debug_log_parser("Entering %s\n", __func__);
    
    if (CURRENT_TOKEN->kind == TOK_keyword_return) {
        MOVE_NEXT();
        if (CURRENT_TOKEN->kind == TOK_semicolon)
        {
            parser_push_opcode(OP_return, nullptr, 0);
        }
        else{
            parse_equality(true);
            parser_push_opcode(OP_return, nullptr, 1);
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
            parser_push_opcode(OP_assign_local_var, nullptr, num_local_vars-1);
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

static void parse_function() {
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
    size_t begin_fn_opcode = parser_push_opcode(OP_begin_fn, function_name, 0);

    if (CURRENT_TOKEN->kind != TOK_lbrace) {
        parser_error(CURRENT_TOKEN->line_number, "Expected '{' but got %s",
            token_kind_name(CURRENT_TOKEN->kind));
    }
    MOVE_NEXT();

    current_function_name = *function_name;

    parse_scope_body(0);
    parser_push_opcode(OP_return, function_name, 0);

    opcodes[begin_fn_opcode].u64_value = num_local_vars;

    debug_log_parser("Leaving %s\n", __func__);
}

void parse_program() {
    dyn_array_init(&functions_dyn, sizeof(Function), 16);
    push_builtin_function("print", 1);

    current_token = tokens;
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
