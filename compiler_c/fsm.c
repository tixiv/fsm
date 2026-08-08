
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
#include <bits/getopt_core.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>


bool debug_opcodes = false;

static struct option long_options[] = {
    {"help",             no_argument, 0, 'h'},
    {"debug-tokens",     no_argument, 0, 't'},
    {"debug-ast",  required_argument, 0, 'a'},
    {"debug-opcodes",    no_argument, 0, 'p'},
    {0, 0, 0, 0}
};

static void usage(const char *prg) {
    fprintf(stderr, "Usage: %s [options] <input.fsm>\n", prg);
    fprintf(stderr, "  -t --debug-tokens  : Debug the tokens\n");
    fprintf(stderr, "  -a --debug-ast <n> : Debug the AST at different points\n");
    fprintf(stderr, "  -p --debug-opcodes : Debug the generated opcodes\n");
}

int main (int argc, char **argv) {
    int option_index = 0;

    int opt;
    while ((opt = getopt_long(argc, argv, "hta:p", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return 0;
            case 't':
                debug_tokens = true;
                break;
            case 'a':
                debug_ast = optarg;
                break;
            case 'p':
                debug_opcodes = true;
                break;
            default:
                fprintf(stderr, "Unknown option '-%c'\n", optopt);
                return 1;
        }
    }

    if (optind == argc) {
        fprintf(stderr, "%s: Error: No input file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    compile_program(argv[optind]);

    ast_to_il_init();
    
    for (size_t i = 0; i < modules.count; i++) {
        Module *module = &((Module*)modules.data)[i];
        current_filename = module->filename;
        ast_to_il(module->ast);
    }
    
    if (debug_opcodes)
        dump_opcodes();

    const char *asm_file_name = "out.asm";
    output_asm(asm_file_name);

    // printf ("Compilation of '%s' to '%s' was succesfull. You can now run 'fasm %s' to generate the executable.\n",
    //    argv[1], asm_file_name, asm_file_name);
}
