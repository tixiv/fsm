
#pragma once

#include "sv.h"

#define AST_LIST \
    X(AST_function) \
    X(AST_var_decl) \
    X(AST_number) \
    X(AST_string) \
    X(AST_var) \
    X(AST_block) \
    X(AST_symbol) \
    X(AST_binary) \
    X(AST_call) \
    X(AST_return) \
    X(AST_if) \
    X(AST_while) \

typedef enum {
#define X(name) name,
    AST_LIST
#undef X
} AST_kind;

typedef struct AST_node_s {
    struct AST_node_s *next;
    union {
        struct {
            struct AST_node_s *left;
            struct AST_node_s *right;
            int token_kind;
        } binary;

        struct {
            struct AST_node_s *args;
            struct AST_node_s *body;
            SV name;
        } fun;

        struct {
            struct AST_node_s *body;
        } scope;

        struct {
            struct AST_node_s *args;
            SV name;
        } call;

        struct {
            struct AST_node_s *return_val;
        } ret;

        struct {
            struct AST_node_s *initializer;
            SV name;
        } var;

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
    };
    AST_kind kind;
    int line_number;
} AST_node;

AST_node *ast_alloc(AST_kind kind, int line_number);

void ast_dump_tree (AST_node *root);

/*

fn main()
{
    let x = 42;
    print(x + 1);
}

Function "main"
└── Block
    ├── VariableDecl "x"
    │   └── IntegerLiteral 42
    └── Call
        ├── Identifier "print"
        └── Binary(+)
            ├── Identifier "x"
            └── IntegerLiteral 1
            */