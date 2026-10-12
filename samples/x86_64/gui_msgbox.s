    .file 1 "samples/x86_64/gui_msgbox.c"
.globl main
main:
    .loc 1 8 0
    pushq %rbp
    movq %rsp, %rbp
    subq $128, %rsp
    .loc 1 9 0
    movq $0, %rax
    pushq %rax
    leaq .LC_1(%rip), %rax
    pushq %rax
    leaq .LC_2(%rip), %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    movq %rcx, %r9
    movq %rdx, %r8
    movq %rsi, %rdx
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call MessageBoxA
    addq $40, %rsp
    popq %rsp
    .loc 1 10 0
    movq $0, %rax
    jmp .Lmain_ret
.Lmain_ret:
    leave
    ret
.section .rodata
.LC_1:
    .string "Hello from native Windows x86-64 GUI binary!"
.LC_2:
    .string "CCompiler"
.text
