global gdt_flush
global enter_user_mode
global get_rsp

section .text

; uint64_t get_rsp(void);
; Retorna o valor atual do ponteiro de pilha (RSP) em RAX (System V ABI)
get_rsp:
    mov rax, rsp
    ret

; void gdt_flush(gdt_ptr_t* gdt_ptr_addr);
; System V ABI: RDI = gdt_ptr_addr
gdt_flush:
    lgdt [rdi]              ; Carrega a nova GDT

    mov rax, rsp
    push 0x10               ; SS: Kernel Data (0x10)
    push rax                ; RSP
    push 0x202              ; RFLAGS (IF=1)
    push 0x08               ; CS: Kernel Code (0x08)
    
    lea rax, [rel .reload_segments]
    push rax                ; RIP
    iretq                   ; Troca CS com segurança para 0x08

.reload_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ax, 0x28            ; 0x28 = (Slot 5 * 8) -> TSS Segment Selector
    ltr ax                  ; Carrega o Task Register

    ret

; void enter_user_mode(uint64_t entry_point, uint64_t user_stack);
; System V ABI: RDI = entry_point, RSI = user_stack
enter_user_mode:
    cli                     ; Desabilita interrupções temporariamente

    mov ax, 0x23            ; DS de Usuário (Ring 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23               ; SS: User Data (0x23)
    push rsi                ; RSP: User Stack
    push 0x202              ; RFLAGS: RFLAGS limpo com IF=1
    push 0x1B               ; CS: User Code (0x1B)
    push rdi                ; RIP: Entry Point

    iretq                   ; Pula para o Ring 3!

section .note.GNU-stack noalloc noexec nowrite progbits
