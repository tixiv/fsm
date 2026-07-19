
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
        case '"':  return '\"'; break;
        default:
            NOT_IMPLEMENTED("Genearating assembly for the escape sequence '\\%c' is not implemented yet.", c);
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

void output_asm(const char *asm_file_name) {
    FILE *file = fopen(asm_file_name, "w");

    const char* header =
    "format ELF64 executable\n"
    "segment readable writeable executable\n"
    "entry _start\n"
    "_start:\n"
    "call fn_main\n"
    "mov rax, 60\n"
    "mov rdi, 0\n"
    "syscall\n";

    // The print function and some other assembly snippets are copied
    // from the Porth compiler https://gitlab.com/tsoding/porth which
    // was also the main inspiration for me to start this project.

    const char *builtin_functions =
    "fn_print:\n"
    "mov rdi, [rsp+8]\n"
    "test rdi, rdi\n"
    "jns @f\n"
    "push rdi\n"
    "push '-'\n"
    "call fn_putc\n"
    "pop rdi\n"
    "pop rdi\n"
    "neg rdi\n"
    "@@:\n"
    "mov     r9, -3689348814741910323\n"
    "sub     rsp, 40\n"                 
    "mov     BYTE [rsp+31], 10\n"       
    "lea     rcx, [rsp+30]\n"           
    ".L2:\n"                            
    "mov     rax, rdi\n"                
    "lea     r8, [rsp+32]\n"            
    "mul     r9\n"                      
    "mov     rax, rdi\n"                
    "sub     r8, rcx\n"                 
    "shr     rdx, 3\n"                  
    "lea     rsi, [rdx+rdx*4]\n"        
    "add     rsi, rsi\n"                
    "sub     rax, rsi\n"                
    "add     eax, 48\n"                 
    "mov     BYTE [rcx], al\n"          
    "mov     rax, rdi\n"                
    "mov     rdi, rdx\n"                
    "mov     rdx, rcx\n"                
    "sub     rcx, 1\n"                  
    "cmp     rax, 9\n"                  
    "ja      .L2\n"                     
    "lea     rax, [rsp+32]\n"           
    "mov     edi, 1\n"                  
    "sub     rdx, rax\n"                
    "xor     eax, eax\n"                
    "lea     rsi, [rsp+32+rdx]\n"       
    "mov     rdx, r8\n"                 
    "mov     rax, 1\n"                  
    "syscall\n"                         
    "add     rsp, 40\n"                 
    "ret\n"

    "fn_putc:\n"
    "mov rax, 1          ; SYS_write\n"
    "mov rdi, 1          ; stdout\n"
    "lea rsi, [rsp+8]    ; pointer to the char\n"
    "mov rdx, 1          ; length\n"
    "syscall\n"
    "ret\n"

    "fn_puts:\n"
    "mov rax, 1          ; SYS_write\n"
    "mov rdi, 1          ; stdout\n"
    "mov rsi, [rsp+8]    ; pointer\n"
    "mov rdx, [rsp+16]   ; length\n"
    "syscall\n"
    "ret\n"

    "SYS_OPEN    = 2\n"
    "SYS_CLOSE   = 3\n"
    "SYS_FSTAT   = 5\n"
    "SYS_MMAP    = 9\n"
    "O_RDONLY    = 0\n"
    "PROT_READ   = 1\n"
    "MAP_PRIVATE = 2\n"
    "STAT_SIZE      = 144          ; sizeof(struct stat) on x86-64 Linux\n"
    "ST_SIZE_OFFSET = 48           ; offsetof(st_size)\n"
    "\n"
    "fn_open: ; open(filename: u8 &, flags: i64) : i64\n"
    "    mov     eax, SYS_OPEN\n"
    "    mov     rdi, [rsp+16]     ; filename\n"
    "    mov     rsi, [rsp+8]      ; flags\n"
    "    xor     rdx, rdx          ; mode\n"
    "    syscall                   ; fd is in rax\n"
    "    ret\n"
    "\n"
    "fn_mmap: ; mmap(lenght: i64, fd: i64) : u8 &\n"
    "   mov     rax, SYS_MMAP\n"
    "   xor     rdi, rdi          ; addr = NULL\n"
    "   mov     rsi, [rsp+16]     ; length\n"
    "   mov     rdx, PROT_READ\n"
    "   mov     r10, MAP_PRIVATE\n"
    "   mov     r8,  [rsp+8]      ; fd\n"
    "   xor     r9, r9            ; offset = 0\n"
    "   syscall\n"
    "   ret\n"
    "\n"
    "fn_fsize: ; fsize(fd: i64) : i64\n"
    "   push rbp\n"
    "   mov rbp, rsp\n"
    "   sub     rsp, STAT_SIZE\n"
    "   mov     eax, SYS_FSTAT\n"
    "   mov     rdi, [rbp+16]    ; fd\n"
    "   mov     rsi, rsp         ; struct statbuf*\n"
    "   syscall\n"
    "   test    rax, rax\n"
    "   js      .cleanup\n"
    "   mov     rax, [rsp + ST_SIZE_OFFSET]\n"
    "   mov rsp, rbp\n"
    "   pop rbp\n"
    "   ret\n"
    ".cleanup:\n"
    "   xor rax, rax\n"
    "   mov rsp, rbp\n"
    "   pop rbp\n"
    "   ret\n";
/*

    ;---------------------------------------
    ; close(fd)
    ;---------------------------------------

    mov     eax, SYS_CLOSE
    mov     rdi, r12
    syscall

    add     rsp, STAT_SIZE

    ; return slice
    mov     rax, r14
    mov     rdx, r13
    ret

.cleanup:

    mov     eax, SYS_CLOSE
    mov     rdi, r12
    syscall

    add     rsp, STAT_SIZE

.error:

    xor     eax, eax
    xor     edx, edx
    ret

*/

    fprintf(file, "%s", header);
    fprintf(file, "%s", builtin_functions);
    
    for (int i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];

        fprintf(file,"; ------- %s ---------\n", opcode_name(op->kind));

        switch(op->kind) {
            case OP_begin_fn:
                fprintf(file,"fn_" SV_FMT ":\n", SV_prnt(op->string_value));
                fprintf(file, "\t" "push rbp\n");
                fprintf(file, "\t" "mov rbp, rsp\n");
                if (op->size) {
                    ASSERT(op->size % 8 == 0, "Can't work with local var sizes not divisible by 8.\n");
                    fprintf(file, "\t" "sub rsp, %lu\n", op->size);
                    fprintf(file, "\t" "mov rcx, %lu\n", op->size / 8);
                    fprintf(file, "\t" "lea rdi, [rbp - %lu]\n", op->size);
                    fprintf(file, "\t" "xor rax, rax\n");
                    fprintf(file, "\t" "rep stosq\n");
                }
                break;
            case OP_return:
                if      (op->size == 0);
                else if (op->size <= 8) fprintf(file, "\t" "pop rax\n");
                else if (op->size <= 16) {
                    fprintf(file, "\t" "pop rax\n");
                    fprintf(file, "\t" "pop rdx\n");
                } else NOT_IMPLEMENTED("Generating OP_return with storage size %lu is not implemented yet.\n", op->size)
                fprintf(file, "\t" "mov rsp, rbp\n");
                fprintf(file, "\t" "pop rbp\n");
                fprintf(file, "\t" "ret\n");
                break;
            case OP_add:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "add [rsp], rax\n");
                break;
            case OP_sub:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "sub [rsp], rax\n");
                break;
            case OP_mul:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "mul QWORD [rsp]\n");
                fprintf(file, "\t" "mov [rsp], rax\n");
                break;
            case OP_div:
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cqo\n");
                fprintf(file, "\t" "idiv rbx\n");
                fprintf(file, "\t" "push rax\n");
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
                fprintf(file, "\t" "neg QWORD [rsp]\n");
                break;
            case OP_equal:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmove rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: left operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_unequal:
                fprintf(file, "\t" "mov rcx, 1\n");
                fprintf(file, "\t" "mov rdx, 0\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmove rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: left operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_compare_GT:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmovg rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: right operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_compare_LT:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmovl rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: right operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_compare_GE:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmovge rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: right operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_compare_LE:
                fprintf(file, "\t" "mov rcx, 0\n");
                fprintf(file, "\t" "mov rdx, 1\n");
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "cmp rax, rbx\n");
                fprintf(file, "\t" "cmovle rcx, rdx\n");
                if (op->u64_value) fprintf(file, "\t" "push rbx\n"); // chaining value: right operand
                fprintf(file, "\t" "push rcx\n");
                break;
            case OP_push_literal:
                fprintf(file, "\t" "mov rax,%lu\n", op->string_value.begin ? strtoul(op->string_value.begin, 0, 0) : op->u64_value);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_string_literal:
                fprintf(file, "\t" "mov rax, %lu\n", unescaped_string_len(op->string_value));
                fprintf(file, "\t" "push rax\n");
                fprintf(file, "\t" "mov rax, string_literal_%lu\n", op->u64_value);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_char_literal:
                fprintf(file, "\t" "mov rax, %d\n", get_char_constant(op->string_value));
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_call:
                fprintf(file, "\t" "call fn_" SV_FMT "\n", SV_prnt(op->string_value));
                if (op->size) fprintf(file, "\t" "add rsp, %lu\n", op->size);
                break;
            case OP_icall:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "call rax\n");
                if (op->size) fprintf(file, "\t" "add rsp, %lu\n", op->size);
                break;
            case OP_push_result:
                if      (op->size == 0);
                else if (op->size <= 8) fprintf(file, "\t" "push rax\n");
                else if (op->size <= 16) {
                    fprintf(file, "\t" "push rdx\n");
                    fprintf(file, "\t" "push rax\n");
                } else NOT_IMPLEMENTED("Generating OP_push_result with storage size %lu is not implemented yet.\n", op->size)
                break;
            case OP_push_arg:
                if (op->size <= 8) {
                    fprintf(file, "\t" "%s [rbp+%lu]\n", make_movx("rax", op->size, op->_signed), 16 + op->u64_value);
                    fprintf(file, "\t" "push rax\n");
                }
                else if (op->size <= 16) {
                    fprintf(file, "\t" "mov rax, [rbp+%lu]\n", 16 + 8 + op->u64_value);
                    fprintf(file, "\t" "push rax\n");
                    fprintf(file, "\t" "mov rax, [rbp+%lu]\n", 16 + op->u64_value);
                    fprintf(file, "\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", op->size);
                break;
            case OP_push_local_var:
                if (op->size <= 8) {
                    fprintf(file, "\t" "%s [rbp-%lu]\n", make_movx("rax", op->size, op->_signed) , op->u64_value);
                    fprintf(file, "\t" "push rax\n");
                }
                else if (op->size <= 16) {
                    fprintf(file, "\t" "mov rax, [rbp-%lu]\n", op->u64_value - 8);
                    fprintf(file, "\t" "push rax\n");
                    fprintf(file, "\t" "mov rax, [rbp-%lu]\n", op->u64_value);
                    fprintf(file, "\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", op->size);
                break;
            case OP_push_arg_address:
                fprintf(file, "\t" "lea rax, [rbp+%lu]\n", 16 + op->u64_value);
                fprintf(file, "\t" "push rax\n");
                break;
            case OP_push_local_var_address:
                fprintf(file, "\t" "lea rax, [rbp-%lu]\n", op->u64_value);
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
                if (op->u32_value[1])
                    fprintf(file, "\t" "jmp else_%u\n", op->u32_value[0]);
                else
                    fprintf(file, "\t" "jmp end_if_%u\n", op->u32_value[0]);
                fprintf(file,"@@:\n");
                break;
            case OP_else:
                fprintf(file, "\t" "jmp end_if_%u\n", op->u32_value[0]);
                fprintf(file,"else_%u:\n", op->u32_value[0]);
                break;
            case OP_end_if:
                fprintf(file,"end_if_%u:\n", op->u32_value[0]);
                break;
            case OP_while_loop:
                fprintf(file,"while_loop_%lu:\n", op->u64_value);
                break;
            case OP_while_check:
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "or rax, rax\n");
                fprintf(file, "\t" "jne @f\n");
                fprintf(file, "\t" "jmp end_while_%lu\n", op->u64_value);
                fprintf(file,"@@:\n");
                break;
            case OP_while_end:
                fprintf(file, "\t" "jmp while_loop_%lu\n", op->u64_value);
                fprintf(file,"end_while_%lu:\n", op->u64_value);
                break;
            case OP_array_access:
                fprintf(file, "\t" "pop rax\n"); // index
                if (op->size != 1) { // storage size 1: don't need to multiply with element size
                    fprintf(file, "\t" "mov rbx, %lu\n", op->size);
                    fprintf(file, "\t" "mul QWORD rbx\n");
                }
                fprintf(file, "\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_member_access:
                fprintf(file, "\t" "mov rax, %lu\n", op->u64_value); // offset
                fprintf(file, "\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_load:
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "xor rax, rax\n");
                if      (op->size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                else if (op->size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                else if (op->size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                else if (op->size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                else if (op->size == 16) {
                    fprintf(file, "\t" "mov rcx, [rbx]\n");
                    fprintf(file, "\t" "mov rax, [rbx+8]\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_load with storages size %lu is not implemented.\n", op->size);

                fprintf(file, "\t" "push rax\n");
                if (op->size == 16) fprintf(file, "\t" "push rcx\n");
                break;
            case OP_store:
                if      (op->size == 16) fprintf(file, "\t" "pop rcx\n");
                fprintf(file, "\t" "pop rax\n");
                fprintf(file, "\t" "pop rbx\n");
                if      (op->size == 1) fprintf(file, "\t" "mov [rbx],  al\n");
                else if (op->size == 2) fprintf(file, "\t" "mov [rbx],  ax\n");
                else if (op->size == 4) fprintf(file, "\t" "mov [rbx], eax\n");
                else if (op->size == 8) fprintf(file, "\t" "mov [rbx], rax\n");
                else if (op->size == 16) {
                    fprintf(file, "\t" "mov [rbx], rcx\n");
                    fprintf(file, "\t" "mov [rbx+8], rax\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_store with storages size %lu is not implemented.\n", op->size);
                break;

            case OP_integer_plus_plus:
                fprintf(file, "\t" "pop rbx\n");
                if (op->u64_value == 2) { // post increment: push value first
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (op->size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (op->size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (op->size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (op->size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }

                if      (op->size == 1) fprintf(file, "\t" "inc BYTE [rbx]\n");
                else if (op->size == 2) fprintf(file, "\t" "inc WORD [rbx]\n");
                else if (op->size == 4) fprintf(file, "\t" "inc DWORD [rbx]\n");
                else if (op->size == 8) fprintf(file, "\t" "inc QWORD [rbx]\n");
                else NOT_IMPLEMENTED("Generating asm for OP_integer_plus_plus with storages size %lu is not implemented.\n", op->size);

                if (op->u64_value == 1) { // pre increment: push value after
                                        fprintf(file, "\t" "xor rax, rax\n");
                    if      (op->size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (op->size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (op->size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (op->size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");

                }
                break;

            case OP_integer_minus_minus:
                fprintf(file, "\t" "pop rbx\n");
                if (op->u64_value == 2) { // post decrement: push value first
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (op->size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (op->size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (op->size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (op->size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");
                }
                
                if      (op->size == 1) fprintf(file, "\t" "dec BYTE [rbx]\n");
                else if (op->size == 2) fprintf(file, "\t" "dec WORD [rbx]\n");
                else if (op->size == 4) fprintf(file, "\t" "dec DWORD [rbx]\n");
                else if (op->size == 8) fprintf(file, "\t" "dec QWORD [rbx]\n");
                else NOT_IMPLEMENTED("Generating asm for OP_integer_plus_plus with storages size %lu is not implemented.\n", op->size);

                if (op->u64_value == 1) { // pre decrement: push value after
                    fprintf(file, "\t" "xor rax, rax\n");
                    if      (op->size == 1) fprintf(file, "\t" "mov  al, [rbx]\n");
                    else if (op->size == 2) fprintf(file, "\t" "mov  ax, [rbx]\n");
                    else if (op->size == 4) fprintf(file, "\t" "mov eax, [rbx]\n");
                    else if (op->size == 8) fprintf(file, "\t" "mov rax, [rbx]\n");
                    fprintf(file, "\t" "push rax\n");

                }
                break;

            case OP_slice_plus_plus:
                fprintf(file, "\t" "pop rbx\n");
                fprintf(file, "\t" "cmp QWORD [rbx+8], 0\n");
                fprintf(file, "\t" "jle @f\n");
                fprintf(file, "\t" "add QWORD [rbx], %lu\n", op->size);
                fprintf(file, "\t" "dec QWORD [rbx+8]\n");
                fprintf(file,"@@:\n");
                break;

            case OP_sign_extend:
                fprintf(file, "\t" "pop  rax\n");
                if      (op->size == 1) fprintf(file, "\t" "movsx  rax,  al\n");
                else if (op->size == 2) fprintf(file, "\t" "movsx  rax,  ax\n");
                else if (op->size == 4) fprintf(file, "\t" "movsxd rax, eax\n");
                else NOT_IMPLEMENTED("Generating asm for OP_sign_extend with storages size %lu is not implemented.\n", op->size);
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

            case OP_get_enum_member_name:
                fprintf(file, "rept 1 {\n");
                fprintf(file, "local fail\n");
                fprintf(file, "local done\n");
                fprintf(file, "\t" "mov rbx, enum_names_%.*s\n", SV_prnt(op->type->name));
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

            default:
                fprintf(stderr, "%s:%d Generating %s opcode is not implemented yet.\n", __FILE__, __LINE__, opcode_name(op->kind));
                exit(EXIT_FAILURE);
                break;
        }
    }

    Dyn_array enum_types_with_names;
    dyn_array_init(&enum_types_with_names, sizeof(Type*), 8);

    for (int i=0; i<num_opcodes; i++) {
        Opcode *op = &opcodes[i];

        if (op->kind == OP_push_string_literal) {

            SV str = op->string_value;

            fprintf(file, "string_literal_%lu: db ", op->u64_value);

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
            for (int i = 0; !found && i < enum_types_with_names.count; i++)
                if(op->type == ((Type**)enum_types_with_names.data)[i])
                    found = true;
            
            if(!found) dyn_array_push_p(&enum_types_with_names, op->type);
        }

    }

    for (int i = 0; i < enum_types_with_names.count; i++) {
        Type *t = ((Type**)enum_types_with_names.data)[i];
        ASSERT(is_enum_kind(t), "Tried to generate member names for something that is not an enum.\n");
        fprintf(file, "align 8\n");
        fprintf(file, "enum_names_%.*s:\n", SV_prnt(t->name));
        int64_t max_enum_value = get_max_enum_value(t);
        int64_t min_enum_value = get_min_enum_value(t);
        for (int j = min_enum_value; j <= max_enum_value; j++) {
            EnumMember *member = get_enum_member_by_value(t, j);
            if (member) {
                fprintf(file, "dq enum_names_%.*s_%.*s\n", SV_prnt(t->name), SV_prnt(member->name));
                fprintf(file, "dq %lu\n", member->name.len);
            }
            else {
                fprintf(file, "dq 0\n");
                fprintf(file, "dq 0\n");
            }
        }

        for (int j = 0; j < t->_enum.num_members; j++) {
            fprintf(file, "enum_names_%.*s_%.*s: db \"%.*s\"\n", SV_prnt(t->name),
                    SV_prnt(t->_enum.members[j].name), SV_prnt(t->_enum.members[j].name));
        }
    }

    fclose(file);
}
