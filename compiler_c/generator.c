
#include "generator.h"
#include "dyn_array.h"
#include "opcodes.h"
#include "sv.h"
#include "common.h"
#include "type.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static size_t unescaped_string_len(SV str) {
    size_t len = 0;
    while (str.len) {
        if (*str.begin == '\\') sv_pop(&str);
        sv_pop(&str);
        len++;
    }
    return len;
}

uint8_t get_unescaped_char(uint8_t c) {
    switch (c) {
        case '\\': return '\\'; break;
        case '{':  return '{'; break;
        case '}':  return '}'; break;
        case 'a':  return '\a'; break;
        case 'b':  return '\b'; break;
        case 'e':  return '\e'; break;
        case 'n':  return '\n'; break;
        case 'r':  return '\r'; break;
        case 't':  return '\t'; break;
        case '"':  return '"'; break;
        case '0':  return 0; break;
        case '\'':  return '\''; break;
        default:
            NOT_IMPLEMENTED("Genearating assembly for the escape sequence '\\%c' = %d is not implemented yet.\n", c, c);
            break;
    }
}

int32_t get_char_constant(SV str) {
    int32_t c;
    if (str.len && *str.begin == '\\') {
        sv_pop(&str);
        c = get_unescaped_char(sv_pop(&str));
    } 
    else {
        c = sv_pop(&str);
    }

    return c;
}

const char *make_movx(const char*reg, size_t size, bool _sigend) {
    static char buf[20];
    char sz = _sigend ? 's' : 'z';

    switch (size) {
        case 8: sprintf(buf, "mov %s,", reg); break;
        case 4:
            if (_sigend) sprintf(buf, "movsxd %s, DWORD ", reg);
            else         sprintf(buf, "mov e%s, ", reg+1); // moving to eRx zero extends into rRx automatically
            break;
        case 2: sprintf(buf, "mov%cx %s, WORD ", sz, reg); break;
        case 1: sprintf(buf, "mov%cx %s, BYTE ", sz, reg); break;
        default: NOT_IMPLEMENTED("make_movx is not implemented for size %lu\n", size);
    }
    return buf;
}

void output_comaprison(FILE *file, Opcode *op) {
    fprintf(file, "\t" "mov rcx, 0\n");
    fprintf(file, "\t" "mov rdx, 1\n");
    fprintf(file, "\t" "pop rbx\n");
    fprintf(file, "\t" "pop rax\n");
    
    if (op->i64_value[0]) { // chained
        if (op->kind == OP_equal || op->kind == OP_unequal) {
            fprintf(file, "\t" "push rax\n"); // chaining value: left operand
        }
        else {
            fprintf(file, "\t" "push rbx\n"); // chaining value: right operand
        }
    }

    if (is_float_kind(op->type)) {
        fprintf(file, "\t" "movq xmm0, rax\n");
        fprintf(file, "\t" "movq xmm1, rbx\n");
        fprintf(file, "\t" "ucomisd xmm0, xmm1\n");
    }
    else if (is_integer_kind(op->type) || is_enum_kind(op->type) || is_enumerator_kind(op->type)
             || is_boolean_kind(op->type) || is_reference_kind(op->type))
    {
        fprintf(file, "\t" "cmp rax, rbx\n");
    }
    else {
        char buf [1024];
        NOT_IMPLEMENTED("Generating comparison for type '%s' is not implemented.\n", get_type_name_r(buf, op->type));
    }

    if      (op->kind == OP_equal)      {  fprintf(file, "\t" "cmove  rcx, rdx\n"); }
    else if (op->kind == OP_unequal)    {  fprintf(file, "\t" "cmovne rcx, rdx\n"); }
    else if (is_signed_integer(op->type)) {
        if      (op->kind == OP_compare_GT) {  fprintf(file, "\t" "cmovg  rcx, rdx\n"); }
        else if (op->kind == OP_compare_LT) {  fprintf(file, "\t" "cmovl  rcx, rdx\n"); }
        else if (op->kind == OP_compare_GE) {  fprintf(file, "\t" "cmovge rcx, rdx\n"); }
        else if (op->kind == OP_compare_LE) {  fprintf(file, "\t" "cmovle rcx, rdx\n"); }
        else ASSERT(false, "Not a comparison opcode.\n");
    }
    else {
        if      (op->kind == OP_compare_GT) {  fprintf(file, "\t" "cmova  rcx, rdx\n"); }
        else if (op->kind == OP_compare_LT) {  fprintf(file, "\t" "cmovb  rcx, rdx\n"); }
        else if (op->kind == OP_compare_GE) {  fprintf(file, "\t" "cmovae rcx, rdx\n"); }
        else if (op->kind == OP_compare_LE) {  fprintf(file, "\t" "cmovbe rcx, rdx\n"); }
        else ASSERT(false, "Not a comparison opcode.\n");
        // if (is_float_kind(op->type)) {
            // setp   dl
        // }
    }

    fprintf(file, "\t" "push rcx\n");
}

extern const char *builtin_functions_asm;

void output_asm(const char *asm_file_name) {
    FILE *file = fopen(asm_file_name, "w");

    // The print function and some other assembly snippets are copied
    // from the Porth compiler https://gitlab.com/tsoding/porth which
    // was also the main inspiration for me to start this project.

    fprintf(file, "%s", builtin_functions_asm);
    
    for (size_t i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];
        size_t size = 0;
        if (op->kind == OP_call || op->kind == OP_icall) {
            size = get_function_arguments_size(op->type);
        }
        else if (op->type) {
            size = op->type->storage_size;
        }

        if (op->location)
            fprintf(file,"; ------- %s:%d %s ---------\n", op->location->filename, op->location->line, opcode_name(op->kind));
        else
            fprintf(file,"; ------- %s ---------\n", opcode_name(op->kind));

        switch(op->kind) {
            case OP_begin_fn: {
                size_t local_vars_size = op->i64_value[0];
                fprintf(file,"fn_" SV_FMT ":\n", SV_prnt(op->string_value));
                fprintf(file, "\t" "push rbp\n");
                fprintf(file, "\t" "mov rbp, rsp\n");
                if (local_vars_size) {
                    ASSERT(local_vars_size % 8 == 0, "Can't work with local var sizes not divisible by 8.\n");
                    fprintf(file, "\t" "sub rsp, %lu\n", local_vars_size);
                    fprintf(file, "\t" "mov rcx, %lu\n", local_vars_size / 8);
                    fprintf(file, "\t" "lea rdi, [rbp - %lu]\n", local_vars_size);
                    fprintf(file, "\t" "xor rax, rax\n");
                    fprintf(file, "\t" "rep stosq\n");
                }
                break;
            }
            case OP_return:
                if      (size == 0);
                else if (size <= 8) fprintf(file, "\t" "pop rax\n");
                else if (size <= 16) {
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "pop rdx\n");
                } else NOT_IMPLEMENTED("Generating OP_return with storage size %lu is not implemented yet.\n", size)
                fprintf(file, "\t" "mov rsp, rbp\n");
                fprintf(file, "\t" "pop rbp\n");
                fprintf(file, "\t" "ret\n");
                break;
            case OP_add:
                if (is_integer_kind(op->type)) {
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "add [rsp], rax\n");
                } else if (is_float_kind(op->type)) {
                    fprintf(file, "\t" "movsd xmm0, [rsp+8]\n");
                    fprintf(file, "\t" "movsd xmm1, [rsp]\n");
                    fprintf(file, "\t" "addsd xmm0, xmm1\n");
                    fprintf(file, "\t" "add rsp, 8\n");
                    fprintf(file, "\t" "movsd [rsp], xmm0\n");
                } else {
                    NOT_IMPLEMENTED("%s for other than integer or float is not implemented.\n", opcode_name(op->kind))
                }
                break;
            case OP_sub:
                if (is_integer_kind(op->type)) {
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "sub [rsp], rax\n");
                } else if (is_float_kind(op->type)) {
                    fprintf(file, "\t" "movsd xmm0, [rsp+8]\n");
                    fprintf(file, "\t" "movsd xmm1, [rsp]\n");
                    fprintf(file, "\t" "subsd xmm0, xmm1\n");
                    fprintf(file, "\t" "add rsp, 8\n");
                    fprintf(file, "\t" "movsd [rsp], xmm0\n");
                } else {
                    NOT_IMPLEMENTED("%s for other than integer or float is not implemented.\n", opcode_name(op->kind))
                }
                break;
            case OP_mul:
                if (is_integer_kind(op->type)) {
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "mul QWORD [rsp]\n");
                    fprintf(file, "\t" "mov [rsp], rax\n");
                } else if (is_float_kind(op->type)) {
                    fprintf(file, "\t" "movsd xmm0, [rsp+8]\n");
                    fprintf(file, "\t" "movsd xmm1, [rsp]\n");
                    fprintf(file, "\t" "mulsd xmm0, xmm1\n");
                    fprintf(file, "\t" "add rsp, 8\n");
                    fprintf(file, "\t" "movsd [rsp], xmm0\n");
                } else {
                    NOT_IMPLEMENTED("%s for other than integer or float is not implemented.\n", opcode_name(op->kind))
                }
                break;
            case OP_div:
                if (is_integer_kind(op->type)) {
                    fprintf(file, "\t" "pop rbx\n");
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "cqo\n");
                    fprintf(file, "\t" "idiv rbx\n");
                    fprintf(file, "\t" "push rax\n");
                } else if (is_float_kind(op->type)) {
                    fprintf(file, "\t" "movsd xmm0, [rsp+8]\n");
                    fprintf(file, "\t" "movsd xmm1, [rsp]\n");
                    fprintf(file, "\t" "divsd xmm0, xmm1\n");
                    fprintf(file, "\t" "add rsp, 8\n");
                    fprintf(file, "\t" "movsd [rsp], xmm0\n");
                } else {
                    NOT_IMPLEMENTED("%s for other than integer or float is not implemented.\n", opcode_name(op->kind))
                }
                break;
            case OP_mod:
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cqo\n");
                fprintf(file, "\t" "idiv rbx\n");
                fprintf(file, "\t" "push rdx\n");
                break;
            case OP_ipow:
                fprintf(file, "rept 1 {\n");
                fprintf(file, "local loop\n");
                fprintf(file, "local skip_mul\n");
                fprintf(file, "local done\n");
                fprintf(file, "\t" "pop rsi\n");
                fprintf(file, "\t" "pop rdi\n");
                fprintf(file, "\t" "mov rax, 1\n");
                fprintf(file, "\t" "test rsi, rsi\n");
                fprintf(file, "\t" "jz done\n");
                fprintf(file, "loop:\n");
                fprintf(file, "\t" "test rsi, 1\n");
                fprintf(file, "\t" "jz skip_mul\n");
                fprintf(file, "\t" "imul rax, rdi\n");
                fprintf(file, "skip_mul:\n");
                fprintf(file, "\t" "imul rdi, rdi\n");
                fprintf(file, "\t" "shr rsi, 1\n");
                fprintf(file, "\t" "jnz loop\n");
                fprintf(file, "done:\n");
                fprintf(file, "\t" "push rax\n");
                fprintf(file, "}\n");
                break;
            case OP_to_bool:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "or rax, rax\n");
                fprintf(file, "\t" "cmovnz rcx, rdx\n");
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_not:
                fprintf(file, "\t" "xor QWORD [rsp], 1\n");
                break;
            case OP_neg:
                if (is_float_kind(op->type)) {
                    fprintf(file, "\t" "mov rax, 0x8000000000000000\n");
                    fprintf(file, "\t" "xor QWORD [rsp], rax\n");
                }
                else {
                    fprintf(file, "\t" "neg QWORD [rsp]\n");
                }
                break;
            case OP_equal:
            case OP_unequal:
            case OP_compare_GT:
            case OP_compare_LT:
            case OP_compare_GE:
            case OP_compare_LE:
                output_comaprison(file, op);
                break;
            case OP_push_literal:
                fprintf(file, "\t" "mov rax,%lu\n", op->string_value.begin ? strtoul(op->string_value.begin, 0, 0) : (uint64_t)op->i64_value[0]);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_string_literal:
                fprintf(file, "\t" "mov rax, %lu\n", unescaped_string_len(op->string_value));
                fprintf(file, "\t" "push rax\n");
                fprintf(file, "\t" "mov rax, string_literal_%lu\n", op->i64_value[0]);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_char_literal:
                fprintf(file, "\t" "mov rax, %d\n", get_char_constant(op->string_value));
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_call:
                fprintf(file, "\t" "call fn_" SV_FMT "\n", SV_prnt(op->string_value));
                if (size) fprintf(file, "\t" "add rsp, %lu\n", size);
                break;
            case OP_icall:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "call rax\n");
                if (size) fprintf(file, "\t" "add rsp, %lu\n", size);
                break;
            case OP_push_result:
                if      (size == 0);
                else if (size <= 8) fprintf(file, "\t" "push rax\n");
                else if (size <= 16) {
                    fprintf(file, "\t" "push rdx\n");
                    fprintf(file, "\t" "push rax\n");
                } else NOT_IMPLEMENTED("Generating OP_push_result with storage size %lu is not implemented yet.\n", size)
                break;
            case OP_push_arg:
                if (size <= 8) {
                    fprintf(file, "\t" "%s [rbp+%lu]\n", make_movx("rax", size, is_signed_integer(op->type)), 16 + op->i64_value[0]);
                    fprintf(file, "\t" "push rax\n");
                }
                else if (size <= 16) {
                    fprintf(file, "\t" "mov rax, [rbp+%lu]\n", 16 + 8 + op->i64_value[0]);
                    fprintf(file, "\t" "push rax\n");
                    fprintf(file, "\t" "mov rax, [rbp+%lu]\n", 16 + op->i64_value[0]);
                    fprintf(file, "\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", size);
                break;
            case OP_push_local_var:
                if (size <= 8) {
                    fprintf(file, "\t" "%s [rbp-%lu]\n", make_movx("rax", size, is_signed_integer(op->type)) , op->i64_value[0]);
                    fprintf(file, "\t" "push rax\n");
                }
                else if (size <= 16) {
                    fprintf(file, "\t" "mov rax, [rbp-%lu]\n", op->i64_value[0] - 8);
                    fprintf(file, "\t" "push rax\n");
                    fprintf(file, "\t" "mov rax, [rbp-%lu]\n", op->i64_value[0]);
                    fprintf(file, "\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", size);
                break;
            case OP_push_arg_address:
                fprintf(file, "\t" "lea rax, [rbp+%lu]\n", 16 + op->i64_value[0]);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_local_var_address:
                fprintf(file, "\t" "lea rax, [rbp-%lu]\n", op->i64_value[0]);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_global_address:
                fprintf(file, "\t" "mov rax, fn_%.*s\n", SV_prnt(op->string_value));
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_if:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "or rax, rax\n");
                fprintf(file, "\t" "jne @f\n");
                if (op->i64_value[1])
                    fprintf(file, "\t" "jmp else_%ld\n", op->i64_value[0]);
                else
                    fprintf(file, "\t" "jmp end_if_%ld\n", op->i64_value[0]);
                fprintf(file,"@@:\n");
                break;
            case OP_else:
                fprintf(file, "\t" "jmp end_if_%ld\n", op->i64_value[0]);
                fprintf(file,"else_%ld:\n", op->i64_value[0]);
                break;
            case OP_end_if:
                fprintf(file,"end_if_%ld:\n", op->i64_value[0]);
                break;
            case OP_while_loop:
                fprintf(file,"while_loop_%ld:\n", op->i64_value[0]);
                break;
            case OP_while_check:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "or rax, rax\n");
                fprintf(file, "\t" "jne @f\n");
                fprintf(file, "\t" "jmp end_while_%lu\n", op->i64_value[0]);
                fprintf(file,"@@:\n");
                break;
            case OP_while_end:
                fprintf(file, "\t" "jmp while_loop_%lu\n", op->i64_value[0]);
                fprintf(file,"end_while_%lu:\n", op->i64_value[0]);
                break;
            case OP_array_access:
                fprintf(file, "\t" "pop rax\n"); // index
                if (size != 1) { // storage size 1: don't need to multiply with element size
                    fprintf(file, "\t" "mov rbx, %lu\n", size);
                    fprintf(file, "\t" "mul QWORD rbx\n");
                }
                fprintf(file, "\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_member_access:
                fprintf(file, "\t" "mov rax, %lu\n", op->i64_value[0]); // offset
                fprintf(file, "\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_load:
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "xor rax, rax\n");
                if      (size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                else if (size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                else if (size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                else if (size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                else if (size == 16) {
                    fprintf(file, "\t" "mov rcx, [rbx]\n");
                    fprintf(file, "\t" "mov rax, [rbx+8]\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_load with storages size %lu is not implemented.\n", size);

                fprintf(file, "\t" "push rax\n");
                if (size == 16) fprintf(file, "\t" "push rcx\n");
                break;
            case OP_store:
                if      (size == 16) fprintf(file, "\t" "pop rcx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "pop rbx\n");
                if      (size == 1) fprintf(file, "\t" "mov [rbx],  al\n");
                else if (size == 2) fprintf(file, "\t" "mov [rbx],  ax\n");
                else if (size == 4) fprintf(file, "\t" "mov [rbx], eax\n");
                else if (size == 8) fprintf(file, "\t" "mov [rbx], rax\n");
                else if (size == 16) {
                    fprintf(file, "\t" "mov [rbx], rcx\n");
                    fprintf(file, "\t" "mov [rbx+8], rax\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_store with storages size %lu is not implemented.\n", size);
                break;

            case OP_integer_plus_plus:
                fprintf(file, "\t" "pop rbx\n");
                if (op->i64_value[0] == 2) { // post increment: push value first
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }

                if      (size == 1) fprintf(file, "\t" "inc BYTE [rbx]\n");
                else if (size == 2) fprintf(file, "\t" "inc WORD [rbx]\n");
                else if (size == 4) fprintf(file, "\t" "inc DWORD [rbx]\n");
                else if (size == 8) fprintf(file, "\t" "inc QWORD [rbx]\n");
                else NOT_IMPLEMENTED("Generating asm for OP_integer_plus_plus with storages size %lu is not implemented.\n", size);

                if (op->i64_value[0] == 1) { // pre increment: push value after
                                        fprintf(file, "\t" "xor rax, rax\n");
                    if      (size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");

                }
                break;

            case OP_integer_minus_minus:
                fprintf(file, "\t" "pop rbx\n");
                if (op->i64_value[0] == 2) { // post decrement: push value first
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }
                
                if      (size == 1) fprintf(file, "\t" "dec BYTE [rbx]\n");
                else if (size == 2) fprintf(file, "\t" "dec WORD [rbx]\n");
                else if (size == 4) fprintf(file, "\t" "dec DWORD [rbx]\n");
                else if (size == 8) fprintf(file, "\t" "dec QWORD [rbx]\n");
                else NOT_IMPLEMENTED("Generating asm for OP_integer_plus_plus with storages size %lu is not implemented.\n", size);

                if (op->i64_value[0] == 1) { // pre decrement: push value after
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");

                }
                break;

            case OP_slice_plus_plus:
                fprintf(file, "\t" "pop rbx\n");
                if (op->i64_value[0] == 2) { // post increment: push value first
                    fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }
                fprintf(file, "\t" "cmp QWORD [rbx+8], 0\n");
                fprintf(file, "\t" "\n");
                fprintf(file, "\t" "jle @f\n");
                fprintf(file, "\t" "add QWORD [rbx], %lu\n", size);
                fprintf(file, "\t" "dec QWORD [rbx+8]\n");
                fprintf(file,"@@:\n");
                if (op->i64_value[0] == 1) { // pre increment: push value after
                    fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }
                break;

            case OP_sign_extend:
                fprintf(file, "\t" "pop  rax\n");
                if      (size == 1) fprintf(file, "\t" "movsx  rax,  al\n");
                else if (size == 2) fprintf(file, "\t" "movsx  rax,  ax\n");
                else if (size == 4) fprintf(file, "\t" "movsxd rax, eax\n");
                else NOT_IMPLEMENTED("Generating asm for OP_sign_extend with storages size %lu is not implemented.\n", size);
                fprintf(file, "\t" "push  rax\n");
                break;
             
            case OP_bittest:
                fprintf(file, "\t" "pop rax\n"); // bit index
                fprintf(file, "\t" "cqo\n");
                fprintf(file, "\t" "mov rbx, 8\n");
                fprintf(file, "\t" "div rbx\n"); // -> rax: byte offset, rdx: bit index
                fprintf(file, "\t" "pop rbx\n"); // rbx: mem address
                fprintf(file, "\t" "add rbx, rax\n"); // rbx: byte address
                fprintf(file, "\t" "bt [rbx], rdx\n"); // test the bit
                fprintf(file, "\t" "setc al\n");
                fprintf(file, "\t" "movzx rax, al\n");
                fprintf(file, "\t" "push rax\n");
                break;

            case OP_setbit:
                fprintf(file, "\t" "pop rdi\n"); // rdi: value
                fprintf(file, "\t" "pop rax\n"); // bit index
                fprintf(file, "\t" "cqo\n");
                fprintf(file, "\t" "mov rbx, 8\n");
                fprintf(file, "\t" "div rbx\n"); // -> rax: byte offset, rdx: bit index
                fprintf(file, "\t" "pop rsi\n"); // rsi: mem address
                fprintf(file, "\t" "add rsi, rax\n"); // rsi: byte address
                fprintf(file, "\t" "mov al, [rsi]\n"); // load the byte
                fprintf(file, "\t" "mov rcx, rdx\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "shl rdx, cl\n"); // make the mask

                fprintf(file, "\t" "test rdi, rdi\n"); // set to 1?
                fprintf(file, "\t" "jne @f\n");        // yes: jmp
                fprintf(file, "\t" "xor dl, 0xff\n"); // invert the mask
                fprintf(file, "\t" "and al, dl\n");   // clear the bit
                fprintf(file, "\t" "mov dl, al\n");
                fprintf(file, "@@:");
                fprintf(file, "\t" "or al, dl\n");
                fprintf(file, "\t" "mov [rsi], al\n"); // write back
                break;

            case OP_bitshift: // only implemented for positive shift amounts (right shift) for now
                fprintf(file, "\t" "pop rax\n"); // shift amount
                fprintf(file, "\t" "cqo\n");
                fprintf(file, "\t" "mov rbx, 64\n");
                fprintf(file, "\t" "div rbx\n"); // -> rax: QWORD offset, rdx: sub shift amount
                fprintf(file, "\t" "shl rax, 3\n");
                fprintf(file, "\t" "pop rsi\n"); // rsi: mem address
                fprintf(file, "\t" "add rsi, rax\n"); // rsi: QWORD address
                fprintf(file, "\t" "mov rax, [rsi]\n"); // load QWORD [0]
                fprintf(file, "\t" "mov rcx, rdx\n");
                fprintf(file, "\t" "shr rax, cl\n");
                fprintf(file, "\t" "test rdx, rdx\n");
                fprintf(file, "\t" "jz @f\n");
                fprintf(file, "\t" "mov rbx, [rsi+8]\n"); // load QWORD [1]
                fprintf(file, "\t" "mov rcx, 64\n");
                fprintf(file, "\t" "sub rcx, rdx\n");
                fprintf(file, "\t" "shl rbx, cl\n");
                fprintf(file, "\t" "or rax, rbx\n");
                fprintf(file, "@@:");
                fprintf(file, "\t" "push rax\n");
                break;

            case OP_bitand:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "and QWORD [rsp], rax\n");
                break;
            
            case OP_bitor:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "or QWORD [rsp], rax\n");
                break;
            
            case OP_bitxor:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "xor QWORD [rsp], rax\n");
                break;
            
            case OP_bitnot:
                fprintf(file, "\t" "mov rax,-1\n");
                fprintf(file, "\t" "xor QWORD [rsp], rax\n");
                break;

            case OP_pop:
                fprintf(file, "\t" "pop rax\n");
                break;

            case OP_get_enum_member_name: {
                SV name = is_enum_kind(op->type) ? op->type->name : op->type->enumerator.target_type->name;
                fprintf(file, "rept 1 {\n");
                fprintf(file, "local fail\n");
                fprintf(file, "local done\n");
                fprintf(file, "\t" "mov rbx, enum_names_%.*s\n", SV_prnt(name));
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "sub rax, %lu\n", get_min_enum_value(op->type));
                fprintf(file, "\t" "jl fail\n");
                fprintf(file, "\t" "cmp rax, %lu\n", get_max_enum_value(op->type) - get_min_enum_value(op->type));
                fprintf(file, "\t" "jg fail\n");
                fprintf(file, "\t" "shl rax, 4\n");
                fprintf(file, "\t" "add rax, rbx\n");
                fprintf(file, "\t" "push QWORD [rax+8]\n");
                fprintf(file, "\t" "push QWORD [rax]\n");
                fprintf(file, "\t" "jmp done\n");
                fprintf(file, "fail:\n");
                fprintf(file, "\t" "xor rax, rax\n");
                fprintf(file, "\t" "push rax\n");
                fprintf(file, "\t" "push rax\n");
                fprintf(file, "done:\n");
                fprintf(file, "}\n");
                break;
            }
            case OP_float_to_int: {
                fprintf(file, "\t" "movsd xmm0, [rsp]\n");
                fprintf(file, "\t" "cvttsd2si rax, xmm0\n");
                fprintf(file, "\t" "mov [rsp], rax\n");
                break;
            }
            case OP_int_to_float: {
                fprintf(file, "\t" "mov rax, [rsp]\n");
                fprintf(file, "\t" "cvtsi2sd xmm0, rax\n");
                fprintf(file, "\t" "movsd [rsp], xmm0\n");
                break;
            }
            default:
                fprintf(stderr, "%s:%d Generating %s opcode is not implemented yet.\n", __FILE__, __LINE__, opcode_name(op->kind));
                exit(EXIT_FAILURE);
                break;
        }
    }

    Dyn_array enum_types_with_names;
    dyn_array_init(&enum_types_with_names, sizeof(Type*), 8);

    for (size_t i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];

        if (op->kind == OP_push_string_literal) {

            SV str = op->string_value;

            fprintf(file, "string_literal_%lu: db ", op->i64_value[0]);

            while(str.len) {
                if (*str.begin == '\\') {
                    sv_pop(&str);
                    uint8_t c = get_unescaped_char(*str.begin);
                    fprintf(file, "%d", c);
                    sv_pop(&str);
                } else {
                    fprintf(file, "%d", *str.begin);
                    sv_pop(&str);
                }

                fprintf(file, ", ");
            }

            // terminating null byte for C compatability
            fprintf(file, "0");

            fprintf(file, "\n");
        }
        else if (op->kind == OP_get_enum_member_name)
        {
            bool found = false;
            for (size_t i = 0; !found && i < enum_types_with_names.count; i++)
                if(op->type == ((Type**)enum_types_with_names.data)[i])
                    found = true;
            
            if(!found) dyn_array_push_p(&enum_types_with_names, op->type);
        }

    }

    for (size_t i = 0; i < enum_types_with_names.count; i++) {
        Type *t = ((Type**)enum_types_with_names.data)[i];
        ASSERT(is_enum_kind(t) || is_enumerator_kind(t), "Tried to generate member names for something that is not an enum.\n");

        SV name;
        if (is_enum_kind(t)) name = t->name;
        else if (is_enumerator_kind(t)) name = t->enumerator.target_type->name;
        else NOT_IMPLEMENTED("Generating IL for enums");
        
        fprintf(file, "align 8\n");
        fprintf(file, "enum_names_%.*s:\n", SV_prnt(name));
        int64_t max_enum_value = get_max_enum_value(t);
        int64_t min_enum_value = get_min_enum_value(t);
        for (int j = min_enum_value; j <= max_enum_value; j++) {
            SV *member_name = get_enum_member_name_by_value(t, j);
            if (member_name) {
                fprintf(file, "dq enum_names_%.*s_%.*s\n", SV_prnt(name), SV_prnt(*member_name));
                fprintf(file, "dq %lu\n", member_name->len);
            }
            else {
                fprintf(file, "dq 0\n");
                fprintf(file, "dq 0\n");
            }
        }

        if (is_enum_kind(t)) {
            for (size_t j = 0; j < t->_enum.num_members; j++) {
                fprintf(file, "enum_names_%.*s_%.*s: db \"%.*s\"\n", SV_prnt(t->name),
                        SV_prnt(t->_enum.members[j].name), SV_prnt(t->_enum.members[j].name));
            }
        }
        else if (is_enumerator_kind(t) && is_union_kind(t->enumerator.target_type)) {
            Type *u = t->enumerator.target_type;
            for (size_t j = 0; j < u->_union.num_members; j++) {
                fprintf(file, "enum_names_%.*s_%.*s: db \"%.*s\"\n", SV_prnt(u->name),
                        SV_prnt(u->_union.members[j].name), SV_prnt(u->_union.members[j].name));
            }
        }
        else NOT_IMPLEMENTED("Generating member names is not implemented for this type.\n")
    }

    fclose(file);
}
