/**
 * ============================================================================
 * PHYSICAL MEMORY MANAGER (PMM) - BITMAP ESTRUTURADO
 * ============================================================================
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

// Tamanho padrão da página x86_64
#define PAGE_SIZE 4096 

/**
 * @brief Inicializa o bitmap do gerenciador de memória física.
 * * @param mem_size Tamanho total da RAM detectada (em bytes).
 * @param bitmap_addr Endereço de memória onde o bitmap será armazenado.
 * * NOTA: Inicialmente, todos os blocos são marcados como ocupados (1).
 * O código de boot (E820) deve liberar as regiões usáveis.
 */
void pmm_init(size_t mem_size, void* bitmap_addr);

/**
 * @brief Libera um bloco de 4KB (define o Bit como 0).
 * * @param addr Endereço físico da página a ser liberada.
 */
void pmm_free_block(void* addr);

/**
 * @brief Marca um bloco como ocupado manualmente (define o Bit como 1).
 * * @param addr Endereço físico da página a ser reservada.
 * Útil para reservar áreas do Kernel, buffers de vídeo (LFB) ou IOAPIC.
 */
void pmm_mark_block(void* addr);

/**
 * @brief Aloca a primeira página física de 4KB disponível.
 * * @return void* Endereço físico da página alocada, ou NULL se a RAM estiver cheia.
 * * NOTA: Esta função utiliza o algoritmo Next-Fit (Busca Circular) para maior
 * eficiência em sistemas multitarefa.
 */
void* pmm_alloc_block();

/**
 * @brief Helper para converter endereços em IDs de bloco (opcional)
 */
#define PMM_ADDR_TO_BLOCK(addr) ((uintptr_t)(addr) / PAGE_SIZE)
#define PMM_BLOCK_TO_ADDR(id)   ((void*)((uintptr_t)(id) * PAGE_SIZE))

#endif // PMM_H
