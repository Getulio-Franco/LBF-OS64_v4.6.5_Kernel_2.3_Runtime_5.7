/**
 * ============================================================================
 * SYSTEM CALL HANDLER - V3.6 (RING 3 ISOLATION & VESA EDITION)
 * ============================================================================
 */

#include "drivers/syscall.h"
#include "drivers/video.h"
#include "drivers/timer.h"
#include "drivers/proc.h"
#include "drivers/keyboard.h"
#include "drivers/io.h"
#include "fs/fat32.h"
#include "fs/fat32_file.h"
#include "fs/fat32_xcopy.h"
#include "fs/fat32_dir.h"
#include "mem/heap.h"
#include "mem/vmm.h"
#include "mem/pmm.h"
#include "util/string.h"
#include "include/elf.h"
#include "drivers/hw/disk.h"
#include "drivers/hw/serial.h"
#include "drivers/hw/pic.h"
#include "drivers/shell/shell_commands.h"

#include "drivers/pd/ehci_pci.h" 
#include "drivers/pd/ehci_core.h" // Garante o acesso ao xhci_ports_init e variáveis
#include "drivers/pd/storage.h"
#include "drivers/pd/ehci_storage.h"

#include "drivers/pci/barramento_pci.h"

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082

extern void syscall_handler_64(); 
extern uint32_t current_dir_cluster;
extern process_t* foreground_process;
extern int refresh_screen;

extern volatile int mouse_x;
extern volatile int mouse_y;
extern volatile uint32_t mouse_buttons;
extern uint8_t* lfb_ptr;
extern uint8_t* ram_buffer;      // Backbuffer do kernel
extern uint32_t screen_width;
extern uint32_t screen_height;
extern uint32_t screen_bpp;
extern int screen_pitch;
extern int bpp_bytes;

//static struct usb_device_desc_t live_pendrive_desc;  //aqui para test testinit

// --- Sincronização Real com os Drivers do Kernel ---
extern void set_current_disk_target(int disk_id);
extern SerialPort com1_port;
// Declaração explícita para o compilador achar as assinaturas exatas do seu serial.c
bool serial_available(SerialPort* port);
bool serial_read(SerialPort* port, char* data);
void serial_write(SerialPort* port, char data);

extern int ahci_hal_ler(uint32_t lba, uint32_t count, void* buffer);
extern int fat32_mount(uint8_t dev_id);

// Coloque isso no topo do seu syscall.c, fora de qualquer função:
extern char pendente_exec_path[128];
extern volatile bool tem_pedido_exec;

// Externs da nova arquitetura task_d
extern volatile int g_exec_pending_flag;
extern char g_pending_elf_path[128];
extern uint64_t g_last_exec_pid;

volatile uint64_t pai_solicitante_pid = 0; // Guarda quem chamou a syscall

void terminal_print_hex8(uint8_t num);
void terminal_print_hex16(uint16_t num);

// Protótipo da função implementada na Parte 1 (video.c)
extern uint32_t kdebug_read_bytes(char* dest_buf, uint32_t max_bytes);

// Adaptador de leitura para o storage unificado
static int usb_read_adapter(uint32_t lba, uint32_t count, void* buffer) {
    // Repassa o comando para o pendrive que está fixado no endereço USB 1
    return ehci_storage_read_sectors(1, lba, count, buffer);
}

// Adaptador de escrita para o storage unificado
static int usb_write_adapter(uint32_t lba, uint32_t count, const void* buffer) {
    return ehci_storage_write_sectors(1, lba, count, buffer);
}

void init_syscall_msrs() {
    uint32_t low, high;
    uint64_t handler_addr = (uint64_t)syscall_handler_64;
    
    low = (uint32_t)handler_addr;
    high = (uint32_t)(handler_addr >> 32);
    __asm__ volatile ("wrmsr" : : "c"(MSR_LSTAR), "a"(low), "d"(high));

    uint32_t star_high = (0x10 << 16) | 0x08; 
    __asm__ volatile ("wrmsr" : : "c"(MSR_STAR), "a"(0), "d"(star_high));

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    low |= 1; 
    __asm__ volatile ("wrmsr" : : "c"(MSR_EFER), "a"(low), "d"(high));
}

uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                          uint64_t arg3, uint64_t arg4, uint64_t arg5) {

    switch (syscall_num) {
        
        // --- 1. VFS / I/O GENÉRICO ---
        case SYS_OPEN:  
            if (!arg1) return (uint64_t)-1;
            return (uint64_t)sys_open((char*)arg1);
            
        case SYS_READ:  
            return (uint64_t)sys_read((int)arg1, (uint8_t*)arg2, (uint32_t)arg3);
       
        case SYS_WRITE: {
            int fd = (int)arg1;
            char* buf = (char*)arg2;
            size_t count = (size_t)arg3;

            if ((uint64_t)buf >= 0x8000000000000000ULL) return 0;

            // Em vez de processar aqui, delega para a função real do VFS
            return sys_write(fd, (uint8_t*)buf, count);
        }
        
        // ====================================================================
        // SYSCALL: OPEN SERIAL (Instancia e configura o hardware sob demanda)
        // ====================================================================
        case SYS_SERIAL_OPEN: {
            int port_num = (int)arg1;     
            uint32_t baud = (uint32_t)arg2; 

            if (port_num < 1 || port_num > 4) return (uint64_t)-1;
            
            int idx = port_num - 1; 

            // === TRAVA DE SEGURANÇA MULTIPROCESSAMENTO ===
            // Se a variável booleana já for true, rejeita o segundo processo!
            if (system_serial_ports[idx].in_use) {
                return (uint64_t)-2; // Retorna um erro específico (Porta Ocupada)
            }

            uint16_t base_addr = 0;
            uint8_t irq_num = 0;

            if (port_num == 1) { base_addr = 0x3F8; irq_num = 4; }
            else if (port_num == 2) { base_addr = 0x2F8; irq_num = 3; }
            else if (port_num == 3) { base_addr = 0x3E8; irq_num = 4; }
            else if (port_num == 4) { base_addr = 0x2E8; irq_num = 3; }

            __asm__ volatile("cli");

            int init_status = driver_serial_init(&system_serial_ports[idx], base_addr, irq_num, baud);

            if (init_status == 0) {
                // Ativa o Lock definitivo
                system_serial_ports[idx].in_use = true;
                
                pic_unmask(irq_num); 
                __asm__ volatile("sti");
                return (uint64_t)idx; 
            }

            __asm__ volatile("sti");
            return (uint64_t)-1; 
        }
        
        // ====================================================================
        // SYSCALL: WRITE SERIAL (Agora parametrizada por Porta)
        // ====================================================================
        case SYS_SERIAL_WRITE: {
            int port_id = (int)arg1; // Recebe o Handle do Ring 3
            char* buf = (char*)arg2;
            size_t count = (size_t)arg3;

            if (port_id < 0 || port_id >= MAX_SERIAL_PORTS) return 0;
            if (!system_serial_ports[port_id].in_use) return 0;
            if (!buf || (uint64_t)buf >= 0x8000000000000000ULL) return 0;

            // Envia para os registradores da porta solicitada
            for (size_t i = 0; i < count; i++) {
                serial_write(&system_serial_ports[port_id], buf[i]); 
            }
            return count;
        }

        // ====================================================================
        // SYSCALL: READ SERIAL (Agora parametrizada por Porta)
        // ====================================================================
        case SYS_SERIAL_READ: {
            int port_id = (int)arg1; // Recebe o Handle do Ring 3
            uint8_t* buf = (uint8_t*)arg2;
            uint32_t count = (uint32_t)arg3;
            
            if (port_id < 0 || port_id >= MAX_SERIAL_PORTS) return (uint64_t)-1;
            if (!system_serial_ports[port_id].in_use) return (uint64_t)-1;
            if (!buf || (uint64_t)buf >= 0x8000000000000000ULL) return (uint64_t)-1;

            uint32_t bytes_lidos = 0;
            char caractere_temporario;
            
            // Lê estritamente do buffer circular da porta solicitada
            while (bytes_lidos < count && serial_available(&system_serial_ports[port_id])) {
                if (serial_read(&system_serial_ports[port_id], &caractere_temporario)) {
                    buf[bytes_lidos] = (uint8_t)caractere_temporario;
                    bytes_lidos++;
                }
            }
            return (uint64_t)bytes_lidos;
        }
        
        // ====================================================================
        // SYSCALL: CLOSE SERIAL (Agora fechando a Porta)
        // ====================================================================
        
        case SYS_SERIAL_CLOSE: {
            int idx = (int)arg1; // Recebe o ID da porta (retornado pelo OPEN)

            if (idx < 0 || idx > 3) return (uint64_t)-1;

            // Se a porta nem estava aberta, não há o que fechar
            if (!system_serial_ports[idx].in_use) return 0;

            __asm__ volatile("cli");

            // 1. Desabilita as interrupções direto no chip UART 16550A
            outb(system_serial_ports[idx].port_base + 1, 0x00); // Zera o IER
            io_wait();

            // 2. Avisa o PIC para ignorar essa IRQ a partir de agora (Mascara novamente)
            // COM1/COM3 usam IRQ4, COM2/COM4 usam IRQ3
            uint8_t irq_num = system_serial_ports[idx].irq;
            pic_mask(irq_num); // Função inversa do pic_unmask

            // 3. Limpa os buffers de software para a próxima abertura
            system_serial_ports[idx].rx_head = 0;
            system_serial_ports[idx].rx_tail = 0;

            // 4. Libera o LOCK para outros processos do S.O. usarem
            system_serial_ports[idx].in_use = false;

            __asm__ volatile("sti");
            return 0; // Sucesso
        }
                
        case SYS_PS:
            list_processes(); 
            return 0;

        /* ====================================================================\n         
         * SYSCALL 25: SYS_GET_PS_DATA (PROCESSO INFO PROTEGIDO)
         * arg1 = TProcessInfo* user_buffer
         * arg2 = int max_items
         * ==================================================================== */
        case SYS_GET_PS_DATA: {
            TProcessInfo* user_buf = (TProcessInfo*)arg1;
            int max_items = (int)arg2;

            if (!user_buf || max_items <= 0) return 0;

            // Segurança contra escrita fora da memória de usuário
            if ((uint64_t)user_buf >= 0x8000000000000000ULL) return 0;

            // Previne estourar buffers maiores do que o limite seguro (evita crash do 16º processo)
            if (max_items > 32) max_items = 32;

            return get_process_info_list(user_buf, max_items);
        }
           
         case SYS_CLEAR: {
            // Exemplo de blindagem (se o seu kernel usa cli/sti ou funções equivalentes)
            asm volatile("cli"); 
    
            terminal_clear(); 
            refresh_screen = 1;
    
            asm volatile("sti");
            return 0;
        }

        case SYS_MEM_INFO:
            if (!arg1 || !arg2) return (uint64_t)-1;
            get_system_memory_info((size_t*)arg1, (size_t*)arg2);
            return 0;

        // --- 2. FAT32 / DISCO ---
        case SYS_FATLS: 
            fat32_list_directory(current_dir_cluster); 
            return 0;
                  
        case SYS_FATCAT: 
            return (arg1) ? (uint64_t)fat32_display_file((const char*)arg1) : (uint64_t)-1;

        case SYS_FATRM:
            if (!arg1) return (uint64_t)-1;
            // Chama a função do seu driver FAT32 para deletar
            return (uint64_t)fat32_delete_file((const char*)arg1);

        case SYS_FATCP: {
            const char* src_path = (const char*)arg1;
            const char* dest_path = (const char*)arg2;

            if (!src_path || !dest_path) return (uint64_t)-1;

            // Chame diretamente a função especializada do seu xcopy.c!
            // Certifique-se de ter o #include "fs/fat32_copy.h" no topo do syscall.c
            return (uint64_t)fat32_copy_file(src_path, dest_path);
        }
        
        case SYS_MKDIR: {
            const char* dir_name = (const char*)arg1;
            if (!dir_name) return (uint64_t)-1;
                 return (uint64_t)fat32_create_dir(dir_name);
        }
        
        case SYS_SET_DRIVE: { // 28 mudar de unidade de disco.
            uint8_t dev_id = (uint8_t)arg1; // Pega o ID do disco que o Ring 3 enviou
    
            // Valida se o ID está dentro dos limites suportados
            if (dev_id >= 3) {
                return 0; // Falha
            }
    
            // Chama a função interna do Kernel que altera as variáveis globais da FAT32
            fat32_mudar_disco_ativo(dev_id);
    
            return 1; // Sucesso
        }
        
        case SYS_FATAPPEND: {
            const char* filename = (const char*)arg1;
            uint8_t* data = (uint8_t*)arg2;
            uint32_t len = (uint32_t)arg3;
    
            // Filtro protetor de memória
            if ((uint64_t)filename >= 0x8000000000000000ULL || (uint64_t)data >= 0x8000000000000000ULL) return -1;
    
            return fat32_append_file(filename, data, len);
        }
        
        
        case SYS_FATRENAME: { // 14 SYS_FATRENAME
            const char* old_name = (const char*)arg1;
            const char* new_name = (const char*)arg2;
            if ((uint64_t)old_name >= 0x8000000000000000ULL || (uint64_t)new_name >= 0x8000000000000000ULL) return -1;
    
                // Aqui você chamará a função do seu driver fat32 futuramente:
                return fat32_rename(old_name, new_name);
            return 0; 
        }

        case SYS_FATSTAT: { // 24 SYS_FATSTAT
            const char* filename = (const char*)arg1;
            file_info_t* user_info = (file_info_t*)arg2;
            
            // Proteção de memória contra ponteiros maliciosos do Ring 3
            if ((uint64_t)filename >= 0x8000000000000000ULL || (uint64_t)user_info >= 0x8000000000000000ULL) {
                return -1;
            }
            if (!filename || !user_info) {
                return -1;
            }

            // Criamos uma estrutura local segura no espaço do Kernel (Ring 0)
            file_info_t kernel_info;
            
            // Chama a função interna do driver FAT32
            int resultado = fat32_stat(filename, &kernel_info);
            if (resultado != 0) {
                return resultado; // Retorna o erro caso o arquivo não exista (-1)
            }

            // Copia com segurança os dados coletados para a memória do Ring 3
            user_info->size = kernel_info.size;
            user_info->attributes = kernel_info.attributes;

            return 0; // Sucesso!
        }        
        
        case SYS_CHDIR: {
            const char* target_dir = (const char*)arg1;
            if (!target_dir) return (uint64_t)-1;

            fat32_directory_entry_t entry;

            // 1. Tratamento especial se o usuário digitar exatamente ".."
            if (strcmp(target_dir, "..") == 0) {
                // Busca o registro ".." dentro do diretório atual para saber quem é o pai
                if (fat32_find_entry("..", &entry) == 0) {
                    uint32_t parent_cluster = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
                    
                    // REGRA DE OURO DO FAT32: Se o cluster pai for 0, significa que o pai é a RAIZ (Cluster 2)
                    if (parent_cluster == 0 || parent_cluster == 2) {
                        current_dir_cluster = 2; // Força apontar para a raiz do sistema
                    } else {
                        current_dir_cluster = parent_cluster; // Vai para a subpasta pai anterior
                    }
                    return 0;
                }
                return (uint64_t)-1;
            }

            // 2. Navegação normal para frente (entrar em pastas como cd sistema)
            if (fat32_find_entry(target_dir, &entry) == 0) {
                if (entry.attributes & 0x10) { // Verifica se é um diretório
                    uint32_t new_cluster = ((uint32_t)entry.cluster_high << 16) | entry.cluster_low;
                    
                    // Proteção caso o cluster venha zerado por qualquer motivo
                    if (new_cluster == 0) {
                        current_dir_cluster = 2;
                    } else {
                        current_dir_cluster = new_cluster;
                    }
                    return 0;
                }
            }
            return (uint64_t)-1;
        }
        
        case SYS_FATREADDIR: {
             int index         = (int)arg1;
             char* user_buf    = (char*)arg2;
             file_info_t* info = (file_info_t*)arg3;

             if (!user_buf || !info) return (uint64_t)-1;
                  uint32_t size = 0;
                  uint8_t attr = 0;
                  char local_name[16];
                  // Chama a função que criamos acima usando o cluster do diretório atual do processo
                  int resultado = fat32_get_entry_by_index(current_dir_cluster, index, local_name, &size, &attr);

              if (resultado == 1) {
                    // Copia os dados do espaço do Kernel para as structs do espaço de usuário
                   strcpy(user_buf, local_name);
                   info->size = size;
                   info->attributes = attr;
              }

              return (uint64_t)resultado; 
        }
        
        // --- 3. GESTÃO DE PROCESSOS ---(ASSÍNCRONO LIMPO) ---
       /* ====================================================================\n         
         * SYSCALL 19: SYS_EXEC (ASSÍNCRONA VIA TASK_D)
         * arg1 = const char* filename
         * ==================================================================== */

      case SYS_EXEC: {
            if (!arg1) return (uint64_t)-1;

            if (g_exec_pending_flag == 1) {
                return (uint64_t)-2; // Ocupado no momento
            }

            const char* user_path = (const char*)arg1;
            
            if ((uint64_t)user_path >= 0x8000000000000000ULL) {
                return (uint64_t)-1;
            }

            int i = 0;
            while (user_path[i] != '\0' && i < 127) {
                g_pending_elf_path[i] = user_path[i];
                i++;
            }
            g_pending_elf_path[i] = '\0';

            // Mensagem única e estritamente necessária antes de ir para a task_d
            vga_print_string("\n[SYS_EXEC v9_v1 indo para task_d] ", 0, 37);

            g_exec_pending_flag = 1;

            return 0; 
        }
      
     /* case SYS_EXEC: {
            if (!arg1) return (uint64_t)-1;

            // Se já houver um processo em fila para a task_d carregar, evita sobrescrever
            if (g_exec_pending_flag == 1) {
                return (uint64_t)-2; // Ocupado no momento
            }

            const char* user_path = (const char*)arg1;
            
            // Validação de ponteiro do Ring 3
            if ((uint64_t)user_path >= 0x8000000000000000ULL) {
                return (uint64_t)-1;
            }

            // Copia o nome do executável de forma segura
            int i = 0;
            while (user_path[i] != '\0' && i < 127) {
                g_pending_elf_path[i] = user_path[i];
                i++;
            }
            g_pending_elf_path[i] = '\0';

            // Sinaliza para a task_d processar na próxima fatia do Kernel
            g_exec_pending_flag = 1;

            return 0; // Retorno instantâneo para o Ring 3!
        }*/
       
        case SYS_SBRK: {
            intptr_t increment = (intptr_t)arg1; 
            process_t* proc = get_current_process();
    
            if (!proc) return (uint64_t)-1;
            if (proc->heap_end == 0) proc->heap_end = 0x0000700000000000;

            uint64_t old_break = proc->heap_end;
            if (increment == 0) return old_break;

            uint64_t new_break = old_break + increment;
            if (new_break < 0x0000700000000000) return (uint64_t)-1;

            if (increment > 0) {
                uint64_t start_page = (old_break + 4095) & ~4095ULL;
                uint64_t end_page = (new_break + 4095) & ~4095ULL;

                for (uint64_t virt_addr = start_page; virt_addr < end_page; virt_addr += 4096) {
                    void* phys_frame = pmm_alloc_block();
                    if (!phys_frame) return (uint64_t)-1;

                    if (!vmm_map_user(proc->cr3, virt_addr, (uint64_t)phys_frame)) {
                        pmm_free_block(phys_frame); 
                        return (uint64_t)-1;
                    }
                    memset((void*)virt_addr, 0, 4096); 
                }
            } 
            proc->heap_end = new_break;
            return old_break;
        }

        case SYS_EXIT: {
            terminate_current_process();
            return 0;
        }
        
        case SYS_KILL: { // ID 21
            return kill_process((uint64_t)arg1);
        }

        case SYS_GETPID:
            return (uint64_t)get_current_process()->pid;

        // --- 4. SISTEMA E RELÓGIO ---
        case SYS_SLEEP: 
            sys_sleep((uint64_t)arg1);
            return 0;
            
        case SYS_GET_TICKS:  
            return timer_get_ticks(); 

        case SYS_GET_PARAM:
            return sys_get_kernel_data(arg1);
            
        // --- 5. INPUT ---
        case SYS_GET_MOUSE: {
            uint64_t* user_mouse_ptr = (uint64_t*)arg1;
            if ((uint64_t)user_mouse_ptr >= 0x8000000000000000ULL) return 0; 
            user_mouse_ptr[0] = (uint64_t)mouse_x;       
            user_mouse_ptr[1] = (uint64_t)mouse_y;       
            user_mouse_ptr[2] = (uint64_t)mouse_buttons; 
            return 1; 
        }

        case SYS_GET_KEY:
            return (uint64_t)keyboard_pop_char();    

        // ================================================================
        // SYS_VIDEO_FLIP: Exibe ram_buffer do kernel na VRAM
        // ================================================================
        case SYS_VIDEO_FLIP: {
            // Quando arg1 é NULL, apenas faz flush do ram_buffer do kernel
            // Quando arg1 é um buffer, copia do Ring 3 primeiro (compatibilidade)
            
            if (arg1 != 0 && (uint64_t)arg1 < 0x8000000000000000ULL) {
                // Modo antigo: copia do buffer do usuário
                void* user_backbuffer = (void*)arg1;
                size_t total_bytes = screen_pitch * screen_height;
                memcpy(ram_buffer, user_backbuffer, total_bytes);
            }
            // Se arg1 == NULL, apenas faz flush do que já está no ram_buffer
            
            // Exibe o ram_buffer do kernel na VRAM
            //video_flush();
            video_flush();
            return 0;
        }

        case SYS_GET_LFB_CRITICAL_DATA: {
            typedef struct {
                uint64_t lfb_address;
                uint32_t width;
                uint32_t height;
                uint32_t bpp;
                uint32_t pitch;
            } LFBInfo;

            LFBInfo* user_info = (LFBInfo*)arg1;
            if ((uint64_t)user_info >= 0x8000000000000000ULL) return 0;

            user_info->lfb_address = (uint64_t)lfb_ptr; 
            user_info->width       = screen_width;
            user_info->height      = screen_height;
            user_info->bpp         = screen_bpp;
            user_info->pitch       = screen_width * (screen_bpp / 8);
            return 0;
        }
        
        // --- HARDWARE / PCI ---
        case SYS_GET_PCI_DEVICE: {  // 67
            int index = (int)arg1;
            pci_device_t* user_dev = (pci_device_t*)arg2;

            // Validação de segurança para não estourar a tabela do Kernel
            if (index < 0 || index >= pci_device_count || user_dev == NULL) {
                return (uint64_t)-1; // Retorna erro/fim da lista
            }

            // Copia os dados do cache do Kernel para o ponteiro fornecido pelo Shell
            *user_dev = pci_device_list[index];
            return 0; // Sucesso
        }
        
        case SYS_LSPCI_TERMINAL: {  // 68
            int modo = (int)arg1; // Usamos arg1 como o "Modo de Operação"

            // MODO 0: Modo Terminal (Aciona a varredura e impressão nativa do driver)
            if (modo == 0) {
                pci_device_count = 0; 
                pci_iniciar_barramento(); 
                return 0; // Sucesso
            }
            return (uint64_t)-1;
        }
        
        //=================

        case SYS_EHCI: { // Substitua pelo número/macro correto da sua syscall - SYS_TESTINIT:
            terminal_print("\n=== INICIANDO SUBSISTEMA USB E ARMAZENAMENTO ===\n");

            // ---------------------------------------------------------
            // FASE 1: Inicialização do Controlador EHCI (Antigo SYS_TESTUSB)
            // ---------------------------------------------------------
            terminal_print("USB: Inicializando controlador EHCI via PCI...\n");
            if (!ehci_pci_init()) {
                terminal_print("USB: [FALHA] Controlador EHCI nao respondeu.\n");
                return 0; // Aborta se o hardware não estiver presente
            }
            terminal_print("USB: [OK] Controlador EHCI inicializado com sucesso!\n");

            // ---------------------------------------------------------
            // FASE 2: Reset, Detecção e DMA (Antigo SYS_TESTINIT)
            // ---------------------------------------------------------
            if (!ehci_core_init()) {
                terminal_print("USB: [FALHA] Erro na Fase 2 (Core Reset).\n");
                return 0;
            }

            // Varre as portas e executa o reset elétrico automático
            ehci_detectar_dispositivos();

            // Inicializa o mecanismo de DMA em RAM
            if (!ehci_init_async_list()) {
                terminal_print("USB: [FALHA] Erro ao ativar o motor de DMA.\n");
                return 0;
            }

            // Lê a identificação do dispositivo conectado
            ehci_ler_identificacao();
            terminal_print("USB: [OK] Barramento, Portas e DMA operacionais!\n");

            // ---------------------------------------------------------
            // FASE 3: Gerenciamento de Armazenamento (Antigo SYS_TESTSTORAGE)
            // ---------------------------------------------------------
            terminal_print("\n=== REGISTRANDO DISCOS E SISTEMAS DE ARQUIVOS ===\n");

            // Garante que a Central de Discos unificada está limpa e online
            storage_init();

            // Registra primeiro o HD SATA antigo como Disco 0 
            storage_register_device(
                STORAGE_DEV_SATA, 
                "SATA Hard Disk", 
                200000, 
                512, 
                (storage_read_func_t)ahci_hal_ler, 
                NULL
            );

            // Variáveis locais para capturar os dados reais vindos do comando SCSI
            uint32_t usb_max_lba = 0;
            uint32_t usb_block_size = 512;

            // Inicializa o protocolo SCSI dentro do pendrive (Endereço 1)
            if (!ehci_storage_init(1, &usb_max_lba, &usb_block_size)) {
                terminal_print("USB STORAGE: [FALHA] O pendrive nao respondeu aos comandos SCSI.\n");
                // Nota: Retornando 0 aqui, o S.O. desiste de montar o pendrive,
                // mas o disco SATA (Disco 0) já foi registrado na memória.
                return 0; 
            }

            // Registra o Pendrive como DISCO 1 com tamanho real e blocos via SCSI
            storage_register_device(
                STORAGE_DEV_USB, 
                "USB Flash Drive", 
                usb_max_lba,   
                usb_block_size, 
                (storage_read_func_t)usb_read_adapter, 
                (storage_write_func_t)usb_write_adapter
            );
            terminal_print("USB STORAGE: Registrado no gerenciador como Disco 1!\n");

            // ---------------------------------------------------------
            // O GRAND FINALE: Montagem do Sistema de Arquivos
            // ---------------------------------------------------------
            terminal_print("FAT32: Tentando montar o sistema de arquivos no Pendrive...\n");
            
            if (fat32_mount(STORAGE_DEV_USB)) {
                terminal_print("\n[SUCESSO HISTORICO] O seu Kernel montou o FAT32 do Pendrive!\n");
            } else {
                terminal_print("\n[FALHA] O setor MBR ou VBR do pendrive nao continha um FAT32 valido.\n");
            }

            return 1; // Tudo finalizado com sucesso, retorna para o Kernel
        }
        
        /* ====================================================================
         * SYSCALL 69: SYS_READ_KERNEL_LOG
         * arg1 = Ponteiro para o buffer no Ring 3 (char* user_buffer)
         * arg2 = Tamanho máximo do buffer (int max_bytes)
         * ==================================================================== */
        case SYS_READ_KERNEL_LOG: {
            char* user_buffer = (char*)arg1;
            uint32_t max_bytes = (uint32_t)arg2;

            // 1. Validação de segurança de ponteiro de User Space
            if (!user_buffer || max_bytes == 0) {
                return 0;
            }

            // Garante que o ponteiro fornecido pelo aplicativo está em User Space
            if ((uint64_t)user_buffer >= 0x8000000000000000ULL) {
                return 0; 
            }

            // 2. Lê os bytes do buffer de debug do Kernel diretamente para o buffer do Ring 3
            return kdebug_read_bytes(user_buffer, max_bytes);
        }

        //=================

    default:
        return (uint64_t)-1;
    }
}

uint64_t sys_get_kernel_data(uint64_t param_id) {
    switch (param_id) {
        case 1: return 100;           
        case 2: return 1024;          
        case 3: return 768;           
        default: return 0;
    }
}
