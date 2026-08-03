#include "ahci_hal.h"
#include "ahci_pci.h"
#include "ahci_vmm.h"
#include "ahci_reset.h"
#include "ahci_mem.h"
#include "ahci_cmd.h"
#include "../../util/string.h"
#include "../../drivers/video.h"

#define MAX_SATA_DISKS 4

// Estrutura interna para mapear portas físicas a IDs lógicos de armazenamento
typedef struct {
    volatile ahci_port_reg_t* regs;
    int numero_porta;
    bool presente;
} ahci_disk_t;

// Estado global encapsulado expandido para suportar múltiplos discos
static struct {
    ahci_disk_t discos[MAX_SATA_DISKS];
    int total_discos;
    bool inicializado;
} g_ahci_system = {{{0, 0, false}}, 0, false};

bool ahci_hal_inicializar(void) {
    terminal_print("AHCI_HAL: Iniciando sequencia de boot do driver SATA...\n");

    // Reinicializa o estado lógico
    g_ahci_system.total_discos = 0;
    g_ahci_system.inicializado = false;
    for(int i = 0; i < MAX_SATA_DISKS; i++) {
        g_ahci_system.discos[i].presente = false;
    }

    // Passo 1: Detectar via PCI
    ahci_pci_info_t pci = ahci_pci_detectar();
    if (!pci.encontrado) {
        terminal_print("AHCI_HAL: Abortado. Nenhuma controladora PCI encontrada.\n");
        return false;
    }

    // Passo 2: Mapear BAR5 no VMM com Cache Disable
    void* abar = ahci_vmm_mapear_abar(pci.bar5_phys);
    if (!abar) {
        terminal_print("AHCI_HAL: Abortado. Falha critica no mapeamento VMM.\n");
        return false;
    }

    // Passo 3: Reset global da controladora e checagem de link eletrico
    volatile ahci_hba_reg_t* hba = (volatile ahci_hba_reg_t*)abar;
    int discos_detectados = ahci_reset_controladora(abar);
    if (discos_detectados <= 0) {
        terminal_print("AHCI_HAL: Abortado. Nenhum disco SATA respondeu ao COMRESET.\n");
        return false;
    }

    // Passo 4: Localizar TODAS as portas com discos e configurar o Rebase de Memória
    for (int p = 0; p < 32; p++) {
        if ((hba->pi & (1 << p)) && (hba->ports[p].sig == 0x00000101)) {
            if (ahci_mem_configurar_porta(&hba->ports[p], p)) {
                int idx = g_ahci_system.total_discos;
                if (idx < MAX_SATA_DISKS) {
                    g_ahci_system.discos[idx].regs = &hba->ports[p];
                    g_ahci_system.discos[idx].numero_porta = p;
                    g_ahci_system.discos[idx].presente = true;
                    g_ahci_system.total_discos++;
                }
            }
        }
    }

    if (g_ahci_system.total_discos > 0) {
        g_ahci_system.inicializado = true;
        terminal_print("AHCI_HAL: Driver SATA operacional. Discos registrados com sucesso!\n");
        return true;
    }

    return false;
}

// ATUALIZADO: Agora aceita o dev_id para saber de qual disco SATA ler
int ahci_hal_ler(uint8_t dev_id, uint64_t lba, uint32_t count, void* buffer) {
    if (!g_ahci_system.inicializado || dev_id >= g_ahci_system.total_discos) {
        return -1; // Driver não inicializado ou ID inválido
    }

    if (!g_ahci_system.discos[dev_id].presente) {
        return -1;
    }

    if (count == 0 || buffer == 0) {
        return -2; // Parâmetros inválidos
    }

    // Estratégia do Bounce Buffer Seguro
    uint64_t bounce_buffer_phys = AHCI_MEM_SAFE_BASE + 0x80000; 
    void* bounce_buffer_virt = (void*)(uintptr_t)bounce_buffer_phys;

    // Dispara a transferência física via DMA usando a porta correspondente ao dev_id
    bool sucesso = ahci_cmd_ler_setores(
        g_ahci_system.discos[dev_id].regs, 
        g_ahci_system.discos[dev_id].numero_porta, 
        lba, 
        count, 
        bounce_buffer_phys
    );

    if (!sucesso) {
        terminal_print("AHCI_HAL: Erro de E/S durante a leitura de setores.\n");
        return -3; 
    }

    memcpy(buffer, bounce_buffer_virt, count * 512);
    return 0; 
}

// ATUALIZADO: Corrigido tipo para const void* para alinhar com a camada abstrata de Storage
int ahci_hal_escrever(uint8_t dev_id, uint64_t lba, uint32_t count, const void* buffer) {
    if (!g_ahci_system.inicializado || dev_id >= g_ahci_system.total_discos) {
        return -1; 
    }

    if (!g_ahci_system.discos[dev_id].presente) {
        return -1;
    }

    if (count == 0 || buffer == 0) {
        return -2; 
    }

    uint64_t bounce_buffer_phys = AHCI_MEM_SAFE_BASE + 0x80000; 
    void* bounce_buffer_virt = (void*)(uintptr_t)bounce_buffer_phys;

    // Cópia segura para memória alinhada de hardware
    memcpy(bounce_buffer_virt, buffer, count * 512);

    extern bool ahci_cmd_escrever_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t phys_addr);

    // Dispara a transferência física usando a porta correspondente ao dev_id
    bool sucesso = ahci_cmd_escrever_setores(
        g_ahci_system.discos[dev_id].regs, 
        g_ahci_system.discos[dev_id].numero_porta, 
        lba, 
        count, 
        bounce_buffer_phys
    );

    if (!sucesso) {
        terminal_print("AHCI_HAL: Erro de E/S durante a escrita de setores.\n");
        return -3; 
    }

    return 0; 
}
