; The print function and some other assembly snippets are copied
; from the Porth compiler https://gitlab.com/tsoding/porth which
; was also the main inspiration for me to start this project.

fn_print:
    mov rdi, [rsp+8]
    test rdi, rdi
    jns @f
    push rdi
    push '-'
    call fn_putc
    pop rdi
    pop rdi
    neg rdi
@@:
    mov     r9, -3689348814741910323
    sub     rsp, 40                 
    mov     BYTE [rsp+31], 10       
    lea     rcx, [rsp+30]           
.L2:                            
    mov     rax, rdi                
    lea     r8, [rsp+32]            
    mul     r9                      
    mov     rax, rdi                
    sub     r8, rcx                 
    shr     rdx, 3                  
    lea     rsi, [rdx+rdx*4]        
    add     rsi, rsi                
    sub     rax, rsi                
    add     eax, 48                 
    mov     BYTE [rcx], al          
    mov     rax, rdi                
    mov     rdi, rdx                
    mov     rdx, rcx                
    sub     rcx, 1                  
    cmp     rax, 9                  
    ja      .L2                     
    lea     rax, [rsp+32]           
    mov     edi, 1                  
    sub     rdx, rax                
    xor     eax, eax                
    lea     rsi, [rsp+32+rdx]       
    mov     rdx, r8                 
    mov     rax, 1                  
    syscall                         
    add     rsp, 40                 
    ret

fn_putc:
    mov rax, 1          ; SYS_write
    mov rdi, 1          ; stdout
    lea rsi, [rsp+8]    ; pointer to the char
    mov rdx, 1          ; length
    syscall
    ret

fn_puts:
    mov rax, 1          ; SYS_write
    mov rdi, 1          ; stdout
    mov rsi, [rsp+8]    ; pointer
    mov rdx, [rsp+16]   ; length
    syscall
    ret

SYS_OPEN    = 2
SYS_CLOSE   = 3
SYS_FSTAT   = 5
SYS_MMAP    = 9
O_RDONLY    = 0
PROT_READ   = 1
MAP_PRIVATE = 2
STAT_SIZE      = 144          ; sizeof(struct stat) on x86-64 Linux
ST_SIZE_OFFSET = 48           ; offsetof(st_size)

fn_open: ; open(filename: u8 &, flags: i64) : i64
    mov     eax, SYS_OPEN
    mov     rdi, [rsp+16]     ; filename
    mov     rsi, [rsp+8]      ; flags
    xor     rdx, rdx          ; mode
    syscall                   ; fd is in rax
    ret

fn_mmap: ; mmap(lenght: i64, fd: i64) : u8 &
   mov     rax, SYS_MMAP
   xor     rdi, rdi          ; addr = NULL
   mov     rsi, [rsp+16]     ; length
   mov     rdx, PROT_READ
   mov     r10, MAP_PRIVATE
   mov     r8,  [rsp+8]      ; fd
   xor     r9, r9            ; offset = 0
   syscall
   ret

fn_fsize: ; fsize(fd: i64) : i64
   push rbp
   mov rbp, rsp
   sub     rsp, STAT_SIZE
   mov     eax, SYS_FSTAT
   mov     rdi, [rbp+16]    ; fd
   mov     rsi, rsp         ; struct statbuf*
   syscall
   test    rax, rax
   js      .cleanup
   mov     rax, [rsp + ST_SIZE_OFFSET]
   mov rsp, rbp
   pop rbp
   ret
.cleanup:
   xor rax, rax
   mov rsp, rbp
   pop rbp
   ret;

fn_close:
    mov eax, SYS_CLOSE
    pop rdi
    syscall
    ret
