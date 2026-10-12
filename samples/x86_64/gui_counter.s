    .file 1 "samples/x86_64/gui_counter.c"
.globl WndProc
WndProc:
    .loc 1 67 0
    pushq %rbp
    movq %rsp, %rbp
    subq $144, %rsp
    movq %rcx, -8(%rbp)
    movq %rdx, -16(%rbp)
    movq %r8, -24(%rbp)
    movq %r9, -32(%rbp)
    .loc 1 68 0
    movq -16(%rbp), %rax
    pushq %rax
    movq $1, %rax
    movq %rax, %r10
    popq %rax
    cmpq %r10, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_2
    .loc 1 70 0
    movq $0, %rax
    pushq %rax
    leaq .LC_3(%rip), %rax
    pushq %rax
    leaq .LC_4(%rip), %rax
    pushq %rax
    movq $1073741824, %rax
    pushq %rax
    movq $268435456, %rax
    movq %rax, %r10
    popq %rax
    orq %r10, %rax
    pushq %rax
    movq $1, %rax
    movq %rax, %r10
    popq %rax
    orq %r10, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $30, %rax
    pushq %rax
    movq $200, %rax
    pushq %rax
    movq $30, %rax
    pushq %rax
    movq $50, %rax
    pushq %rax
    movq 64(%rsp), %rax
    movq %rax, %r9
    movq %rax, %rcx
    movq 72(%rsp), %rax
    movq %rax, %r8
    movq %rax, %rdx
    movq 80(%rsp), %rax
    movq %rax, %rdx
    movq %rax, %rsi
    movq 88(%rsp), %rax
    movq %rax, %rcx
    movq %rax, %rdi
    movb $0, %al
    subq $32, %rsp
    call CreateWindowExA
    addq $32, %rsp
    addq $64, %rsp
    movq %rax, -40(%rbp)
    .loc 1 78 0
    movq $0, %rax
    pushq %rax
    leaq .LC_5(%rip), %rax
    pushq %rax
    leaq .LC_6(%rip), %rax
    pushq %rax
    movq $1073741824, %rax
    pushq %rax
    movq $268435456, %rax
    movq %rax, %r10
    popq %rax
    orq %r10, %rax
    pushq %rax
    movq $0, %rax
    movq %rax, %r10
    popq %rax
    orq %r10, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $35, %rax
    pushq %rax
    movq $150, %rax
    pushq %rax
    movq $80, %rax
    pushq %rax
    movq $75, %rax
    pushq %rax
    movq 64(%rsp), %rax
    movq %rax, %r9
    movq %rax, %rcx
    movq 72(%rsp), %rax
    movq %rax, %r8
    movq %rax, %rdx
    movq 80(%rsp), %rax
    movq %rax, %rdx
    movq %rax, %rsi
    movq 88(%rsp), %rax
    movq %rax, %rcx
    movq %rax, %rdi
    movb $0, %al
    subq $32, %rsp
    call CreateWindowExA
    addq $32, %rsp
    addq $64, %rsp
    movq %rax, -48(%rbp)
    .loc 1 84 0
    movq $0, %rax
    jmp .LWndProc_ret
.Lif_end_2:
    .loc 1 87 0
    movq -16(%rbp), %rax
    pushq %rax
    movq $273, %rax
    movq %rax, %r10
    popq %rax
    cmpq %r10, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_8
    .loc 1 88 0
    movq -32(%rbp), %rax
    pushq %rax
    movq -48(%rbp), %rax
    movq %rax, %r10
    popq %rax
    cmpq %r10, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_10
    .loc 1 89 0
    .loc 1 90 0
    movq -120(%rbp), %rax
    pushq %rax
    movq $1, %rax
    movq %rax, %r10
    popq %rax
    addq %r10, %rax
    movq %rax, -120(%rbp)
    .loc 1 91 0
    leaq -112(%rbp), %rax
    pushq %rax
    leaq .LC_11(%rip), %rax
    pushq %rax
    movq -120(%rbp), %rax
    pushq %rax
    popq %rdx
    popq %rsi
    popq %rdi
    movq %rdx, %r8
    movq %rsi, %rdx
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call sprintf
    addq $40, %rsp
    popq %rsp
    .loc 1 92 0
    movq -40(%rbp), %rax
    pushq %rax
    leaq -112(%rbp), %rax
    pushq %rax
    popq %rsi
    popq %rdi
    movq %rsi, %rdx
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call SetWindowTextA
    addq $40, %rsp
    popq %rsp
.Lif_end_10:
    .loc 1 94 0
    movq $0, %rax
    jmp .LWndProc_ret
.Lif_end_8:
    .loc 1 97 0
    movq -16(%rbp), %rax
    pushq %rax
    movq $2, %rax
    movq %rax, %r10
    popq %rax
    cmpq %r10, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_13
    .loc 1 98 0
    movq $0, %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call PostQuitMessage
    addq $40, %rsp
    popq %rsp
    .loc 1 99 0
    movq $0, %rax
    jmp .LWndProc_ret
.Lif_end_13:
    .loc 1 102 0
    movq -8(%rbp), %rax
    pushq %rax
    movq -16(%rbp), %rax
    pushq %rax
    movq -24(%rbp), %rax
    pushq %rax
    movq -32(%rbp), %rax
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
    call DefWindowProcA
    addq $40, %rsp
    popq %rsp
    jmp .LWndProc_ret
.LWndProc_ret:
    leave
    ret
.globl main
main:
    .loc 1 105 0
    pushq %rbp
    movq %rsp, %rbp
    subq $208, %rsp
    .loc 1 106 0
    movq $0, %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call GetModuleHandleA
    addq $40, %rsp
    popq %rsp
    movq %rax, -8(%rbp)
    .loc 1 107 0
    leaq .LC_14(%rip), %rax
    movq %rax, -16(%rbp)
    .loc 1 109 0
    .loc 1 110 0
    movq $0, %rax
    movq %rax, -104(%rbp)
    .loc 1 111 0
    movq $0, %rax
    movq %rax, -96(%rbp)
    .loc 1 112 0
    leaq WndProc(%rip), %rax
    movq %rax, -88(%rbp)
    .loc 1 113 0
    movq $0, %rax
    movq %rax, -80(%rbp)
    .loc 1 114 0
    movq $0, %rax
    movq %rax, -72(%rbp)
    .loc 1 115 0
    movq -8(%rbp), %rax
    movq %rax, -64(%rbp)
    .loc 1 116 0
    movq $0, %rax
    movq %rax, -56(%rbp)
    .loc 1 117 0
    movq $0, %rax
    movq %rax, -48(%rbp)
    .loc 1 118 0
    movq $6, %rax
    movq %rax, -40(%rbp)
    .loc 1 119 0
    movq $0, %rax
    movq %rax, -32(%rbp)
    .loc 1 120 0
    movq -16(%rbp), %rax
    movq %rax, -24(%rbp)
    .loc 1 122 0
    leaq -104(%rbp), %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call RegisterClassA
    addq $40, %rsp
    popq %rsp
    movw %ax, -112(%rbp)
    .loc 1 123 0
    movswq -112(%rbp), %rax
    cmpq $0, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_16
    .loc 1 124 0
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call GetLastError
    addq $40, %rsp
    popq %rsp
    movq %rax, -120(%rbp)
    .loc 1 125 0
    movq $100, %rax
    pushq %rax
    movq -120(%rbp), %rax
    movq %rax, %r10
    popq %rax
    addq %r10, %rax
    jmp .Lmain_ret
.Lif_end_16:
    .loc 1 128 0
    movq $0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    pushq %rax
    leaq .LC_17(%rip), %rax
    pushq %rax
    movq $13565952, %rax
    pushq %rax
    movq $268435456, %rax
    movq %rax, %r10
    popq %rax
    orq %r10, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $200, %rax
    pushq %rax
    movq $320, %rax
    pushq %rax
    movq $2147483648, %rax
    pushq %rax
    movq $2147483648, %rax
    pushq %rax
    movq 64(%rsp), %rax
    movq %rax, %r9
    movq %rax, %rcx
    movq 72(%rsp), %rax
    movq %rax, %r8
    movq %rax, %rdx
    movq 80(%rsp), %rax
    movq %rax, %rdx
    movq %rax, %rsi
    movq 88(%rsp), %rax
    movq %rax, %rcx
    movq %rax, %rdi
    movb $0, %al
    subq $32, %rsp
    call CreateWindowExA
    addq $32, %rsp
    addq $64, %rsp
    movq %rax, -128(%rbp)
    .loc 1 137 0
    movq -128(%rbp), %rax
    cmpq $0, %rax
    sete %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lif_end_19
    .loc 1 138 0
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call GetLastError
    addq $40, %rsp
    popq %rsp
    movq %rax, -120(%rbp)
    .loc 1 139 0
    movq $200, %rax
    pushq %rax
    movq -120(%rbp), %rax
    movq %rax, %r10
    popq %rax
    addq %r10, %rax
    jmp .Lmain_ret
.Lif_end_19:
    .loc 1 142 0
    movq -128(%rbp), %rax
    pushq %rax
    movq $5, %rax
    pushq %rax
    popq %rsi
    popq %rdi
    movq %rsi, %rdx
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call ShowWindow
    addq $40, %rsp
    popq %rsp
    .loc 1 143 0
    movq -128(%rbp), %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call UpdateWindow
    addq $40, %rsp
    popq %rsp
    .loc 1 145 0
    .loc 1 146 0
.Lwhile_cond_20:
    leaq -184(%rbp), %rax
    pushq %rax
    movq $0, %rax
    pushq %rax
    movq $0, %rax
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
    call GetMessageA
    addq $40, %rsp
    popq %rsp
    pushq %rax
    movq $0, %rax
    movq %rax, %r10
    popq %rax
    cmpq %r10, %rax
    setg %al
    movzbq %al, %rax
    cmpq $0, %rax
    je .Lwhile_end_21
    .loc 1 147 0
    leaq -184(%rbp), %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call TranslateMessage
    addq $40, %rsp
    popq %rsp
    .loc 1 148 0
    leaq -184(%rbp), %rax
    pushq %rax
    popq %rdi
    movq %rdi, %rcx
    movb $0, %al
    movq %rsp, %r11
    andq $-16, %rsp
    pushq %r11
    subq $40, %rsp
    call DispatchMessageA
    addq $40, %rsp
    popq %rsp
    jmp .Lwhile_cond_20
.Lwhile_end_21:
    .loc 1 151 0
    movq -168(%rbp), %rax
    jmp .Lmain_ret
.Lmain_ret:
    leave
    ret
.section .rodata
.LC_3:
    .string "STATIC"
.LC_4:
    .string "Count: 0"
.LC_5:
    .string "BUTTON"
.LC_6:
    .string "Increment"
.LC_11:
    .string "Count: %d"
.LC_14:
    .string "CounterWindowClass"
.LC_17:
    .string "Win32 Counter GUI"
.text
