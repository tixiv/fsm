
#pragma once

#include "sv.h"
#include "tokenizer.h"
#include "type.h"

#define SYM_LIST \
    X(SYM_global) \
    X(SYM_local) \
    X(SYM_arg) \


typedef enum {
#define X(name) name,
    SYM_LIST
#undef X
} SymbolKind;

const char *symbol_kind_name(SymbolKind kind);

typedef struct {
    SV name;
    SymbolKind kind;
    Type *type;

    // TODO: move all of these or most to the type
    size_t size; // stack frame size for functions
    size_t offset; // stack offset for args and local vars
    int num_fn_args; // argument count for functions
    int num_fn_returns; // 1 or 0 for now
} Symbol;


#define AST_LIST \
    X(AST_program) \
    X(AST_function) \
    X(AST_scope) \
    X(AST_var_decl) \
    X(AST_arg_decl) \
    X(AST_number) \
    X(AST_string) \
    X(AST_symbol) \
    X(AST_binary) \
    X(AST_call) \
    X(AST_return) \
    X(AST_if) \
    X(AST_while) \
    X(AST_for) \

typedef enum {
#define X(name) name,
    AST_LIST
#undef X
} AST_kind;

const char *ast_kind_name(AST_kind kind);

typedef struct AST_node_s {
    struct AST_node_s *next;
    Type *type;
    bool result_used;
    union {
        struct {
            struct AST_node_s *left;
            struct AST_node_s *right;
            TokenKind token_kind;
        } binary;

        struct {
            struct AST_node_s *args;
            struct AST_node_s *body;
            Symbol *symbol;
            SV name;
        } fun;

        struct {
            struct AST_node_s *body;
        } scope;

        struct {
            struct AST_node_s *body;
        } program;

        struct {
            struct AST_node_s *args;
            Symbol *symbol;
            SV name;
        } call;

        struct {
            struct AST_node_s *return_val;
        } ret;

        struct {
            struct AST_node_s *initializer;
            Symbol *symbol;
            SV name;
        } var_decl;

        struct {
            SV name;
            Symbol *symbol;
        } symbol;

        struct {
            SV value;
        } number;

        struct {
            SV value;
        } str;

        struct {
            struct AST_node_s *condition;
            struct AST_node_s *if_clause;
            struct AST_node_s *else_clause;
        } _if;

        struct {
            struct AST_node_s *condition;
            struct AST_node_s *body;
        } _while;

        struct {
            struct AST_node_s *initializer;
            struct AST_node_s *condition;
            struct AST_node_s *post_action;
            struct AST_node_s *body;
        } _for;
    };
    AST_kind kind;
    int line_number;
} AST_node;

AST_node *ast_alloc(AST_kind kind, int line_number);

void ast_dump_tree (AST_node *root);



typedef void (*AstVisitor)(AST_node *, void *);

void ast_visit_chain(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg);
void ast_visit_children(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg);
