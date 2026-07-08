
#include "generator.h"
#include "opcodes.h"
#include "sv.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

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
    "mov rdi, [rsp+8]\n"
    "push rdi\n"
    "xor rcx, rcx\n"
    ".loop:\n"
    "cmp byte [rdi + rcx], 0\n"
    "je .found\n"
    "inc rcx\n"
    "jmp .loop\n"
    ".found:\n"
    "mov rax, 1          ; SYS_write\n"
    "mov rdi, 1          ; stdout\n"
    "pop rsi             ; original pointer\n"
    "mov rdx, rcx        ; length\n"
    "syscall\n"
    "ret\n";

    fprintf(file, "%s", header);
    fprintf(file, "%s", print_function);
    fprintf(file, "%s", puts_function);

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
                if (t->u64_value) fprintf(file,"\t" "pop rax\n");
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
                fprintf(file,"\t" "mov rax,%lu\n", strtoul(t->string_value.begin, 0, 10));
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_string_literal:
                fprintf(file,"\t" "mov rax, string_literal_%lu\n", t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_call:
                fprintf(file,"\t" "call fn_" SV_FMT "\n", SV_prnt(t->string_value));
                if (t->u64_value) fprintf(file,"\t" "add rsp, %lu\n", t->u64_value);
                break;
            case OP_push_result:
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_arg:
                fprintf(file,"\t" "mov rax, [rbp+%lu]\n", 16 + t->u64_value);
                fprintf(file,"\t" "push rax\n");
                break;
            case OP_push_local_var:
                fprintf(file,"\t" "mov rax, [rbp-%lu]\n", t->u64_value);
                fprintf(file,"\t" "push rax\n");
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
                if (t->u64_value != 1) { // storage size
                    fprintf(file,"\t" "mov rbx, %lu\n", t->u64_value);
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
                if      (t->u64_value == 8) fprintf(file,"\t" "mov rax, [rbx]\n");
                else if (t->u64_value == 4) fprintf(file,"\t" "mov eax, [rbx]\n");
                else if (t->u64_value == 2) fprintf(file,"\t" "mov  ax, [rbx]\n");
                else if (t->u64_value == 1) fprintf(file,"\t" "mov  al, [rbx]\n");
                else NOT_IMPLEMENTED("Generating asm for OP_load with storages size %lu is not implemented.\n", t->u64_value);

                fprintf(file,"\t" "push rax\n");
                break;
            case OP_store:
                fprintf(file,"\t" "pop rax\n");
                fprintf(file,"\t" "pop rbx\n");
                if      (t->u64_value == 8) fprintf(file,"\t" "mov [rbx], rax\n");
                else if (t->u64_value == 4) fprintf(file,"\t" "mov [rbx], eax\n");
                else if (t->u64_value == 2) fprintf(file,"\t" "mov [rbx],  ax\n");
                else if (t->u64_value == 1) fprintf(file,"\t" "mov [rbx],  al\n");
                else NOT_IMPLEMENTED("Generating asm for OP_store with storages size %lu is not implemented.\n", t->u64_value);
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
                    uint8_t c = ' ';
                    switch (*str.begin) {
                        case '\\': c = '\\'; break;
                        case 'a': c = '\a'; break;
                        case 'b': c = '\b'; break;
                        case 'e': c = '\e'; break;
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case '"': c = '\"'; break;
                        default:
                            NOT_IMPLEMENTED("Genearating assembly for the escape sequence '\\%c' is not implemented yet.", *str.begin);
                            break;
                    }

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
