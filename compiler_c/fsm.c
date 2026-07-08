
#include "ast.h"
#include "sv.h"
#include "common.h"
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
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *current_filename;

void read_file(SV *contents, const char *path)
{
    current_filename = path;
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
bool debug_opcodes = false;
bool debug_ast = false;

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

    AST_node *ast = parse_program_ast();

    chain_operators(ast);

    run_type_resolver(ast);

    resolver(ast);

    run_typechecking(ast);

    if (debug_ast)
        ast_dump_tree(ast);

    calculate_stacks(ast);
    
    ast_to_il(ast);

    if (debug_opcodes)
        dump_opcodes();

    const char *asm_file_name = "out.asm";
    output_asm(asm_file_name);

    // printf ("Compilation of '%s' to '%s' was succesfull. You can now run 'fasm %s' to generate the executable.\n",
    //    argv[1], asm_file_name, asm_file_name);
}
