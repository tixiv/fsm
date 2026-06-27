
#include "sv.h"
#include "tokenizer.h"
#include "parser.h"
#include "opcodes.h"
#include "generator.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

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

    if (fread(buffer, 1, size, f) != size) {
        fprintf(stderr, "[FSM Compiler] Error opening file '%s':", path);
        perror("\n");
        exit(EXIT_FAILURE);
    }
    buffer[size] = '\0';

    contents->begin = buffer;
    contents->len = size;

    fclose(f);
}


bool debug_tokens = false;
bool debug_opcodes = true;

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
