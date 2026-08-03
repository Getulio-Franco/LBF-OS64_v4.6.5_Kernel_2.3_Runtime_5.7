/* user.ld - Linker Script para programas de usuário 64-bit */
ENTRY(_start)

SECTIONS
{
    /* Endereço base: 0x401000 (Primeira página é NULL) */
    . = 0x401000;

    /* --- SEÇÃO DE CÓDIGO (Read-Execute) --- */
    .text : ALIGN(4K) {
        _text_start = .;
        
        /* Prioridade máxima para a entrada do programa */
        KEEP(*(.text.entry)) 
        
        *(.text)
        *(.text.*)
        
        . = ALIGN(4K);
        _text_end = .;
    }

    /* --- SEÇÃO DE DADOS SOMENTE LEITURA (Strings e Constantes) --- */
    .rodata : ALIGN(4K) {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4K);
        _rodata_end = .;
    }

    /* --- SEÇÃO DE DADOS INICIALIZADOS (Variáveis com valor) --- */
    .data : ALIGN(4K) {
        _data_start = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4K);
        _data_end = .;
    }

    /* --- SEÇÃO BSS (Dados zerados) --- */
    .bss : ALIGN(4K) {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        . = ALIGN(4K);
        _bss_end = .;
    }

    /* Define o símbolo de fim para o malloc/sbrk saberem onde começar */
    _end = .;

    /* --- LIMPEZA DE LIXO --- */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.metadata)
        *(.interp)
    }
}
