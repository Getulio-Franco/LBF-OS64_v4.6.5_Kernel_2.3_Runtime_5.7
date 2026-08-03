#include "ahci_vmm.h"
#include "../../mem/vmm.h"  // Verifique se o caminho do seu vmm.h está correto aqui
#include "../../drivers/video.h"

void* ahci_vmm_mapear_abar(uint32_t phys_addr) {
    // Alinha o endereço físico na fronteira de uma página de 4KB (0x1000)
    uint64_t phys_page = (uint64_t)phys_addr & ~0xFFFULL;
    
    // Mapeamento direto (Identidade): Virtual igual ao Físico
    uint64_t virt_page = phys_page;

    // Flags essenciais do seu vmm.h para desativar o cache da CPU nesta área
    uint64_t flags = PAGE_PRESENT | PAGE_PCD | PAGE_PWT;

    // Correção: Passando os 4 argumentos exigidos (adicionado 4096 bytes como tamanho)
    vmm_map_area(virt_page, phys_page, 4096, flags);

    terminal_print("AHCI_VMM: ABAR mapeado com Cache Disable com sucesso.\n");

    // Retorna o ponteiro virtual ajustado com o offset original do BAR5
    return (void*)(uintptr_t)(virt_page | (phys_addr & 0xFFF));
}
