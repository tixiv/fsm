
#include "type_resolver.h"
#include "ast.h"
#include "common.h"
#include "dyn_array.h"
#include "sv.h"
#include "type.h"
#include "symbol.h"
#include <stdarg.h>
#include <string.h>

static void type_resolver_error(int line_number, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[FSM Type Resolver] %s:%d Error: ", current_filename, line_number);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

Dyn_array dyn_structs;

Type *get_struct(size_t index) {
    ASSERT(index < dyn_structs.count, "Tried to access struct that doesn't exist.\n");
    return ((void**)dyn_structs.data)[index];
}

Dyn_array dyn_enums;

Type *get_enum(size_t index) {
    ASSERT(index < dyn_enums.count, "Tried to access enum that doesn't exist.\n");
    return ((void**)dyn_enums.data)[index];
}

Type *get_type_by_name(const SV *name) {
    if (sv_compare_cstr(name,"void")) return &builtin_void;
    if (sv_compare_cstr(name,"any")) return &builtin_any;
    if (sv_compare_cstr(name,"u64")) return &builtin_u64;
    if (sv_compare_cstr(name,"i64")) return &builtin_i64;
    if (sv_compare_cstr(name,"u32")) return &builtin_u32;
    if (sv_compare_cstr(name,"i32")) return &builtin_i32;
    if (sv_compare_cstr(name,"u16")) return &builtin_u16;
    if (sv_compare_cstr(name,"i16")) return &builtin_i16;
    if (sv_compare_cstr(name,"u8"))  return &builtin_u8;
    if (sv_compare_cstr(name,"i8"))  return &builtin_i8;

    for (int i = 0; i < dyn_structs.count; i++) {
        Type *t = get_struct(i);
        if (sv_equal(&t->name, name))
            return t;
    }

    for (int i = 0; i < dyn_enums.count; i++) {
        Type *t = get_enum(i);
        if (sv_equal(&t->name, name))
            return t;
    }

    return nullptr;
}

static void type_resolver_push_symbol(Symbol *s, int line_number) {
    Symbol *conflicting = get_symbol_by_name(&global_symbols,  &s->name);
    if (conflicting) {
        type_resolver_error(line_number, "Symbol '%.*s' redefined\n", SV_prnt(s->name));
        return;
    }

    dyn_array_push_p(&global_symbols, s);
}

void push_struct(SV name, int line_number) {
    if (get_type_by_name(&name)) {
        type_resolver_error(line_number, "Tried to redefine type '%.*s'\n", SV_prnt(name));
    }

    Type *t = type_alloc(T_struct);
    t->name = name;
    dyn_array_push_p(&dyn_structs, t);

    Symbol *s = alloc_symbol(SYM_type, name);
    s->name = name;
    s->type = t;

    type_resolver_push_symbol(s, line_number);
}

void push_enum(SV name, int line_number) {
    if (get_type_by_name(&name)) {
        type_resolver_error(line_number, "Tried to redefine type '%.*s'\n", SV_prnt(name));
    }

    Type *t = type_alloc(T_enum);
    t->name = name;
    t->storage_size = 8;
    dyn_array_push_p(&dyn_enums, t);

    Symbol *s = alloc_symbol(SYM_type, name);
    s->name = name;
    s->type = t;

    type_resolver_push_symbol(s, line_number);
}

void type_lookup_visitor(AST_node *n, void *arg) {
    switch (n->kind) {
        case AST_struct:
            push_struct(n->_struct.name, n->line_number);
            break;
        
        case AST_enum:
            push_enum(n->_enum.name, n->line_number);
            break;
        
        default:
            ast_visit_children(n, type_lookup_visitor, arg);
            break;
    }
}

typedef struct {
    Type *current_struct;
    Dyn_array struct_members; // DynArray<TypeMember>
    Type *current_enum;
    Dyn_array enum_members;   // DanArray<EnumMember>
} TypeResolverState;

void copy_struct_members(TypeResolverState *trs){
    size_t size = trs->struct_members.count * sizeof(TypeMember);
    trs->current_struct->_struct.members = malloc(size);
    memcpy(trs->current_struct->_struct.members, trs->struct_members.data, size);
    trs->current_struct->_struct.num_members = trs->struct_members.count;
    calculate_storage_size(trs->current_struct);
}

void copy_enum_members(TypeResolverState *trs){
    size_t size = trs->enum_members.count * sizeof(EnumMember);
    trs->current_enum->_enum.members = malloc(size);
    memcpy(trs->current_enum->_enum.members, trs->enum_members.data, size);
    trs->current_enum->_enum.num_members = trs->enum_members.count;
}

void type_resolver_visitor(AST_node *n, TypeResolverState *trs) {
    switch (n->kind) {
        case AST_struct: {
            TypeResolverState trs_1;
            dyn_array_init(&trs_1.struct_members, sizeof(TypeMember), 8);
            n->type = get_type_by_name(&n->_struct.name);
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            trs_1.current_struct = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, &trs_1);
            copy_struct_members(&trs_1);
            break;
        }

        case AST_member_def: {
            ASSERT(trs->current_struct, "Encountered %s outside of struct.\n", ast_kind_name(n->kind));
            TypeMember *member = dyn_array_push(&trs->struct_members);
            member->name = n->member_def.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            if (n->member_def._typedef) {
                if (!n->member_def._typedef->type) type_resolver_error(n->line_number, "The type for struct member '%.*s' could not be resolved.\n", SV_prnt(member->name));
                n->type = n->member_def._typedef->type;
            }
            else {
                n->type = &builtin_i64;
            }
            member->type = n->type;
            break;
        }

        case AST_enum: {
            TypeResolverState trs_1;
            dyn_array_init(&trs_1.enum_members, sizeof(EnumMember), 8);
            n->type = get_type_by_name(&n->_enum.name);
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            trs_1.current_enum = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, &trs_1);
            copy_enum_members(&trs_1);
            break;
        }

        case AST_enum_member: {
            ASSERT(trs->current_enum, "Encountered %s outside of enum.\n", ast_kind_name(n->kind));
            EnumMember *member = dyn_array_push(&trs->enum_members);
            member->name = n->_enum_member.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            member->value = n->_enum_member.value;
            n->type = &builtin_i64;
            break;
        }

        case AST_typename: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_type_by_name(&n->_typename.name);
            if (!n->type) type_resolver_error(n->line_number, "The typename '%.*s' could not be resolved.\n", SV_prnt(n->_typename.name));
            break;
        }

        case AST_type_ref:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_ref_type_for(n->_type_ref.body->type);
            break;

        case AST_type_array:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_array_type(n->_type_array.body->type, n->_type_array.n_elements);
            break;

        case AST_type_slice: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_sclice_type(n->_type_slice.body->type);
            break;
        }

        default:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            break;
            
    }
}

TypeResolverState trs;

void run_type_resolver(AST_node *root) {
    dyn_array_init(&dyn_structs, sizeof(void*), 32);
    dyn_array_init(&dyn_enums, sizeof(void*), 32);

    type_lookup_visitor(root, nullptr);
    type_resolver_visitor(root, &trs);
}