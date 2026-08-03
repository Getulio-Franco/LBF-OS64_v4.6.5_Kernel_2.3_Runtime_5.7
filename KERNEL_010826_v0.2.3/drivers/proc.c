#include "drivers/proc.h"
#include "drivers/video.h"
#include "drivers/timer.h"
#include "util/string.h"
#include "mem/heap.h"
#include "mem/vmm.h"
#include "drivers/shell/shell_commands.h"
#include "drivers/keyboard.h"

// Seletores da GDT
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x1B 
#define GDT_USER_DATA   0x23 
//#define USER_STACK_VADDR 0x2000000

// ====================================================================
// VARIÁVEIS GLOBAIS DE PROCESSOS E PONTE TASK_D <-> SYSCALL
// ====================================================================
process_t* head = NULL;            
process_t* current_process = NULL; 
process_t* foreground_process = NULL; 
static uint64_t next_pid = 1; 

// Ponte Assíncrona para Execução de .ELF (Syscall -> task_d)
volatile int g_exec_pending_flag = 0;
char g_pending_elf_path[128] = {0};
uint64_t g_last_exec_pid = 0;

// Funções e referências externas do Kernel
extern void tss_set_stack(uint64_t stack);
extern void elf_unload_failed_pml4(uint64_t* pml4_phys);
extern uint64_t read_cr3(void);
extern void write_cr3(uint64_t cr3);
extern volatile uint64_t system_ticks;
extern uint32_t global_ui_color;

extern int term_cursor_x;
extern int term_cursor_y;
extern uint32_t term_color;

// ====================================================================
// HELPERS DE IMPRESSÃO DENTRO DO KERNEL
// ====================================================================
static void proc_print(const char* str) {
    if (!str) return;
    while (*str) terminal_putc(*str++);
}

// ====================================================================
// INICIALIZAÇÃO E LISTAGEM DE PROCESSOS
// ====================================================================
void scheduler_init(void) {
    head = NULL;
    current_process = NULL;
    foreground_process = NULL;
    next_pid = 1; 
    g_exec_pending_flag = 0;
    g_last_exec_pid = 0;
}

void list_processes(void) {
    process_t* curr = head;
    int safety_limit = 0;

    term_color = 0xFFFF00; 
    terminal_print("\nPID  NAME          STATE       RING  RIP                CR3\n");
    terminal_print("---------------------------------------------------------------------------\n");
    term_color = 0xFFFFFFFF;

    while (curr != NULL && safety_limit < 50) {
        safety_limit++;

        // 1. PID
        draw_dec(term_cursor_x, term_cursor_y, curr->pid, term_color);
        
        // 2. Nome
        term_cursor_x = 45;
        if (curr->name[0] >= 32 && curr->name[0] <= 126) {
            terminal_print(curr->name);
        } else {
            terminal_print("Unknown");
        }

        // 3. Estado
        term_cursor_x = 140;
        if (curr->state == PROCESS_RUNNING) {
            term_color = 0x00FF00; terminal_print("Running ");
        } else if (curr->state == PROCESS_READY) {
            term_color = 0xCCCCCC; terminal_print("Ready   ");
        } else if (curr->state == PROCESS_SLEEPING) {
            term_color = 0xFFFF00; terminal_print("Sleeping");
        } else {
            term_color = 0xFF0000; terminal_print("Zombie  ");
        }
        term_color = 0xFFFFFFFF;

        // 4. Ring e RIP
        term_cursor_x = 250; 
        if (curr == current_process) {
            terminal_print("R0    [CURRENT]");
            term_cursor_x += 160;
        } else {
            interrupt_frame_t* frame = (interrupt_frame_t*)curr->stack_top;
            if ((uint64_t)frame > 0xFFFF800000000000 || (uint64_t)frame < 0x100000) {
                 terminal_print("R?    Invalid Stack");
                 term_cursor_x += 160;
            } else {
                uint64_t saved_rip = frame->rip; 
                uint64_t saved_cs  = frame->cs; 
                int ring = (int)(saved_cs & 0x03);
                
                terminal_print("R");
                draw_dec(term_cursor_x, term_cursor_y, ring, (ring == 3 ? 0x00FF00 : 0xFFFFFF));
                term_cursor_x += 20;
                draw_hex(term_cursor_x, term_cursor_y, saved_rip, 0xAAAAAA);
                term_cursor_x += 150;
            }
        }

        // 5. CR3
        draw_hex(term_cursor_x, term_cursor_y, curr->cr3, 0x00AAAA);
        terminal_print("\n");

        if (curr->next == curr) break; 
        curr = curr->next;
    }

    extern int refresh_screen;
    refresh_screen = 1; 
}

// ====================================================================
// CRIAÇÃO DE PROCESSOS (INSTRUMENTADA PARA DEBUG DE .ELF)
// ====================================================================
uint64_t create_process(void (*entry_point)(), int privilege_level, const char* name, uint64_t cr3) {
   // vga_print_string("\n[PROC DEBUG] Criando processo: ", 0, 39);
   // vga_print_string(name, 35, 39);

    void* raw = kmalloc(sizeof(process_t) + 15);
    if (!raw) {
        vga_print_string("\n[PROC ERROR] Falha ao alocar raw_mem_ptr!", 0, 39);
        return (uint64_t)-1;
    }

    // Alinhamento de memória
    process_t* new_proc = (process_t*)(((uintptr_t)raw + 15) & ~0xFULL);
    memset(new_proc, 0, sizeof(process_t));
    new_proc->raw_mem_ptr = (uint64_t)raw;
    
    // Inicialização do contexto FPU/SSE
    memset(new_proc->fpu_context, 0, 512);
    *(uint32_t*)&new_proc->fpu_context[24] = 0x1F80; 
    *(uint16_t*)&new_proc->fpu_context[0]  = 0x037F; 
    
    new_proc->pid = next_pid++;
    strncpy(new_proc->name, name, MAX_PROCESS_NAME - 1);
    new_proc->privilege = privilege_level;
    new_proc->state = PROCESS_READY;
    
    if (current_process != NULL) new_proc->parent_pid = current_process->pid;
    else new_proc->parent_pid = 0;
    
    new_proc->is_foreground = 0;
    new_proc->cr3 = (cr3 == 0) ? (read_cr3() & ~0xFFFULL) : (cr3 & ~0xFFFULL);

    void* kernel_stack = kmalloc(STACK_SIZE);
    if (!kernel_stack) { 
        vga_print_string("\n[PROC ERROR] Falha ao alocar kernel_stack!", 0, 39);
        kfree(raw); 
        return (uint64_t)-1; 
    }
    
    new_proc->stack_mem = kernel_stack;
    
    // Calcula o topo da pilha alinhado em 16 bytes
    uint64_t kstack_top = (uint64_t)((uint8_t*)kernel_stack + STACK_SIZE) & ~15ULL;
    
    // Mapeia a estrutura exatamente no topo da pilha (descendo o tamanho do frame)
    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t)); // Zera os registradores limpos
    
    // --- MONTAGEM DA PILHA (IRETQ FRAME CLARO E SÓLIDO) ---
    frame->rflags = 0x202;       // Interrupções ativas (IF=1)
    frame->rip    = (uint64_t)entry_point;

    if (privilege_level == RING3) {
        frame->cs  = GDT_USER_CODE;     // 0x1B
        frame->ss  = GDT_USER_DATA;     // 0x23
        frame->rsp = USER_STACK_TOP;    // Pilha virtual do usuário
    } else if (privilege_level == RING0) {
        frame->cs  = GDT_KERNEL_CODE;   // 0x08
        frame->ss  = GDT_KERNEL_DATA;   // 0x10
        frame->rsp = kstack_top;        // A própria pilha do kernel
    } else {
        vga_print_string("\n[PROC ERROR] Privilege level invalido!", 0, 39);
        kfree(kernel_stack);
        kfree(raw);
        return (uint64_t)-1;
    }
    
    // Define o stack_top para o endereço base da nossa estrutura preenchida
    new_proc->stack_top = (uint64_t)frame;
    
    // Adiciona o processo na lista encadeada do Kernel (Crítico)
    __asm__ volatile("cli");
    new_proc->next = head;
    head = new_proc;
    if (current_process == NULL) current_process = new_proc;
    __asm__ volatile("sti");

    vga_print_string("\n[PROC DEBUG] Processo criado com sucesso. PID: ", 0, 39);
    return new_proc->pid; 
}

// ====================================================================
// AGENDADOR DE PROCESSOS (SCHEDULE - INSTRUMENTADO COM PROTEÇÃO DE CR3)
// ====================================================================
uint64_t schedule(uint64_t current_rsp) {
    if (head == NULL) return current_rsp; 

    // 1. Verifica se há processos dormindo que devem acordar
    process_t* scan = head;
    while (scan != NULL) {
        if (scan->state == PROCESS_SLEEPING) { 
            if (system_ticks >= scan->wake_up_time) {
                scan->state = PROCESS_READY; // Acorda o processo
            }
        }
        scan = scan->next;
    }

    // 2. Salva o contexto do processo atual
    if (current_process != NULL) {
        current_process->stack_top = current_rsp;
        
        if (current_process->state > PROCESS_ZOMBIE) {
            // Salva FPU apenas se o ponteiro estiver alinhado corretamente (16-byte)
            if (((uintptr_t)current_process->fpu_context & 0xF) == 0) {
                __asm__ volatile("fxsave %0" : "=m" (current_process->fpu_context));
            }
        }

        if (current_process->state == PROCESS_RUNNING) {
            current_process->state = PROCESS_READY;
        }
    }

    // 3. Escolhe o próximo processo (Round-Robin blindado)
    process_t* next_proc = (current_process && current_process->next) ? current_process->next : head;
    process_t* start_proc = next_proc;

    // Procura por um processo READY dando exatamente uma volta completa na lista
    while (next_proc->state != PROCESS_READY) {
        next_proc = (next_proc->next) ? next_proc->next : head;
        if (next_proc == start_proc) {
            break; // Deu uma volta inteira e não achou ninguém pronto
        }
    }

    // Fallback de Segurança: Se não houver ninguem pronto (todos dormindo)
    if (next_proc->state != PROCESS_READY) {
        if (current_process && current_process->state == PROCESS_READY) {
            next_proc = current_process; // Mantém no atual
        } else {
            next_proc = head; // Retorna para o head para não crashear
        }
    }

    // 4. Prepara o contexto do processo selecionado
    current_process = next_proc;
    current_process->state = PROCESS_RUNNING;

    // PONTO CRÍTICO DE ERRO COMUM: Verificação e troca da Tabela de Páginas (CR3)
    uint64_t next_cr3 = current_process->cr3 & ~0xFFFULL;
    if (next_cr3 != 0) {
        if (read_cr3() != next_cr3) {
            write_cr3(next_cr3);
        }
    }

    // Restaura o contexto FPU
    if (((uintptr_t)current_process->fpu_context & 0xF) == 0) {
        __asm__ volatile("fxrstor %0" : : "m" (current_process->fpu_context));
    }

    // Atualiza a TSS para interrupções disparadas em Ring 3
    uint64_t kstack_for_cpu = (uint64_t)((uint8_t*)current_process->stack_mem + STACK_SIZE);
    tss_set_stack(kstack_for_cpu);

    return current_process->stack_top;
}

// ====================================================================
// CRIAÇÃO DE PROCESSOS (MÉTODO OTIMIZADO VIA STRUCT)
// ====================================================================
/*uint64_t create_process(void (*entry_point)(), int privilege_level, const char* name, uint64_t cr3) {
    void* raw = kmalloc(sizeof(process_t) + 15);
    if (!raw) return (uint64_t)-1;

    // Alinhamento de memória
    process_t* new_proc = (process_t*)(((uintptr_t)raw + 15) & ~0xFULL);
    memset(new_proc, 0, sizeof(process_t));
    new_proc->raw_mem_ptr = (uint64_t)raw;
    
    // Inicialização do contexto FPU/SSE
    memset(new_proc->fpu_context, 0, 512);
    *(uint32_t*)&new_proc->fpu_context[24] = 0x1F80; 
    *(uint16_t*)&new_proc->fpu_context[0]  = 0x037F; 
    
    new_proc->pid = next_pid++;
    strncpy(new_proc->name, name, MAX_PROCESS_NAME - 1);
    new_proc->privilege = privilege_level;
    new_proc->state = PROCESS_READY;
    
    if (current_process != NULL) new_proc->parent_pid = current_process->pid;
    else new_proc->parent_pid = 0;
    
    new_proc->is_foreground = 0;
    new_proc->cr3 = (cr3 == 0) ? (read_cr3() & ~0xFFFULL) : (cr3 & ~0xFFFULL);

    void* kernel_stack = kmalloc(STACK_SIZE);
    if (!kernel_stack) { 
        kfree(raw); 
        return (uint64_t)-1; 
    }
    
    new_proc->stack_mem = kernel_stack;
    
    // Calcula o topo da pilha alinhado em 16 bytes
    uint64_t kstack_top = (uint64_t)((uint8_t*)kernel_stack + STACK_SIZE) & ~15ULL;
    
    // Mapeia a estrutura exatamente no topo da pilha (descendo o tamanho do frame)
    interrupt_frame_t* frame = (interrupt_frame_t*)(kstack_top - sizeof(interrupt_frame_t));
    memset(frame, 0, sizeof(interrupt_frame_t)); // Zera os registradores limpos
    
    // --- MONTAGEM DA PILHA (IRETQ FRAME CLARO E SÓLIDO) ---
    frame->rflags = 0x202;         // Interrupções ativas (IF=1)
    frame->rip    = (uint64_t)entry_point;

    if (privilege_level == RING3) {
        frame->cs  = GDT_USER_CODE;     // 0x1B
        frame->ss  = GDT_USER_DATA;     // 0x23
        frame->rsp = USER_STACK_VADDR;  // Pilha virtual do usuário
    } else if (privilege_level == RING0) {
        frame->cs  = GDT_KERNEL_CODE;   // 0x08
        frame->ss  = GDT_KERNEL_DATA;   // 0x10
        frame->rsp = kstack_top;        // A própria pilha do kernel
    } else {
        kfree(kernel_stack);
        kfree(raw);
        return (uint64_t)-1;
    }
    
    // Define o stack_top para o endereço base da nossa estrutura preenchida
    new_proc->stack_top = (uint64_t)frame;
    
    // Adiciona o processo na lista encadeada do Kernel (Critico)
    __asm__ volatile("cli");
    new_proc->next = head;
    head = new_proc;
    if (current_process == NULL) current_process = new_proc;
    __asm__ volatile("sti");

    return new_proc->pid; 
}*/

// ====================================================================
// AGENDADOR DE PROCESSOS (SCHEDULER - CHAMADO NO TIMER IRQ0)
// ====================================================================
/*uint64_t schedule(uint64_t current_rsp) {
    if (head == NULL) return current_rsp; 

    // 1. Verifica se há processos dormindo que devem acordar
    process_t* scan = head;
    while (scan != NULL) {
        if (scan->state == PROCESS_SLEEPING) { 
            if (system_ticks >= scan->wake_up_time) {
                scan->state = PROCESS_READY; // Acorda o processo
            }
        }
        scan = scan->next;
    }

    // 2. Salva o contexto do processo atual
    if (current_process != NULL) {
        current_process->stack_top = current_rsp;
        
        if (current_process->state > PROCESS_ZOMBIE) {
            // Salva FPU apenas se o ponteiro estiver alinhado corretamente (16-byte)
            if (((uintptr_t)current_process->fpu_context & 0xF) == 0) {
                __asm__ volatile("fxsave %0" : "=m" (current_process->fpu_context));
            }
        }

        if (current_process->state == PROCESS_RUNNING) {
            current_process->state = PROCESS_READY;
        }
    }

    // 3. Escolhe o próximo processo (Round-Robin blindado)
    process_t* next_proc = (current_process && current_process->next) ? current_process->next : head;
    process_t* start_proc = next_proc;

    // Procura por um processo READY dando exatamente uma volta completa na lista
    while (next_proc->state != PROCESS_READY) {
        next_proc = (next_proc->next) ? next_proc->next : head;
        if (next_proc == start_proc) {
            break; // Deu uma volta inteira e não achou ninguém pronto
        }
    }

    // Fallback de Segurança: Se não houver ninguem pronto (todos dormindo)
    if (next_proc->state != PROCESS_READY) {
        if (current_process && current_process->state == PROCESS_READY) {
            next_proc = current_process; // Mantém no atual
        } else {
            next_proc = head; // Retorna para o head para não crashear (Idealmente head é o kernel idle)
        }
    }

    // 4. Prepara o contexto do processo selecionado
    current_process = next_proc;
    current_process->state = PROCESS_RUNNING;

    // Alterna Tabela de Páginas se necessário
    uint64_t next_cr3 = current_process->cr3 & ~0xFFFULL;
    if (read_cr3() != next_cr3) {
        write_cr3(next_cr3);
    }

    // Restaura o contexto FPU
    if (((uintptr_t)current_process->fpu_context & 0xF) == 0) {
        __asm__ volatile("fxrstor %0" : : "m" (current_process->fpu_context));
    }

    // Atualiza a TSS para interrupções disparadas em Ring 3
    uint64_t kstack_for_cpu = (uint64_t)((uint8_t*)current_process->stack_mem + STACK_SIZE);
    tss_set_stack(kstack_for_cpu);

    return current_process->stack_top;
}*/

// ====================================================================
// ENCERRAMENTO E LIMPEZA DE PROCESSOS
// ====================================================================
void terminate_current_process(void) {
    if (current_process == NULL || current_process->pid <= 1) { 
        proc_print("\n[Erro] Tentativa de fechar processo vital do Kernel!\n");
        return; 
    }
    
    __asm__ volatile("cli");
    
    current_process->state = PROCESS_ZOMBIE; 
    
    proc_print("\n[Kernel] Processo isolado para descarte: ");
    proc_print(current_process->name);
    proc_print("\n");
    
    __asm__ volatile("int $0x20");
    __asm__ volatile("sti");
    while(1) { __asm__ volatile("hlt"); }
}

void process_cleanup_zombies(void) {
    __asm__ volatile("cli"); 
    
    process_t* curr = head;
    process_t* prev = NULL;
    uint64_t kernel_cr3 = read_cr3() & ~0xFFFULL;

    while (curr != NULL) {
        if (curr->state == PROCESS_ZOMBIE && curr != current_process && curr->pid > 3) {
            process_t* to_delete = curr;

            if (prev == NULL) {
                head = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr = curr->next; 

            if (to_delete->cr3 != 0 && (to_delete->cr3 & ~0xFFFULL) != kernel_cr3) {
                elf_unload_failed_pml4((uint64_t*)to_delete->cr3);
            }
            
            if (to_delete->stack_mem != 0) {
                kfree(to_delete->stack_mem);
            }

            if (to_delete->raw_mem_ptr != 0) {
                kfree((void*)to_delete->raw_mem_ptr);
            } else {
                kfree(to_delete);
            }
            
            continue; 
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    __asm__ volatile("sti"); 
}

int kill_process(uint64_t pid) {
    if (pid <= 3) {
        proc_print("\n[Kernel Erro] Tentativa de matar processo vital ou protegido!\n");
        return -1; 
    }

    __asm__ volatile("cli");

    process_t* target = find_process_by_pid(pid);
    if (target == NULL) {
        __asm__ volatile("sti");
        return -1;
    }

    if (target->state == PROCESS_ZOMBIE) {
        __asm__ volatile("sti");
        return 0;
    }

    target->state = PROCESS_ZOMBIE;

    proc_print("\n[Kernel] Processo finalizado via SYS_KILL: ");
    proc_print(target->name);
    proc_print("\n");

    if (target->is_foreground) {
        process_t* pai = find_process_by_pid(target->parent_pid);
        if (pai) { 
            pai->is_foreground = 1; 
            foreground_process = pai; 
        }
    }

    if (target == get_current_process()) {
        __asm__ volatile("int $0x20");
        __asm__ volatile("sti");
        while(1) { __asm__ volatile("hlt"); }
    }

    __asm__ volatile("sti");
    return 0; 
}

// ====================================================================
// CONSULTAS E SYSCALLS DE SISTEMA
// ====================================================================
void force_reschedule(void) {
    __asm__ volatile("int $0x20"); 
}

void sys_sleep(uint64_t ms) {
    if (ms == 0 || current_process == NULL) return;

    __asm__ volatile("cli");

    uint64_t ticks_to_wait = ms / 10;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    current_process->wake_up_time = timer_get_ticks() + ticks_to_wait;
    current_process->state = PROCESS_SLEEPING;

    __asm__ volatile("int $0x20"); 
    __asm__ volatile("sti");
}

process_t* find_process_by_pid(uint64_t pid) {
    process_t* curr = head; 
    while (curr != NULL) {
        if (curr->pid == pid) return curr;
        curr = curr->next;
    }
    return NULL;
}

process_t* get_current_process(void) { 
    return current_process; 
}

uint64_t get_current_pid(void) {
    if (current_process != NULL) {
        return current_process->pid;
    }
    return 0; 
}

uint64_t sys_get_param(uint64_t id) {
    switch(id) {
        case 0: return (current_process) ? current_process->pid : 0;
        case 1: return system_ticks;
        case 2: return (current_process) ? current_process->privilege : 0;
        default: return 0;
    }
}

int sys_set_param(uint64_t id, uint64_t value) {
    switch(id) {
        case 100: 
            global_ui_color = value;
            return 0;
        default:
            return -1;
    }
}

int get_process_info_list(TProcessInfo* user_buffer, int max_items) {
    process_t* curr = head;
    int count = 0;

    while (curr != NULL && count < max_items) {
        if (curr->state == PROCESS_ZOMBIE) {
            curr = curr->next;
            continue; 
        }

        TProcessInfo k_temp;
        k_temp.pid = curr->pid;
        k_temp.state = curr->state;
        k_temp.cr3 = curr->cr3;
        
        for(int i = 0; i < 15; i++) {
            k_temp.name[i] = curr->name[i];
            if (curr->name[i] == '\0') break;
        }
        k_temp.name[15] = '\0';

        user_buffer[count] = k_temp; 

        curr = curr->next;
        count++;
    }
    return count;
}
