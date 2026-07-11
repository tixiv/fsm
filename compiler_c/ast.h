
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

    size_t size; // stack frame size for functions
    size_t offset; // stack offset for args and local vars
} Symbol;


#define AST_LIST \
    X(AST_program) \
    X(AST_function) \
    X(AST_arg_list) \
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
    X(AST_cast) \
    X(AST_array_access) \
    X(AST_array_len) \
    X(AST_dereference) \
    X(AST_reference) \
    X(AST_struct) \
    X(AST_member_def) \
    X(AST_typename) \
    X(AST_type_ref) \
    X(AST_type_array) \
    X(AST_type_slice) \
    X(AST_member_access) \
    X(AST_array_to_slice) \

typedef enum {
#define X(name) name,
    AST_LIST
#undef X
} AST_kind;

const char *ast_kind_name(AST_kind kind);

typedef struct AST_node_s {
    struct AST_node_s *next;
    AST_kind kind;
    int line_number;
    Type *type;
    bool addressable;
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
            struct AST_node_s *ret_typedecl;
            Symbol *symbol;
            SV name;
        } fun;

        struct {
            struct AST_node_s *body;
        } arg_list;

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
            bool implicit;
        } ret;

        struct {
            struct AST_node_s *initializer;
            struct AST_node_s *_typedecl;
            Symbol *symbol;
            SV name;
            TokenKind initializer_operator;
        } var_decl;

        struct {
            struct AST_node_s *_typedecl;
            Symbol *symbol;
            SV name;
        } arg_decl;

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

        struct {
            struct AST_node_s *body;
            Type *right_type; // the left type is the type of the AST node
        } _cast;

        struct {
            struct AST_node_s *body;
        } _array_to_slice;

        struct {
            struct AST_node_s *array;
            struct AST_node_s *index;
        } _array;

        struct {
            struct AST_node_s *body;
        } deref;

        struct {
            struct AST_node_s *body;
        } reference;

        struct {
            struct AST_node_s *body;
            SV name;
        } _struct;

        struct {
            struct AST_node_s *_typedef;
            SV name;
        } member_def;

        struct {
            struct AST_node_s *body;
            SV name;
            size_t offset;                
        } member_access;

        struct {
            size_t len;
        } array_len;

        struct {
            SV name;
        } _typename;

        struct {
            struct AST_node_s *body;
        } _type_ref;

        struct {
            struct AST_node_s *body;
            size_t n_elements;
        } _type_array;

        struct {
            struct AST_node_s *body;
        } _type_slice;
    };
} AST_node;

AST_node *ast_alloc(AST_kind kind, int line_number);

AST_node *get_last_in_chain(AST_node *n);
void ast_insert_node(AST_node **at, AST_node *new_node);
void ast_remove_node(AST_node *n);

typedef void (*AstVisitor)(AST_node *, void *);

void ast_visit_chain(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg);
void ast_visit_children(AST_node *n, void (*visit)(AST_node *, void *arg), void *arg);

void ast_dump_tree (AST_node *root);
