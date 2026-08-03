#include "liblib.h"

// Avisa o compilador que a função main está em outro arquivo (g_tarefa.c)
extern int main();

// Adicione este atributo aqui:
__attribute__((section(".text.entry")))

// O Linker.ld aponta o Entry Point para esta função
void _start() {
    // 1. Alinha a stack em 16 bytes (Exigência do System V ABI para x86_64)
    // Se não alinhar, funções matemáticas (FPU/SSE) disparam General Protection Fault
    __asm__ volatile("andq $-16, %rsp");

    // 2. Chama o código principal do usuário
    main();        

    // 3. Se o main() retornar, mata o processo limpamente
    sys_exit();   

    // 4. Barreira de contenção infinita. A CPU nunca deve passar daqui.
    while(1) {
        __asm__ volatile("hlt");
    }
}
