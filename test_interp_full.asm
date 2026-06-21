[bits 64]
    default rel
section .text
global _start
_start:
    mov rbp, rsp
    call main
    hlt
    hlt
global main
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rax, [rel Lstr0]
    mov qword  [rbp-8], rax
    lea rax, [rel Lstr1]
    push rax
    mov rax, 0
    push rax
    lea rax, [rel Lstr2]
    pop rcx
    push rcx
    push rax
    call __aether_concat
    add rsp, 16
    pop rcx
    push rcx
    push rax
    call __aether_concat
    add rsp, 16
    mov qword  [rbp-16], rax
    mov rsp, rbp
    pop rbp
    ret
section .rodata
Lstr2:
db Hello ", 0
Lstr1: db "!", 0
Lstr0: db "World", , 0
Lstr1:
db !", 0
Lstr0: db "World", 0
, 0
Lstr0:
db World", 0
, 0
