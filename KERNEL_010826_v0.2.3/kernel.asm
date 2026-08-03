; ==========================================================
; KERNEL ENTRY - VERSÃO 1.4 (ESTÁVEL & COMPATÍVEL COM IDT)
; LOCALIZAÇÃO: 0x8000 | TRANSIÇÃO: 32 -> 64 BITS
; Paging temporário em 0x70000 para evitar conflito com 0x1000
; ==========================================================
[BITS 32]

; --- EXPORTAÇÕES PARA O LINKER ---
global _start
global kernel_main_return_point
global syscall_handler_64

; --- IMPORTAÇÕES DO C ---
extern kernel_main
extern stack_top
extern syscall_handler

section .text._start
_start:
    cli
    ; Configuração inicial de segmentos 32-bit
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

; ==========================================================
; 1. PAGINAÇÃO DE BOOT (IDENTITY MAPPING 4GB)
; Objetivo: Criar tabelas em 0x70000 (Longe da PML4 do C em 0x1000)
; ==========================================================
setup_paging:
    mov edi, 0x70000        ; Local das tabelas temporárias do ASM
    mov cr3, edi
    xor eax, eax
    mov ecx, 6144           ; Limpa as tabelas (24KB)
    rep stosd

    ; Estrutura: PML4 -> PDPT -> PDs
    mov dword [0x70000], 0x71003
    mov dword [0x71000], 0x72003
    mov dword [0x71008], 0x73003
    mov dword [0x71010], 0x74003
    mov dword [0x71018], 0x75003

    ; Loop para mapear 4GB em páginas de 2MB (Huge Pages)
    mov edi, 0x72000
    mov eax, 0x00000083     ; Flags: Present + Write + Huge
    mov ecx, 2048           ; 2048 * 2MB = 4GB
.map_loop:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .map_loop

; ==========================================================
; 2. TRANSIÇÃO PARA MODO LONGO
; Objetivo: Ativar PAE, EFER.LME e Paginação
; ==========================================================
enable_long_mode:
    lgdt [gdt64_desc]       ; Carrega GDT de 64 bits

    mov eax, cr4
    or eax, 1 << 5          ; Habilita PAE (Obrigatório para 64-bit)
    mov cr4, eax

    mov ecx, 0xC0000080     ; EFER MSR
    rdmsr
    or eax, 1 << 8          ; LME (Long Mode Enable)
    wrmsr

    mov eax, cr0
    or eax, 1 << 31         ; Ativa Paginação (Entra em Long Mode)
    mov cr0, eax

    jmp 0x08:init_64        ; Salto para o código de 64 bits

; ==========================================================
; 3. MODO 64 BITS
; Objetivo: Chamar o Kernel em C e definir ponto de retorno
; ==========================================================
[BITS 64]
init_64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Configura a pilha definitiva do C (alinhada a 16 bytes)
    mov rsp, [rel stack_top]
    and rsp, -16
    mov rbp, rsp

    call kernel_main

; --- PONTO DE RETORNO (Requisitado pelo idt.c) ---
kernel_main_return_point:
    cli
    ; Recarrega a pilha caso ocorra um retorno inesperado ou exceção fatal
    mov rsp, [rel stack_top]
    and rsp, -16
    hlt
    jmp kernel_main_return_point

; ==========================================================
; 4. HANDLER DE SYSCALL (Interface C <-> ASM)
; ==========================================================
section .text
syscall_handler_64:
    swapgs
    push r11
    push rcx
    push rbp
    
    call syscall_handler
    
    pop rbp
    pop rcx
    pop r11
    swapgs
    o64 sysret

; ==========================================================
; 5. DADOS E GDT 64-BIT
; ==========================================================
section .data
align 16
gdt64:
    dq 0x0000000000000000 ; Null
    dq 0x00AF9A000000FFFF ; Kernel Code (0x08)
    dq 0x00CF92000000FFFF ; Kernel Data (0x10)
    dq 0x00CFF2000000FFFF ; User Data   (0x18)
    dq 0x00AFFA000000FFFF ; User Code   (0x20)
gdt64_end:

gdt64_desc:
    dw gdt64_end - gdt64 - 1
    dd gdt64 ; Endereço da GDT
