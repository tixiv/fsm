
#include "ast.h"
#include "sv.h"
#include "common.h"
#include "tokenizer.h"
#include "parser_ast.h"
#include "opcodes.h"
#include "generator.h"
#include "ast_to_il.h"
#include "resolver.h"
#include "type_checker.h"
#include "type_resolver.h"
#include "calculate_stacks.h"
#include "modules.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


bool debug_opcodes = false;

int main (int argc, const char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: fsm <input.fsm>\n");
        exit(EXIT_FAILURE);
    }
    
    compile_program(argv[1]);

    ast_to_il_init();
    
    for (int i = 0; i < modules.count; i++) {
        ast_to_il(((Module*)modules.data)[i].ast);
    }
    
    if (debug_opcodes)
        dump_opcodes();

    const char *asm_file_name = "out.asm";
    output_asm(asm_file_name);

    // printf ("Compilation of '%s' to '%s' was succesfull. You can now run 'fasm %s' to generate the executable.\n",
    //    argv[1], asm_file_name, asm_file_name);
}
