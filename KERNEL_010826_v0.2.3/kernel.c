/**
 * ============================================================================
 * KERNEL MAIN - PONTO DE ENTRADA DO SISTEMA OPERACIONAL
 * ============================================================================
 */

/* --- Includes do Sistema e Memória --- */
#include "mem/gdt.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/heap.h"
#include "mem/e820.h"

/* --- Drivers de Hardware e Arquitetura --- */
#include "drivers/idt.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "drivers/syscall.h"
#include "drivers/hw/ps2.h"
#include "drivers/pci/barramento_pci.h"

/* --- Vídeo e Interface Gráfica --- */
#include "drivers/video.h"
#include "drivers/desktop.h"
#include "drivers/mouse.h"
#include "drivers/double_buffering.h"
#include "drivers/bga.h"

/* --- Processos e Apps --- */
#include "drivers/proc.h"
#include "drivers/apps.h"
#include "drivers/shell/shell.h"
#include "include/elf.h"

/* --- Sistema de Arquivos e Armazenamento --- */
#include "drivers/vfs.h"
#include "fs/fat32.h"
#include "util/string.h"
#include "drivers/hw/disk.h" 
#include "drivers/hw/ahci_pci.h"
#include "drivers/hw/ahci_vmm.h"
#include "drivers/hw/ahci_reset.h"
#include "drivers/hw/ahci_mem.h"
#include "drivers/hw/ahci_cmd.h"
#include "drivers/hw/ahci_hal.h"
#include "drivers/pd/storage.h"
#include "drivers/pd/ehci_pci.h" 
#include "drivers/pd/ehci_core.h"
#include "drivers/pd/ehci_storage.h"

/* ============================================================================
 * VARIÁVEIS GLOBAIS E EXTERNAS
 * ============================================================================ */

/* Pilha Global do Kernel */
__attribute__((aligned(16))) uint8_t kernel_stack[16384];
uint64_t stack_top = (uint64_t)&kernel_stack[16384];

/* Buffers de Boot e MBR */
uint8_t boot_sector[512];
__attribute__((aligned(4096))) static uint8_t teste_mbr_buffer[512];

/* Flags e Funções de Vídeo/GUI */
extern volatile int vga_ring0_enabled;
extern void db_swap_buffers(void);
extern void desktop_update(void);

/* Funções Externas de Sistema de Arquivos */
extern int fat32_mount(uint8_t dev_id);
extern void fat32_test_early_write(void);

/* ============================================================================
 * ROTINAS DE INICIALIZAÇÃO DE HARDWARE
 * ============================================================================ */

/**
 * @brief Habilita extensões SSE/FPU. 
 * Necessário para cálculos em 64-bit e salvamento de contexto (fxsave).
 */
void init_sse() {
    uint64_t cr0, cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Limpa EM (Emulation)
    cr0 |= (1 << 1);  // Seta MP (Monitor Coprocessor)
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // OSFXSR
    cr4 |= (1 << 10); // OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

/* ============================================================================
 * ENTRY POINT PRINCIPAL
 * ============================================================================ */

void kernel_main() {
    /* 1. Estado Inicial: Garantir que interrupções estejam desligadas no boot */
    __asm__ volatile ("cli"); 

    /* ====================================================================
     * FASE 1: ARQUITETURA BASE E CPU
     * ==================================================================== */
    gdt_init(); 
    init_idt(); 
    init_syscall_msrs();
    init_sse();

    /* ====================================================================
     * FASE 2: GERENCIAMENTO DE MEMÓRIA (FÍSICA E VIRTUAL)
     * ==================================================================== */
    pmm_init(4294967296ULL, (void*)0x200000); // Inicia rastreio de 4GB
    detect_memory_from_e820();                // Marca áreas em uso pela BIOS
    vmm_init_identity();                      // Constrói tabelas de página do Kernel
    heap_init();                              // Libera o kmalloc()
    
    /* ====================================================================
     * FASE 3: SUBSISTEMA DE VÍDEO E INTERFACE
     * ==================================================================== */
    uint32_t fb_raw     = *((uint32_t*)0x508);
    uint16_t pitch_raw  = *((uint16_t*)0x50C);
    uint16_t width_raw  = *((uint16_t*)0x510);
    uint16_t height_raw = *((uint16_t*)0x514);
    uint8_t  bpp_raw    = *((uint8_t*)0x518);

    if (bga_is_available()) {
        bga_set_video_mode(1900, 985, 32); 
        width_raw  = 1900;
        height_raw = 985;
        pitch_raw  = 1900 * 4; 
        bpp_raw    = 32;          
    }
   
    video_init((void*)(uintptr_t)fb_raw, width_raw, height_raw, pitch_raw, bpp_raw);
    desktop_init(width_raw, height_raw); 
    mouse_set_screen_size(width_raw, height_raw);

    /* ====================================================================
     * FASE 4: BARRAMENTOS E PERIFÉRICOS
     * ==================================================================== */
    ps2_bus_init(); 
    pci_iniciar_barramento();

    /* ====================================================================
     * FASE 5: ARMAZENAMENTO E SISTEMA DE ARQUIVOS
     * ==================================================================== */
    storage_init();

    // Inicializa HD SATA (Disco 0)
    if (ahci_hal_inicializar()) {
        storage_register_device(0, "SATA HDD", 200000, 512, (storage_read_func_t)ahci_hal_ler, NULL);
        fat32_mount(0); 
    } else {
        // terminal_print("Kernel: Falha no barramento SATA.\n");
    }

    // Inicializa Controlador USB (Apenas preparação)
    if (ehci_pci_init() && ehci_core_init()) {
        ehci_detectar_dispositivos();
    } else {
        // terminal_print("Kernel: Controlador USB/EHCI nao disponivel.\n");
    }

    // Define HD SATA como raiz ativa
    fat32_mudar_disco_ativo(0);
    
    /* ====================================================================
     * FASE 6: MULTITAREFAS E PROCESSOS (RING 3)
     * ==================================================================== */
    scheduler_init();

    uint64_t k_cr3 = read_cr3();
    
    create_process(task_a, RING0, "Kernel_Task", k_cr3);
    
    uint64_t shell_pid = create_process(shell_task, RING0, "Shell", k_cr3);
    process_t* shell_proc = find_process_by_pid(shell_pid);
    if (shell_proc != NULL) {
        extern process_t* foreground_process; 
        shell_proc->is_foreground = 1;
        foreground_process = shell_proc;
    }
    
    create_process(task_c, RING0, "App_Launcher", k_cr3);
    create_process(task_d, RING0, "App_Exec", k_cr3);

    /* ====================================================================
     * FASE 7: STARTUP DO SISTEMA (TIMER & INTERRUPÇÕES)
     * ==================================================================== */
    timer_init(100); 
 
    // Ativa as interrupções de hardware. O Escalonador assume o controle a partir daqui!
    __asm__ volatile ("sti"); 

    /* Loop de Repouso do Kernel (Halt-Loop) */
    while(1) {
        if (vga_ring0_enabled) {
            // Processa o buffer visual apenas se o GUI ainda não tomou 100% do controle
            db_swap_buffers(); 
        }
        __asm__ volatile("hlt"); // Economiza energia da CPU quando não há interrupções
    }
}
