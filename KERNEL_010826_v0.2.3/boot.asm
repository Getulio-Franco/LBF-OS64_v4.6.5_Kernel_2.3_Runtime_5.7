; ==========================================================
; BOOTLOADER - VERSÃO 1.4 (LOOP DE LEITURA SEGMENTADA LBA)
; LOCALIZAÇÃO: SETOR 1 (MBR) | DESTINO: 0x7C00
; ==========================================================
[BITS 16]
[ORG 0x7C00]

_start:
    ; --- INICIALIZAÇÃO DE REGISTRADORES ---
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [BOOT_DRIVE], dl

; ==========================================================
; 1. CONFIGURAÇÃO DO VÍDEO (VESA GRÁFICO)
; Objetivo: Ativar 1024x768x32bpp e salvar dados para o C
; ==========================================================
set_vesa:
    mov ax, 0x4F02
    mov bx, 0x4118          ; Modo 1024x768x32bpp
    int 0x10

    mov ax, 0x4F01
    mov cx, 0x118           
    mov di, 0x7000          ; Armazena Info VESA em 0x7000 (Longe do 0x1000)
    int 0x10

    ; --- Passando parâmetros para o Kernel C (Endereço 0x500) ---
    mov eax, [0x7028]       ; Framebuffer (LFB)
    mov [0x508], eax
    mov ax, [0x7010]        ; Pitch
    mov [0x50C], ax
    mov ax, [0x7012]        ; Width
    mov [0x510], ax
    mov ax, [0x7014]        ; Height
    mov [0x514], ax
    mov al, [0x7019]        ; Bits Per Pixel
    mov [0x518], al

; ==========================================================
; 2. ATIVAR LINHA A20
; Objetivo: Liberar acesso a endereços de memória acima de 1MB
; ==========================================================
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al

; ==========================================================
; 3. MAPA DE MEMÓRIA (E820)
; Objetivo: Descobrir quanta RAM o PC possui
; ==========================================================
get_memory_map:
    mov di, 0x2000          ; Mapa de memória armazenado em 0x2000
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.loop:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .done
    add di, 24
    inc bp
    test ebx, ebx
    jnz .loop
.done:
    mov [0x500], bp         ; Salva a quantidade de entradas em 0x500

; ==========================================================
; 4. LEITURA DO KERNEL (LBA COM LOOP SEGMENTADO)
; Objetivo: Ler o kernel do disco em blocos e carregar em 0x8000
; ==========================================================
load_kernel:
    ; Queremos ler 256 setores no total (128KB).
    ; Lemos em 8 blocos de 32 setores (8 * 32 = 256 setores).
    ; Se o SO crescer mais, basta aumentar este contador!
    mov cx, 8               

.disk_loop:
    push cx                 ; Guarda o contador do loop principal

    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, dap             ; Endereço da estrutura DAP
    int 0x13
    jc disk_error

    ; --- Atualiza o LBA para a próxima leitura ---
    ; Adiciona 32 setores ao LBA inicial
    mov eax, [dap_lba]
    add eax, 32
    mov [dap_lba], eax

    ; --- Atualiza o Segmento de Memória de Destino ---
    ; 32 setores * 512 bytes = 16384 bytes (16KB)
    ; Em notação de segmento (parágrafos de 16 bytes): 16384 / 16 = 0x0400
    mov ax, [dap_segment]
    add ax, 0x0400
    mov [dap_segment], ax

    pop cx                  ; Restaura o contador
    loop .disk_loop         ; Decrementa CX e pula se não for zero

; ==========================================================
; 5. SALTO PARA MODO PROTEGIDO (32 BITS)
; Objetivo: Desativar modo Real e entrar em 32 bits
; ==========================================================
switch_to_pm:
    cli
    lgdt [gdt32_descriptor]
    mov eax, cr0
    or eax, 1                ; Ativa o bit PE (Protected Mode)
    mov cr0, eax
    jmp 0x08:init_pm        ; Salto longo para limpar o pipeline de 16 bits

; ==========================================================
; 6. AMBIENTE 32 BITS
; Objetivo: Configurar segmentos e pular para o Kernel
; ==========================================================
[BITS 32]
init_pm:
    mov ax, 0x10            ; Data Segment da GDT
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000        ; Pilha temporária protegida
    jmp 0x08:0x8000         ; Salta para o início do kernel em 0x8000

disk_error: 
    ; Se der erro de disco, trava piscando a tela ou parado
    jmp $

; ==========================================================
; ESTRUTURAS DE DADOS DO BOOTLOADER
; ==========================================================
BOOT_DRIVE db 0

align 4
dap:
    db 0x10                 ; Tamanho da estrutura DAP
    db 0                    ; Reservado (Sempre 0)
dap_sectors:
    dw 32                   ; Lendo blocos pequenos e estáveis de 32 setores (16KB)
dap_offset:
    dw 0x8000               ; Offset permanece fixo em 0x8000
dap_segment:
    dw 0x0000               ; O segmento avança dinamicamente via código
dap_lba:
    dq 1                    ; Setor LBA inicial (avança dinamicamente)

gdt32_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF   ; Code Descriptor
    dq 0x00CF92000000FFFF   ; Data Descriptor
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

times 510-($-$$) db 0
dw 0xAA55
