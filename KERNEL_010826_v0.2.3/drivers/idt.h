#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/**
 * ESTRUTURA DE REGISTRADORES (Contexto da CPU)
 * Esta estrutura mapeia exatamente como o interrupts.asm empilha os dados.
 * O RSP aponta para R15 (o último a ser empurrado no PUSH_ALL).
 */
typedef struct {
    // Registradores salvos pelo PUSH_ALL (Ordem exata da pilha: baixo -> alto)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Dados empurrados pelas macros ISR (ID e Código de Erro)
    uint64_t int_no, err_code;
    
    // Frame de interrupção empurrado automaticamente pelo hardware x86_64
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

/**
 * Funções Principais do Subsistema de Interrupções
 */

// Inicializa o PIC, preenche a IDT e carrega com LIDT. 
// Usamos (void) para garantir que ninguém tente passar argumentos.
void init_idt(void);

/**
 * Configura um gate específico na IDT.
 * @param n Número da interrupção (0-255)
 * @param handler Endereço da função no Assembly
 * @param flags Atributos de acesso (0x8E para Kernel, 0xEE para Usuário)
 */
void set_idt_gate(int n, uint64_t handler, uint8_t flags);

/**
 * Tratador central de exceções chamado pelo interrupts.asm
 */
void exception_handler(registers_t *regs);

/**
 * Funções de Baixo Nível (I/O)
 */
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

/**
 * Ponto de recuperação no Kernel para processos que travarem.
 */
extern void kernel_main_return_point(void);

#endif
