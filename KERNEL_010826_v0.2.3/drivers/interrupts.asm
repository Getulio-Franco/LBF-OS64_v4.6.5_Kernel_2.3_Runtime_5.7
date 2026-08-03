[BITS 64]
SECTION .text

; --- IMPORTAÇÕES ---
extern mouse_handler
extern keyboard_handler 
extern timer_handler
extern syscall_handler    
extern exception_handler
extern serial_irq_handler ; Importação correta do C

; --- EXPORTAÇÕES (Hardware e Syscall) ---
global isr_mouse
global isr_keyboard
global isr_timer
global isr_serial         ; Exportação para o C
global syscall_int_handler 

; --- EXPORTAÇÕES (Exceções 0-31) ---
%assign i 0
%rep 32
    global isr%+i
%assign i i+1
%endrep

; -------------------------------------------------------------------------
; MACROS DE CONTEXTO
; -------------------------------------------------------------------------
%macro PUSH_ALL 0
    push rax ; [rsp + 112]
    push rbx ; [rsp + 104]
    push rcx ; [rsp + 96]
    push rdx ; [rsp + 88]
    push rsi ; [rsp + 80]
    push rdi ; [rsp + 72]
    push rbp ; [rsp + 64]
    push r8  ; [rsp + 56]
    push r9  ; [rsp + 48]
    push r10 ; [rsp + 40]
    push r11 ; [rsp + 32]
    push r12 ; [rsp + 24]
    push r13 ; [rsp + 16]
    push r14 ; [rsp + 8]
    push r15 ; [rsp + 0]
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; -------------------------------------------------------------------------
; STUB COMUM PARA EXCEÇÕES
; -------------------------------------------------------------------------
isr_common_stub:
    PUSH_ALL          
    mov rdi, rsp      
    call exception_handler
    POP_ALL           
    add rsp, 16       
    iretq             

; -------------------------------------------------------------------------
; MACROS PARA EXCEÇÕES
; -------------------------------------------------------------------------
%macro ISR_NOERRCODE 1
isr%1:
    cli
    push qword 0      
    push qword %1     
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
isr%1:
    cli
    push qword %1     
    jmp isr_common_stub
%endmacro

; --- DEFINIÇÃO DOS 32 VETORES ---
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; -------------------------------------------------------------------------
; HARDWARE INTERRUPTS (IRQs)
; -------------------------------------------------------------------------
isr_timer:
    push qword 0
    push qword 32
    PUSH_ALL          
    
    mov rdi, rsp      
    call timer_handler  
    mov rsp, rax      

    mov rax, [rsp + 144]
    and rax, 3          
    cmp rax, 3          
    jne .skip_user_segments
    
    mov ax, 0x23      
    mov ds, ax
    mov es, ax
    mov fs, ax    
    mov gs, ax    
    jmp .finish_timer

.skip_user_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax

.finish_timer:
    mov al, 0x20      
    out 0x20, al
    
    POP_ALL             
    add rsp, 16       
    iretq  

isr_keyboard:
    push qword 0
    push qword 33
    PUSH_ALL          
    mov rdi, rsp
    call keyboard_handler
    mov al, 0x20      
    out 0x20, al
    POP_ALL
    add rsp, 16       
    iretq

; --- BLOCO DA SERIAL CORRIGIDO ---
isr_serial:
    push qword 0          
    push qword 36         
    PUSH_ALL              
    mov rdi, rsp
    call serial_irq_handler  ; Erro de sintaxe corrigido (sem a palavra extern)

    ; Padronizamos o EOI direto no Hardware para máxima estabilidade
    mov al, 0x20
    out 0xA0, al           ; Avisa o PIC Slave (Porto 0xA0)
    out 0x20, al           ; Avisa o PIC Master (Porto 0x20)       

    POP_ALL
    add rsp, 16
    iretq

isr_mouse:
    push qword 0          
    push qword 44         
    PUSH_ALL              
    mov rdi, rsp
    call mouse_handler    
    mov al, 0x20
    out 0xA0, al          
    out 0x20, al          
    POP_ALL                
    add rsp, 16           
    iretq                 

; -------------------------------------------------------------------------
; >>> ÁREA DE MUDANÇA CRÍTICA: SYSCALL COMPATIBILITY LAYER <<<
; -------------------------------------------------------------------------
syscall_int_handler:
    push qword 0          
    push qword 128        
    PUSH_ALL              

    mov rdi, [rsp + 112]  
    mov rsi, [rsp + 72]   
    mov rdx, [rsp + 80]   
    mov rcx, [rsp + 88]   
    mov r8,  [rsp + 96]   
    mov r9,  [rsp + 56]   

    call syscall_handler
    mov [rsp + 112], rax 

    POP_ALL
    add rsp, 16
    iretq
