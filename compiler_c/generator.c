
#include "generator.h"
#include "opcodes.h"
#include "sv.h"
#include "common.h"
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

    const char *print_function =
    "fn_print:\n"
    "mov rdi, [rsp+8]\n"
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
    "ret\n";

    const char *puts_function =
    "fn_puts:\n"
    "mov rax, 1          ; SYS_write\n"
    "mov rdi, 1          ; stdout\n"
    "mov rsi, [rsp+8]    ; pointer\n"
    "mov rdx, [rsp+16]   ; length\n"
    "syscall\n"
    "ret\n";

    const char *open_function =
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
    fprintf(file, "%s", print_function);
    fprintf(file, "%s", puts_function);
    fprintf(file, "%s", open_function);
    
    for (int i=0; i<num_opcodes; i++) {
        Opcode *t = &opcodes[i];

        fprintf(file,"; ------- %s ---------\n", opcode_name(t->kind));

        switch(t->kind) {
            case OP_begin_fn:
                fprintf(file,"fn_" SV_FMT ":\n", SV_prnt(t->string_value));
                fprintf(file,"\t" "push rbp\n");
                fprintf(file,"\t" "mov rbp, rsp\n");
                if (t->u64_value) fprintf(file,"\t" "sub rsp, %lu\n", t->u64_value);
                break;
            case OP_return:
                if      (t->size == 0);
                else if (t->size <= 8) fprintf(file,"\t" "pop rax\n");
                else if (t->size <= 16) {
                    fprintf(file,"\t" "pop rax\n");
                    fprintf(file,"\t" "pop rdx\n");
                } else NOT_IMPLEMENTED("Generating OP_return with storage size %lu is not implemented yet.\n", t->size)
                fprintf(file,"\t" "mov rsp, rbp\n");
                fprintf(file,"\t" "pop rbp\n");
                fprintf(file,"\t" "ret\n");
                break;
            case OP_add:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "add [rsp], rax\n");
                break;
            case OP_sub:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "sub [rsp], rax\n");
                break;
            case OP_mul:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "mul QWORD [rsp]\n");
                fprintf(file,"\t" "mov [rsp], rax\n");
                break;
            case OP_div:
                fprintf(file, "pop rbx\n");
                fprintf(file, "pop rax\n");
                fprintf(file, "cqo\n");
                fprintf(file, "idiv rbx\n");
                fprintf(file, "push rax\n");
                break;
            case OP_mod:
                fprintf(file, "pop rbx\n");
                fprintf(file, "pop rax\n");
                fprintf(file, "cqo\n");
                fprintf(file, "idiv rbx\n");
                fprintf(file, "push rdx\n");
                break;
            case OP_to_bool:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "or rax, rax\n");
                fprintf(file,"\t" "cmovnz rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_not:
                fprintf(file,"\t" "xor QWORD [rsp], 1\n");
                break;
            case OP_equal:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmove rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_unequal:
                fprintf(file,"\t" "mov rcx, 1\n");
                fprintf(file,"\t" "mov rdx, 0\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmove rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_compare_GT:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmovg rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_compare_LT:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmovl rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_compare_GE:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmovge rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_compare_LE:
                fprintf(file,"\t" "mov rcx, 0\n");
                fprintf(file,"\t" "mov rdx, 1\n");
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "cmp rax, rbx\n");
                fprintf(file,"\t" "cmovle rcx, rdx\n");
                fprintf(file,"\t" "push rcx\n");
                break;
            case OP_push_literal:
                fprintf(file,"\t" "mov rax,%lu\n", t->string_value.begin ? strtoul(t->string_value.begin, 0, 10) : t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_string_literal:
                fprintf(file,"\t" "mov rax, %lu\n", unescaped_string_len(t->string_value));
                fprintf(file,"\t" "push rax\n");
                fprintf(file,"\t" "mov rax, string_literal_%lu\n", t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_char_literal:
                fprintf(file,"\t" "mov rax,%d\n", get_char_constant(t->string_value));
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_call:
                fprintf(file,"\t" "call fn_" SV_FMT "\n", SV_prnt(t->string_value));
                if (t->size) fprintf(file,"\t" "add rsp, %lu\n", t->size);
                break;
            case OP_push_result:
                if      (t->size == 0);
                else if (t->size <= 8) fprintf(file,"\t" "push rax\n");
                else if (t->size <= 16) {
                    fprintf(file,"\t" "push rdx\n");
                    fprintf(file,"\t" "push rax\n");
                } else NOT_IMPLEMENTED("Generating OP_push_result with storage size %lu is not implemented yet.\n", t->size)
                break;
            case OP_push_arg:
                if (t->size <= 8) {
                    fprintf(file,"\t" "mov rax, [rbp+%lu]\n", 16 + t->u64_value);
                    fprintf(file,"\t" "push rax\n");
                }
                else if (t->size <= 16) {
                    fprintf(file,"\t" "mov rax, [rbp+%lu]\n", 16 + 8 + t->u64_value);
                    fprintf(file,"\t" "push rax\n");
                    fprintf(file,"\t" "mov rax, [rbp+%lu]\n", 16 + t->u64_value);
                    fprintf(file,"\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", t->size);
                break;
            case OP_push_local_var:
                if (t->size <= 8) {
                    fprintf(file,"\t" "mov rax, [rbp-%lu]\n", t->u64_value);
                    fprintf(file,"\t" "push rax\n");
                }
                else if (t->size <= 16) {
                    fprintf(file,"\t" "mov rax, [rbp-%lu]\n", t->u64_value - 8);
                    fprintf(file,"\t" "push rax\n");
                    fprintf(file,"\t" "mov rax, [rbp-%lu]\n", t->u64_value);
                    fprintf(file,"\t" "push rax\n");
                }
                else NOT_IMPLEMENTED("OP_push_local_var is not implemented yet for storage size %lu.\n", t->size);
                break;
            case OP_push_arg_address:
                fprintf(file,"\t" "lea rax, [rbp+%lu]\n", 16 + t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_local_var_address:
                fprintf(file,"\t" "lea rax, [rbp-%lu]\n", t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_if:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "or rax, rax\n");
                fprintf(file,"\t" "jne @f\n");
                if (t->u32_value[1])
                    fprintf(file,"\t" "jmp else_%u\n", t->u32_value[0]);
                else
                    fprintf(file,"\t" "jmp end_if_%u\n", t->u32_value[0]);
                fprintf(file,"@@:\n");
                break;
            case OP_else:
                fprintf(file,"\t" "jmp end_if_%u\n", t->u32_value[0]);
                fprintf(file,"else_%u:\n", t->u32_value[0]);
                break;
            case OP_end_if:
                fprintf(file,"end_if_%u:\n", t->u32_value[0]);
                break;
            case OP_while_loop:
                fprintf(file,"while_loop_%lu:\n", t->u64_value);
                break;
            case OP_while_check:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "or rax, rax\n");
                fprintf(file,"\t" "jne @f\n");
                fprintf(file,"\t" "jmp end_while_%lu\n", t->u64_value);
                fprintf(file,"@@:\n");
                break;
            case OP_while_end:
                fprintf(file,"\t" "jmp while_loop_%lu\n", t->u64_value);
                fprintf(file,"end_while_%lu:\n", t->u64_value);
                break;
            case OP_array_access:
                fprintf(file,"\t" "pop rax\n"); // index
                if (t->size != 1) { // storage size 1: don't need to multiply with element size
                    fprintf(file,"\t" "mov rbx, %lu\n", t->size);
                    fprintf(file,"\t" "mul QWORD rbx\n");
                }
                fprintf(file,"\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_member_access:
                fprintf(file,"\t" "mov rax, %lu\n", t->u64_value); // offset
                fprintf(file,"\t" "add [rsp], rax\n"); // add to pointer
                break;
            case OP_load:
                fprintf(file,"\t" "pop rbx\n");
                fprintf(file,"\t" "xor rax, rax\n");
                if      (t->size == 1) fprintf(file,"\t" "mov  al, [rbx]\n");
                else if (t->size == 2) fprintf(file,"\t" "mov  ax, [rbx]\n");
                else if (t->size == 4) fprintf(file,"\t" "mov eax, [rbx]\n");
                else if (t->size == 8) fprintf(file,"\t" "mov rax, [rbx]\n");
                else if (t->size == 16) {
                    fprintf(file,"\t" "mov rcx, [rbx]\n");
                    fprintf(file,"\t" "mov rax, [rbx+8]\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_load with storages size %lu is not implemented.\n", t->size);

                fprintf(file,"\t" "push rax\n");
                if (t->size == 16) fprintf(file,"\t" "push rcx\n");
                break;
            case OP_store:
                if      (t->size == 16) fprintf(file,"\t" "pop rcx\n");
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                if      (t->size == 1) fprintf(file,"\t" "mov [rbx],  al\n");
                else if (t->size == 2) fprintf(file,"\t" "mov [rbx],  ax\n");
                else if (t->size == 4) fprintf(file,"\t" "mov [rbx], eax\n");
                else if (t->size == 8) fprintf(file,"\t" "mov [rbx], rax\n");
                else if (t->size == 16) {
                    fprintf(file,"\t" "mov [rbx], rcx\n");
                    fprintf(file,"\t" "mov [rbx+8], rax\n");
                }
                else NOT_IMPLEMENTED("Generating asm for OP_store with storages size %lu is not implemented.\n", t->size);
                break;
            case OP_sign_extend:
                fprintf(file,"\t" "pop  rax\n");
                if      (t->size == 1) fprintf(file,"\t" "movsx  rax,  al\n");
                else if (t->size == 2) fprintf(file,"\t" "movsx  rax,  ax\n");
                else if (t->size == 4) fprintf(file,"\t" "movsxd rax, eax\n");
                else NOT_IMPLEMENTED("Generating asm for OP_sign_extend with storages size %lu is not implemented.\n", t->size);
                fprintf(file,"\t" "push  rax\n");
                break;
            default:
                fprintf(stderr, "%s:%d Generating %s opcode is not implemented yet.\n", __FILE__, __LINE__, opcode_name(t->kind));
                exit(EXIT_FAILURE);
                break;
        }
    }

    for (int i=0; i<num_opcodes; i++) {
        Opcode *t = &opcodes[i];

        if (t->kind == OP_push_string_literal) {

            SV str = t->string_value;

            fprintf(file, "string_literal_%lu: db ", t->u64_value);

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

                if (str.len) fprintf(file, ", ");
            }

            // terminating null byte for C compatability
            fprintf(file, ", 0");

            fprintf(file, "\n");
        }
    }

    fclose(file);
}
