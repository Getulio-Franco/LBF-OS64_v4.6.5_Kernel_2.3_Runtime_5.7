ENTRY(_start)
OUTPUT_FORMAT(binary) /* Gera um .bin puro para o bootloader */

SECTIONS
{
    /* O Kernel será carregado em 0x8000 */
    . = 0x8000;

    .text : AT(0x8000)
    {
        *(.text._start) /* Garante que o ASM venha primeiro */
        *(.text)
    }

    /* Alinha as próximas seções em páginas de 4KB */
    . = ALIGN(4096);
    .rodata : { *(.rodata*) }

    . = ALIGN(4096);
    .data : { *(.data) }

    . = ALIGN(4096);
    .bss :
    {
        *(COMMON)
        *(.bss)
    }

    /* Remove lixo de metadados ELF que o binário não precisa */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
