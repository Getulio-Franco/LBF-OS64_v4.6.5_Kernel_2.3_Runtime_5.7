#include "pmm.h"
#include "util/string.h" // Para o memset, se necessário
#include "drivers/video.h" // Para vga_print_string (Debug)

static uint32_t* bitmap;
static size_t total_blocks;
static size_t bitmap_size; 
static uint32_t last_bit_index = 0; 

/**
 * @brief Inicializa o Gerenciador de Memória Física (PMM)
 */
void pmm_init(size_t mem_size, void* bitmap_addr) {
    bitmap = (uint32_t*)bitmap_addr;
    total_blocks = mem_size / PAGE_SIZE;
    
    // Calcula quantos uint32_t são necessários para cobrir todos os blocos
    bitmap_size = (total_blocks + 31) / 32;

    // Inicia tudo como ocupado (1). 
    // O mapeamento E820 deve liberar as áreas seguras via pmm_free_block.
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFFFFFFFF; 
    }
    
    last_bit_index = 0;
    vga_print_string("\n[PMM] Gerenciador de memoria fisica inicializado.", 0, 39);
}

/**
 * @brief Libera um bloco de memória (marca como 0 no bitmap)
 */
void pmm_free_block(void* addr) {
    uintptr_t phys_addr = (uintptr_t)addr;
    uint32_t block_id = (uint32_t)(phys_addr / PAGE_SIZE);

    if (block_id >= total_blocks) return;

    uint32_t word_index = block_id / 32;
    uint32_t bit_index = block_id % 32;

    // Marca como livre (0)
    bitmap[word_index] &= ~(1U << bit_index);

    /* OTIMIZAÇÃO: Se o bloco liberado estiver em um índice menor do que 
     * o atual, movemos o 'last_bit_index' para lá. Isso garante que as 
     * próximas alocações usem primeiro os "buracos" deixados no início da memória.
     */
    if (word_index < last_bit_index) {
        last_bit_index = word_index;
    }
}

/**
 * @brief Marca um bloco como ocupado (marca como 1 no bitmap)
 */
void pmm_mark_block(void* addr) {
    uintptr_t phys_addr = (uintptr_t)addr;
    uint32_t block_id = (uint32_t)(phys_addr / PAGE_SIZE);

    if (block_id >= total_blocks) return;

    bitmap[block_id / 32] |= (1U << (block_id % 32));
}

/**
 * @brief Aloca um bloco de memória física (Busca Circular / Next-Fit)
 * @return Endereço físico da página alocada ou NULL se houver falta de memória.
 */
void* pmm_alloc_block() {
    if (!bitmap) {
        vga_print_string("\n[PMM ERROR] Bitmap nao inicializado!", 0, 39);
        return NULL;
    }

    // Percorremos o bitmap partindo de onde paramos da última vez
    for (uint32_t i = 0; i < bitmap_size; i++) {
        uint32_t index = (last_bit_index + i) % bitmap_size;
        uint32_t word = bitmap[index];

        // Se o uint32_t não estiver todo em 1 (cheio)
        if (word != 0xFFFFFFFF) {
            uint32_t free_bit;

            // BSF encontra o bit 0 (livre) instantaneamente
            __asm__ volatile(
                "bsf %1, %0"
                : "=r"(free_bit)
                : "r"(~word)
            );

            uint32_t block_id = (index * 32) + free_bit;

            if (block_id >= total_blocks) {
                vga_print_string("\n[PMM ERROR] Bloco fora do limite total da RAM!", 0, 39);
                return NULL; // Acabou a RAM real
            }

            // MARCA O BLOCO COMO OCUPADO
            bitmap[index] |= (1U << free_bit);
            
            // Otimização Next-Fit
            last_bit_index = index;

            return (void*)((uintptr_t)block_id * PAGE_SIZE);
        }
    }

    vga_print_string("\n[PMM ERROR] Out of Memory (OOM)! Nao ha blocos livres.", 0, 39);
    return NULL; // Out of Memory
}
