#include "e820.h"
#include "mem/pmm.h"
#include "drivers/video.h" // Certifique-se de que aqui tem o draw_string
#include "util/string.h"

void detect_memory_from_e820() {
    // 0x500 continua guardando a quantidade de entradas (2 bytes)
    uint16_t entry_count = *(uint16_t*)0x500; 

    // NOVO ENDEREÇO: A tabela agora começa em 0x2000 para não colidir com o VBE
    e820_entry_t* map = (e820_entry_t*)0x2000;

    // Usando a cor branca (0xFFFFFFFF) para o log no modo gráfico
  //  draw_string(10, 50, "[E820] Mapeando RAM utilizavel...", 0xFFFFFFFF, 1);

    for (uint32_t i = 0; i < entry_count; i++) {
        // Type 1 = RAM Disponível para o sistema
        if (map[i].type == 1) {
            uint64_t start = map[i].base;
            uint64_t end = start + map[i].length;

            // ALINHAMENTO EM PÁGINAS (4KB)
            start = (start + 0xFFF) & ~0xFFFULL;  
            end = end & ~0xFFFULL; 

            if (start >= end) continue;

            // Avança de PAGE_SIZE em PAGE_SIZE
            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                
                // SEGURANÇA MANTIDA: Protege os primeiros 8MB (Boot, VBE buffers, Fontes, Kernel)
                // Somente endereços a partir de 8MB (0x800000) serão liberados para alocação
                if (addr >= 0x800000) { 
                    pmm_free_block((void*)(uintptr_t)addr);
                }
            }
        }
    }
    
   // draw_string(10, 70, "[E820] Memoria configurada com sucesso.", 0xFFFFFFFF, 1);
}
