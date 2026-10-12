    .file 1 "samples/x86_64/hello.c"
    .file 2 "include/stdio.h"
.globl main
main:
    .loc 1 3 0
    pushq %rbp
    movq %rsp, %rbp
    subq $128, %rsp
    .loc 1 4 0
    leaq .LC_1(%rip), %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call puts
    addq $40, %rsp
    popq %rsp
    .loc 1 5 0
    movq $0, %rax
    jmp .Lmain_ret
.Lmain_ret:
    leave
    ret
.section .rodata
.LC_1:
    .string "Hello, World! Running natively from CCompiler on Windows x86-64."
.text
