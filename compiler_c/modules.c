
#include "modules.h"
#include "string_builder.h"
#include "ast.h"
#include "common.h"
#include "sv.h"
#include "tokenizer.h"
#include "parser_ast.h"
#include "opcodes.h"
#include "generator.h"
#include "ast_to_il.h"
#include "operator_chaining.h"
#include "resolver.h"
#include "type_checker.h"
#include "type_resolver.h"
#include "calculate_stacks.h"
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


const char *current_filename;

Module *current_module;

Dyn_array modules; // Module[]

void read_file(SV *contents, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[FSM Compiler] Error opening file '%s': %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);

    if (fread(buffer, 1, size, f) != size) {
        fprintf(stderr, "[FSM Compiler] Error reading from file '%s': %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    buffer[size] = '\0';

    contents->begin = buffer;
    contents->len = size;

    fclose(f);
}


bool debug_tokens = false;
bool debug_ast = false;

void compile_module(const char *filename) {
    current_module = dyn_array_push(&modules);

    current_filename = filename;
    SV input;
    read_file(&input, filename);
    tokenizer(&input);

    if(debug_tokens)
        dump_tokens();

    AST_node *ast = parse_program_ast();

    chain_operators(ast);

    run_type_resolver(ast);

    resolver(ast);

    run_typechecking(ast);

    if (debug_ast)
        ast_dump_tree(ast);

    calculate_stacks(ast);

    current_module->ast = ast;
}

bool resolve_import (SV name) {
    SB sb; char buf_1 [1024]; sb_init(&sb, buf_1, 1024);

    const char *saved_filename = current_filename;
    Module *saved_module = current_module;

    // Need terminating zeroe
    sb_printf(&sb, "%.*s.fsm", SV_prnt(name));

    if (access(sb.buffer, R_OK) == 0) {
        compile_module(sb.buffer);
    }
    else {
        sb_reset(&sb);
        sb_printf(&sb, "stdlib/%.*s.fsm", SV_prnt(name));

        if (access(sb.buffer, R_OK) == 0) {
            compile_module(sb.buffer);
        }
        else {
            current_filename = saved_filename;
            return false;
        }
    }
    current_filename = saved_filename;
    current_module = saved_module;
    return true;
}

void compile_program(const char *filename) {
    dyn_array_init(&modules, sizeof(Module), 16);
    init_resolver();
    compile_module(filename);
}
