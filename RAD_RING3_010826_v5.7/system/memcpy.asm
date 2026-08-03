; ==============================================================================
; LBF OS - ROTINA ULTRA-RÁPIDA DE COPIA DE MEMÓRIA (64-BITS)
; ==============================================================================

global memcpy

section .text

; void* fast_memcpy(void* dest, const void* src, unsigned long n);
; Convenção de chamadas x86_64: RDI = dest, RSI = src, RDX = n
memcpy:
    mov rax, rdi        ; Guarda o ponteiro de destino para o retorno em RAX (Padrão do C)
    mov rcx, rdx        ; Passa o total de bytes para o contador RCX
    shr rcx, 3          ; Divide por 8 (Descobre quantos blocos de 64 bits/qwords existem)
    
    cld                 ; Limpa a flag de direção (garante incremento para frente)
    rep movsq           ; Move blocos de 64 bits (QWORDS) de [RSI] para [RDI] instantaneamente

    ; Trata os bytes restantes caso (n % 8) não seja zero
    mov rcx, rdx        
    and rcx, 7          ; Pega o resto da divisão por 8
    rep movsb           ; Move o restante byte a byte

    ret
    
    ;Alinha o binário com os padrões modernos de segurança do Linker
section .note.GNU-stack noalloc noexec nowrite progbits
