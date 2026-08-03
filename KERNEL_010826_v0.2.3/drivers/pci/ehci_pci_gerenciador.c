#include "ehci_pci_gerenciador.h"
#include "barramento_pci.h" // Certifique-se de que pci_procurar_dispositivo e pci_read_config estão aqui
#include "../video.h"          
#include "../../util/string.h" 
#include "../../mem/vmm.h"     

// Definição das variáveis exclusivas do gerenciador para evitar conflito na linkagem
uintptr_t ehci_base_virt_info = 0;
volatile ehci_regs_t* ehci_regs_info = 0;

int ehci_pci_init_info(void) {
    pci_device_t dev;

    // Ajustado para checar explicitamente o retorno numérico (0 = falha)
    if (pci_procurar_dispositivo(0x0C, 0x03, 0x20, &dev) == 0) {
        terminal_print("EHCI_GERENCIADOR: Controladora EHCI nao disponivel no cache.\n");
        return 0;
    }

    terminal_print("EHCI_GERENCIADOR: Vinculando informacoes EHCI...\n");

    // Chamadas corrigidas para usar as funções reais do barramento_pci.h
    uint32_t pci_cmd = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x04);
    pci_write_config_info(dev.bus, dev.slot, dev.func, 0x04, pci_cmd | 0x06);

    uint32_t bar0 = pci_read_config_info(dev.bus, dev.slot, dev.func, 0x10);
    uintptr_t ehci_base_phys = bar0 & 0xFFFFFFF0;

    if (ehci_base_phys == 0) {
        terminal_print("EHCI_GERENCIADOR: Erro fatal! BAR0 invalida.\n");
        return 0;
    }

    uint64_t phys_page = (uint64_t)ehci_base_phys & ~0xFFFULL;
    uint64_t virt_page = phys_page;
    vmm_map_area(virt_page, phys_page, 4096, PAGE_PRESENT | PAGE_PCD | PAGE_PWT);
    ehci_base_virt_info = (uintptr_t)(virt_page | (ehci_base_phys & 0xFFF));

    volatile uint8_t* cap_regs = (volatile uint8_t*)ehci_base_virt_info;
    uint8_t cap_length = cap_regs[0]; 
    ehci_regs_info = (volatile ehci_regs_t*)(ehci_base_virt_info + cap_length);

    terminal_print("EHCI_GERENCIADOR: Informacoes mapeadas com sucesso!\n");
    return 1;
}
