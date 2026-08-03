global _do_syscall

section .text

; uint64_t _do_syscall(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
;
; Mapeamento de Entrada (System V ABI):
; RDI = num (Número da Syscall)
; RSI = a1
; RDX = a2
; RCX = a3
; R8  = a4
; R9  = a5

_do_syscall:
    ; Preserva os registradores que serão modificados/usados no System V
    push r10

    ; Ajusta os registradores para a convenção da Int 0x80 do Kernel:
    mov rax, rdi    ; RAX = Número da Syscall
    mov rdi, rsi    ; RDI = a1
    mov rsi, rdx    ; RSI = a2
    mov rdx, rcx    ; RDX = a3
    mov r10, r8     ; R10 = a4
    mov r8,  r9     ; R8  = a5

    ; Dispara a interrupção do Kernel
    int 0x80

    ; O resultado de retorno já vem em RAX do Kernel!
    pop r10
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
