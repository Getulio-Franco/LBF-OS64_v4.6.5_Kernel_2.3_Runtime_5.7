/**
 * ============================================================================
 * VIRTUAL MEMORY MANAGER HEADER - VERSÃO 1.2 (Gráfico Otimizado)
 * ============================================================================
 */

#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/** * @brief Endereço físico da PML4 (Tabela mestre). 
 * Alinhado com o padrão de muitos bootloaders em 0x1000.
 */
#define PML4_ADDR 0x1000

/**
 * @brief Flags de hardware x86_64 para as tabelas de páginas.
 * Estas flags definem como a CPU trata cada bloco de memória.
 */
#define PAGE_PRESENT (1ULL << 0)  // Bit 0: Presente na RAM
#define PAGE_WRITE   (1ULL << 1)  // Bit 1: Leitura/Escrita permitida
#define PAGE_USER    (1ULL << 2)  // Bit 2: Acesso permitido ao Ring 3 (User)
#define PAGE_PWT     (1ULL << 3)  // Bit 3: Page-level Write-Through
#define PAGE_PCD     (1ULL << 4)  // Bit 4: Page-level Cache Disable (0x10) - Essencial para VESA
#define PAGE_ACCESSED (1ULL << 5) // Bit 5: Página foi acessada
#define PAGE_DIRTY   (1ULL << 6)  // Bit 6: Página foi escrita (suja)
#define PAGE_HUGE    (1ULL << 7)  // Bit 7: Página de 2MB (usada em PD/PDPT)
#define PAGE_GLOBAL  (1ULL << 8)  // Bit 8: Página Global (não limpa no CR3 reload)

/**
 * @brief Protótipos das funções de gerenciamento
 */

/* Mapeia uma Huge Page de 2MB (Ideal para Kernel e Framebuffer) */
void vmm_map_page_2mb_ext(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

/* Mapeia uma página padrão de 4KB (Usada por Heap e Aplicativos) */
void vmm_map_page_4kb(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

/* Inicializa o mapeamento de identidade do sistema */
void vmm_init_identity();

/* Verifica se um endereço virtual já possui mapeamento ativo */
int vmm_is_mapped(uint64_t virt_addr);

/* Mapeia uma página em um diretório PML4 específico (Útil para multitarefa) */
void vmm_map_page_to_pml4(uint64_t* pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

/**
 * @brief Funções de baixo nível para manipulação do Registrador de Controle 3
 */
uint64_t read_cr3(void);
void write_cr3(uint64_t cr3);
void vmm_map_area(uint64_t virt_addr, uint64_t phys_addr, size_t size, uint64_t flags);
bool vmm_map_user(uint64_t cr3, uint64_t virt, uint64_t phys);
int handle_page_fault(uint64_t user_cr3, uint64_t fault_address); 

#endif /* VMM_H */
