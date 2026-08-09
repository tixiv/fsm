
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

static void type_resolver_error(const Location *location, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);

    ASSERT(location, "Tried to report error with null location.\n");

    fprintf(stderr, "[FSM Type Resolver] %s:%d Error: ", location->filename, location->line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    exit(EXIT_FAILURE);
    va_end(args);
}

Type *get_builtin_type_by_name(const SV *name) {
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

    return nullptr;
}

Type *get_type_by_name(SV *name, const Location *location) {
    Type *t = get_builtin_type_by_name(name);
    if (t) return t;

    Symbol *sym = get_symbol_by_name(&global_symbols, name);

    if (sym) {
        if (sym->kind != SYM_type) {
            type_resolver_error(location, "'%.*s' is not a type symbol.", SV_prnt(*name));
        }
        return sym->type;
    }

    return nullptr;
}

static void type_resolver_push_symbol(Symbol *s, const Location *location) {
    Symbol *conflicting = get_symbol_by_name(&global_symbols,  &s->name);
    if (conflicting) {
        type_resolver_error(location, "Symbol '%.*s' redefined\n", SV_prnt(s->name));
        return;
    }

    dyn_array_push_p(&global_symbols, s);
}

void push_named_type(SV name, Type * t, const Location *location) {
    Symbol *sym = get_symbol_by_name(&global_symbols, &name);

    if (sym) {
        type_resolver_error(location, "Tried to redefine type '%.*s' which was previously defined in %s:%d\n", SV_prnt(name), sym->location->filename, sym->location->line);
    }

    if (get_builtin_type_by_name(&name)) {
        type_resolver_error(location, "Tried to redefine builtin type '%.*s'", SV_prnt(name));
    }

    t->name = name;

    Symbol *s = alloc_symbol(SYM_type, name, location);
    s->name = name;
    s->type = t;

    type_resolver_push_symbol(s, location);
}

void type_lookup_visitor(AST_node *n, void *) {
    switch (n->kind) {
        case AST_struct: {
            Type *t = type_alloc(T_struct);
            t->_struct.kind = SL_struct;
            if (n->_struct.is_record)
                t->_struct.kind = SL_record;
            push_named_type(n->_struct.name, t, n->location);
            break;
        }
        
        case AST_enum: {
            Type * t =  type_alloc(T_enum);
            t->storage_size = 8;
            push_named_type(n->_enum.name, t, n->location);
            break;
        }

        case AST_union:
            push_named_type(n->_union.name, type_alloc(T_union), n->location);
            break;

        default:
            ast_visit_children(n, type_lookup_visitor, nullptr);
            break;
    }
}

typedef struct {
    SV generic_parameter_name;
    Type *generic_parameter_type;
} TypeResolverState;

typedef struct {
    TypeResolverState *trs;
    Type *current_enum;
    Dyn_array enum_members;   // DynArray<EnumMember>
} EnumResolverState;

typedef struct {
    TypeResolverState *trs;
    Type *current_struct;
    Dyn_array struct_members; // DynArray<TypeMember>
} StructResolverState;

typedef struct {
    TypeResolverState *trs;
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
    calculate_storage_size(trs->current_union);
}

void type_resolver_visitor(AST_node *n, TypeResolverState *trs);
void type_resolver_struct_visitor(AST_node *n, StructResolverState *trs);

void type_resolve_struct(AST_node *n, TypeResolverState *trs, bool global) {
    StructResolverState srs;
    srs.trs = trs;
    dyn_array_init(&srs.struct_members, sizeof(StructMember), 8);
    if (global) {
        n->type = get_type_by_name(&n->_struct.name, n->location);
        ASSERT(n->type, "The type name '%.*s' should exist because it should have been found in the lookup pass.\n", SV_prnt(n->_struct.name));
    }
    else {
        n->type = type_alloc(T_struct);
        n->type->_struct.kind = n->_struct.is_record ? SL_record : SL_struct;
    }
    srs.current_struct = n->type;
    ast_visit_children(n, (AstVisitor)type_resolver_struct_visitor, &srs);
    copy_struct_members(&srs);
}

void type_resolver_enum_visitor(AST_node *n, EnumResolverState *ers) {
    switch (n->kind) {
        case AST_enum_member: {
            ASSERT(ers->current_enum, "Encountered %s outside of enum.\n", ast_kind_name(n->kind));
            EnumMember *member = dyn_array_push(&ers->enum_members);
            member->name = n->_enum_member.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, ers->trs);
            member->value = n->_enum_member.value;
            n->type = &builtin_i64;
            break;
        }
    
        default:
            NOT_IMPLEMENTED("type_resolver_enum_visitor() is not implemented for %s.\n", ast_kind_name(n->kind));
            break;
    }
}

void type_resolver_struct_visitor(AST_node *n, StructResolverState *srs) {
    switch (n->kind) {
        case AST_member_def: {
            ASSERT(srs->current_struct, "Encountered %s outside of struct.\n", ast_kind_name(n->kind));
            StructMember *member = dyn_array_push(&srs->struct_members);
            member->name = n->struct_member_def.name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, srs->trs);
            if (n->struct_member_def._typedef) {
                if (!n->struct_member_def._typedef->type) type_resolver_error(n->location, "The type for struct member '%.*s' could not be resolved.\n", SV_prnt(member->name));
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

void type_resolver_union_visitor(AST_node *n, UnionResolverState *urs) {
    switch (n->kind) {
        case AST_union_member_def: {
            UnionMember *member = dyn_array_push(&urs->union_members);
            member->name = n->union_member_def.name;
            ast_visit_children(n, (AstVisitor)type_resolver_union_visitor, urs);
            if (n->union_member_def._typedef) {
                if (!n->union_member_def._typedef->type) type_resolver_error(n->location, "The type for union member '%.*s' could not be resolved.\n", SV_prnt(member->name));
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
            type_resolve_struct(n, urs->trs, false);
            break;
        }

        default:
            NOT_IMPLEMENTED("type_resolver_union_visitor() is not implemented for %s.\n", ast_kind_name(n->kind));
            break;
    }
}

void type_resolver_visitor(AST_node *n, TypeResolverState *trs) {
    switch (n->kind) {
        case AST_struct:
            type_resolve_struct(n, trs, true);
            break;

        case AST_enum: {
            EnumResolverState ers;
            dyn_array_init(&ers.enum_members, sizeof(EnumMember), 8);
            n->type = get_type_by_name(&n->_enum.name, n->location);
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            ers.current_enum = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_enum_visitor, &ers);
            copy_enum_members(&ers);
            break;
        }

        case AST_union: {
            UnionResolverState urs;
            urs.trs = trs;
            dyn_array_init(&urs.union_members, sizeof(UnionMember), 8);
            n->type = get_type_by_name(&n->_union.name, n->location);
            n->type->_union.enumarator_name = n->_union.enumerator_name;
            ASSERT(n->type, "The type name should exist because it should have been found in the lookup pass.\n");
            urs.current_union = n->type;
            ast_visit_children(n, (AstVisitor)type_resolver_union_visitor, &urs);
            copy_union_members(&urs);
            break;
        }

        case AST_typename: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            if (trs->generic_parameter_name.begin && sv_equal(&trs->generic_parameter_name, &n->_typename.name)) {
                if (trs->generic_parameter_type)
                    n->type = trs->generic_parameter_type;
                else
                    n->type = &builtin_generic;
            }
            else
                n->type = get_type_by_name(&n->_typename.name, n->location);
            if (!n->type) type_resolver_error(n->location, "The typename '%.*s' could not be resolved.\n", SV_prnt(n->_typename.name));
            break;
        }

        case AST_function_type: {
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);

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
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_ref_type_for(n->_type_ref.body->type);
            break;

        case AST_type_array:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_array_type(n->_type_array.body->type, n->_type_array.n_elements);
            break;

        case AST_type_slice:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_slice_type(n->_type_slice.body->type);
            break;

        case AST_type_generic_speciation:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            n->type = get_speciated_type(n->type_generic_speciation.body->type, n->type_generic_speciation.typedecl->type);
            break;

        case AST_generic:
            trs->generic_parameter_name = n->generic.parameter_name;
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            trs->generic_parameter_name = (SV) {nullptr, 0};
            break;


        default:
            ast_visit_children(n, (AstVisitor)type_resolver_visitor, trs);
            break;
            
    }
}

void type_resolver_speciate_generic(AST_node *root, Type *t) {
    TypeResolverState trs = {0};
    trs.generic_parameter_name = (SV) {nullptr, 0};
    trs.generic_parameter_type = t;
    type_resolver_visitor(root, &trs);
}


void run_type_resolver(AST_node *root) {
    TypeResolverState trs = {0};
    trs.generic_parameter_name = (SV) {nullptr, 0};
    type_lookup_visitor(root, nullptr);
    type_resolver_visitor(root, &trs);
}
