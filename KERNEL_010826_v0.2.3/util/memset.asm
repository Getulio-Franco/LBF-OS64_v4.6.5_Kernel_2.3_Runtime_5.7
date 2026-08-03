global memset

section .text

; void* memset(void* s, int c, size_t n);
; System V ABI: RDI = s, RSI = c, RDX = n -> Retorna RAX = s
memset:
    mov rax, rdi        ; Preserva o ponteiro de retorno em RAX

    ; 1. Replicar o byte de RSI (8-bits) para cobrir os 64-bits de R8
    movzx r8, sil       ; Isola o byte 'c' (parte baixa de RSI)
    mov r9, r8
    shl r9, 8
    or  r8, r9          ; R8 tem 16 bits duplicados (0xCCCC)
    mov r9, r8
    shl r9, 16
    or  r8, r9          ; R8 tem 32 bits duplicados (0xCCCCCCCC)
    mov r9, r8
    shl r9, 32
    or  r8, r9          ; R8 tem 64 bits duplicados (0xCCCCCCCCCCCCCCCC)

    ; 2. Preencher em QWORDS (64 bits por vez)
    mov rcx, rdx
    shr rcx, 3          ; n / 8
    
    ; Guarda R8 em RAX para usar com 'rep stosq'
    push rax
    mov rax, r8
    cld
    rep stosq           ; Preenche [RDI] com RAX (64-bits)
    pop rax             ; Restaura RAX com o ponteiro original 's'

    ; 3. Preencher a sobra de bytes (n % 8)
    mov rcx, rdx
    and rcx, 7          ; n % 8
    rep stosb           ; Preenche o restante byte a byte

    ret

section .note.GNU-stack noalloc noexec nowrite progbits
