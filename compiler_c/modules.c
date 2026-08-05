
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
#include "resolver.h"
#include "type_checker.h"
#include "type_resolver.h"
#include "calculate_stacks.h"
#include <sys/types.h>
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
    ssize_t size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);

    if ((ssize_t)fread(buffer, 1, size, f) != size) {
        fprintf(stderr, "[FSM Compiler] Error reading from file '%s': %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    buffer[size] = '\0';

    contents->begin = buffer;
    contents->len = size;

    fclose(f);
}


bool debug_tokens = false;
bool debug_imports = false;

const char *debug_ast;

// filename at this point is relative to execution path.
void compile_module(const char *filename) {
    current_module = dyn_array_push(&modules);
    memset(current_module->filename, 0,1024);
    strncpy(current_module->filename, filename, 1023);

    current_filename = filename;
    SV input;
    read_file(&input, filename);
    tokenizer(&input);

    if(debug_tokens)
        dump_tokens();

    AST_node *ast = parse_program_ast();

    if (debug_ast && debug_ast[0] == '0') ast_dump_tree(ast);
    run_type_resolver(ast);
    if (debug_ast && debug_ast[0] == '1') ast_dump_tree(ast);
    resolver(ast);
    if (debug_ast && debug_ast[0] == '2') ast_dump_tree(ast);
    run_typechecking(ast);
    if (debug_ast && debug_ast[0] == '3') ast_dump_tree(ast);

    calculate_stacks(ast);
    current_module->ast = ast;
}

static SV get_path(const char *filename) {
    
    for (int i = strlen(filename) - 1; i >= 0; i--) {
        if (filename[i] == '/') {
            return (SV) {filename, i+1};
        }
    }
    return (SV) {filename, 0};
}

static bool find_file(SB *sb, SV name, const char *parent_module_filename) {
    // try current directory
    sb_printf(sb, "%.*s.fsm", SV_prnt(name));
    if (access(sb->buffer, R_OK) == 0) return true;

    // search the path of the parent module
    sb_reset(sb);
    SV path = get_path(parent_module_filename);
    sb_printf(sb, "%.*s%.*s.fsm", SV_prnt(path), SV_prnt(name));
    if (access(sb->buffer, R_OK) == 0) return true;

    // search standard library
    sb_reset(sb);
    sb_printf(sb, "stdlib/%.*s.fsm", SV_prnt(name));
    if (access(sb->buffer, R_OK) == 0) return true;

    return false;
}

static bool already_imported (const char *filename) {
    for (size_t i = 0; i < modules.count; i++) {
        Module *module = &((Module*)modules.data)[i];
        if (strcmp(module->filename, filename) == 0) return true;
    }
    return false;
}

int depth;

bool resolve_import (SV name) {
    SB sb; char buf_1 [1024]; sb_init(&sb, buf_1, 1024);

    const char *saved_filename = current_filename;
    Module *saved_module = current_module;

    bool ret = false;
    if (find_file(&sb, name, saved_module->filename)) {
        bool already = already_imported(sb.buffer);
        if (debug_imports) {
            for (int i = 0; i < depth; i++) fprintf(stderr, "    ");
            fprintf(stderr, "'%s': importing '%s' %s\n", saved_filename, sb.buffer, already ? "(already imported)" : "");
        }

        depth++;
        if (!already)
            compile_module(sb.buffer);
        depth--;
        
        ret = true;
    }

    current_filename = saved_filename;
    current_module = saved_module;
    return ret;
}

void compile_program(const char *filename) {
    dyn_array_init(&modules, sizeof(Module), 16);
    init_resolver();
    type_resolver_init();
    compile_module(filename);
}
