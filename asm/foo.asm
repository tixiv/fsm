format ELF64 executable
segment readable executable
entry main
main:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, msg_len
    syscall

    mov rax, 60
    mov rdi, 0
    syscall

msg: db "Hello, World", 0x0a
msg_len = $ - msg
