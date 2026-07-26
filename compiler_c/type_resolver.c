
#include "type_resolver.h"
#include "ast.h"
#include "common.h"
#include "dyn_array.h"
#include "sv.h"
#include "type.h"
#include "symbol.h"
#include <stdarg.h>
#include <stddef.h>
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

Dyn_array named_types;

Type *get_named_type(size_t index) {
    ASSERT(index < named_types.count, "Named type index out of bounds.\n");
    return ((void**)named_types.data)[index];
}

Type *get_type_by_name(const SV *name) {
    if (sv_compare_cstr(name,"void")) return &builtin_void;
    if (sv_compare_cstr(name,"any")) return &builtin_any;
    if (sv_compare_cstr(name,"bool")) return &builtin_bool;
    if (sv_compare_cstr(name,"u64")) return &builtin_u64;
    if (sv_compare_cstr(name,"i64")) return &builtin_i64;
    if (sv_compare_cstr(name,"u32")) return &builtin_u32;
    if (sv_compare_cstr(name,"i32")) return &builtin_i32;
    if (sv_compare_cstr(name,"u16")) return &builtin_u16;
    if (sv_compare_cstr(name,"i16")) return &builtin_i16;
    if (sv_compare_cstr(name,"u8"))  return &builtin_u8;
    if (sv_compare_cstr(name,"i8"))  return &builtin_i8;

    for (int i = 0; i < named_types.count; i++) {
        Type *t = get_named_type(i);
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

void push_named_type(SV name, Type * t, int line_number) {
    if (get_type_by_name(&name)) {
        type_resolver_error(line_number, "Tried to redefine type '%.*s'\n", SV_prnt(name));
    }

    t->name = name;
    dyn_array_push_p(&named_types, t);

    Symbol *s = alloc_symbol(SYM_type, name);
    s->name = name;
    s->type = t;

    type_resolver_push_symbol(s, line_number);
}

void type_lookup_visitor(AST_node *n, void *arg) {
    switch (n->kind) {
        case AST_struct:
            if (n->_struct.is_record)
                push_named_type(n->_struct.name, type_alloc(T_record), n->line_number);
            else
                push_named_type(n->_struct.name, type_alloc(T_struct), n->line_number);
            break;
        
        case AST_enum: {
            Type * t =  type_alloc(T_enum);
            t->storage_size = 8;
            push_named_type(n->_enum.name, t, n->line_number);
            break;
        }

        case AST_union:
            push_named_type(n->_union.name, type_alloc(T_union), n->line_number);
            break;

        default:
            ast_visit_children(n, type_lookup_visitor, arg);
            break;
    }
}

typedef struct {
    Type *current_enum;
    Dyn_array enum_members;   // DynArray<EnumMember>
} EnumResolverState;

typedef struct {
    Type *current_struct;
    Dyn_array struct_members; // DynArray<TypeMember>
} StructResolverState;

typedef struct {
    Type *current_union;
    Dyn_array union_members; // DynArray<UnionMember>
} UnionResolverState;

void copy_struct_members(StructResolverState *trs){
    size_t size = trs->struct_members.count * sizeof(StructMember);
    trs->current_struct->_struct.members = malloc(size);
    memcpy(trs->current_struct->_struct.members, trs->struct_members.data, size);
    trs->current_struct->_struct.num_members = trs->struct_members.count;
    calculate_storage_size(trs->current_struct);
}

void copy_enum_members(EnumResolverState *trs){
    size_t size = trs->enum_members.count * sizeof(EnumMember);
    trs->current_enum->_enum.members = malloc(size);
    memcpy(trs->current_enum->_enum.members, trs->enum_members.data, size);
    trs->current_enum->_enum.num_members = trs->enum_members.count;
}

void copy_union_members(UnionResolverState *trs){
    size_t size = trs->union_members.count * sizeof(UnionMember);
    trs->current_union->_union.members = malloc(size);
    memcpy(trs->current_union->_union.members, trs->union_members.data, size);
    trs->current_union->_union.num_members = trs->union_members.count;
}

void type_resolver_visitor(AST_node *n, void *);
void type_resolver_struct_visitor(AST_node *n, StructResolverState *trs);

void type_resolve_struct(AST_node *n, bool global) {
    StructResolverState trs_1;
    dyn_array_init(&trs_1.struct_members, sizeof(StructMember), 8);
    if (global) {
        n->type = get_type_by_name(&n->_struct.name);
        ASSERT(n->type, "The type name '%.*s' should exist because it should have been found in the lookup pass.\n", SV_prnt(n->_struct.name));
    }
    else {
        n->type = type_alloc(n->_struct.is_record ? T_record : T_struct);
    }
    trs_1.current_struct = n->type;
    ast_visit_children(n, (AstVisitor)type_resolver_struct_visitor, &trs_1);
    copy_struct_members(&trs_1);
}

void type_resolver_enum_visitor(AST_node *n, EnumResolverState *trs) {
    switch (n->kind) {
        case AST_enum_member: {
            ASSERT(trs->current_enum, "Encountered %s outside of enum.\n", ast_kind_name(n->kind));
            EnumMember *member = dyn_array_push(&trs->enum_members);
            member->name = n->_enum_member.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            member->value = n->_enum_member.value;
            n->type = &builtin_i64;
            break;
        }
    
        default:
            NOT_IMPLEMENTED("type_resolver_enum_visitor() is not implemented for %s.\n", ast_kind_name(n->kind));
            break;
    }
}

void type_resolver_struct_visitor(AST_node *n, StructResolverState *trs) {
    switch (n->kind) {
        case AST_member_def: {
            ASSERT(trs->current_struct, "Encountered %s outside of struct.\n", ast_kind_name(n->kind));
            StructMember *member = dyn_array_push(&trs->struct_members);
            member->name = n->struct_member_def.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            if (n->struct_member_def._typedef) {
                if (!n->struct_member_def._typedef->type) type_resolver_error(n->line_number, "The type for struct member '%.*s' could not be resolved.\n", SV_prnt(member->name));
                n->type = n->struct_member_def._typedef->type;
            }
            else {
                n->type = &builtin_i64;
            }
            member->type = n->type;
            break;
        }

        default:
            NOT_IMPLEMENTED("type_resolver_struct_visitor() is not implemented for %s.\n", ast_kind_name(n->kind));
            break;
    }
}

void type_resolver_union_visitor(AST_node *n, UnionResolverState *trs) {
    switch (n->kind) {
        case AST_union_member_def: {
            UnionMember *member = dyn_array_push(&trs->union_members);
            member->name = n->union_member_def.name;
            ast_visit_children(n, (AstVisitor)type_resolver_union_visitor, nullptr);
            if (n->union_member_def._typedef) {
                if (!n->union_member_def._typedef->type) type_resolver_error(n->line_number, "The type for union member '%.*s' could not be resolved.\n", SV_prnt(member->name));
                n->type = n->union_member_def._typedef->type;
            }
            else {
                n->type = &builtin_i64;
            }
            member->type = n->type;
            member->value = n->union_member_def.enum_value;
            break;
        }

        case AST_struct: {
            type_resolve_struct(n, false);
            break;
        }

        default:
            NOT_IMPLEMENTED("type_resolver_union_visitor() is not implemented for %s.\n", ast_kind_name(n->kind));
            break;
    }
}

void type_resolver_visitor(AST_node *n, void *arg) {
    switch (n->kind) {
        case AST_struct:
            type_resolve_struct(n, true);
            break;

        case AST_enum: {
            EnumResolverState trs_1;
            dyn_array_init(&trs_1.enum_members, sizeof(EnumMember), 8);
            n->type = get_type_by_name(&n->_enum.name);
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            trs_1.current_enum = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_enum_visitor, &trs_1);
            copy_enum_members(&trs_1);
            break;
        }

        case AST_union: {
            UnionResolverState trs_1;
            dyn_array_init(&trs_1.union_members, sizeof(UnionMember), 8);
            n->type = get_type_by_name(&n->_union.name);
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            trs_1.current_union = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_union_visitor, &trs_1);
            copy_union_members(&trs_1);
            break;
        }

        case AST_typename: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            n->type = get_type_by_name(&n->_typename.name);
            if (!n->type) type_resolver_error(n->line_number, "The typename '%.*s' could not be resolved.\n", SV_prnt(n->_typename.name));
            break;
        }

        case AST_function_type: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);

            size_t num_arguments = ast_count_chain(n->_function_type.function_args);
            Type**args = malloc(sizeof(Type*) * num_arguments);
            AST_node *arg = n->_function_type.function_args;
            for (size_t i = 0; i < num_arguments; i++) {
                args[i] = arg->type;
                arg = arg->next;
            }

            n->type = get_ref_type_for(get_function_type(n->_function_type.function_ret->type, args, num_arguments));
            break;
        }

        case AST_type_ref:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            n->type = get_ref_type_for(n->_type_ref.body->type);
            break;

        case AST_type_array:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            n->type = get_array_type(n->_type_array.body->type, n->_type_array.n_elements);
            break;

        case AST_type_slice: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            n->type = get_sclice_type(n->_type_slice.body->type);
            break;
        }

        default:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, nullptr);
            break;
            
    }
}

void run_type_resolver(AST_node *root) {
    type_lookup_visitor(root, nullptr);
    type_resolver_visitor(root, nullptr);
}

void type_resolver_init() {
    dyn_array_init(&named_types, sizeof(void*), 32);
}
