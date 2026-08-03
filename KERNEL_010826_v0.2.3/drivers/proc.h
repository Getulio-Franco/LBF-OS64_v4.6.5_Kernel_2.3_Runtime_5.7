/**
 * ============================================================================
 * PROC.H - GERENCIAMENTO DE PROCESSOS (PCB) - V4.1 (STRUCT FRAME EDITION)
 * ============================================================================
 */

#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <stdbool.h>

// ====================================================================
// ESTRUTURA DO FRAME DE INTERRUPÇÃO (Mapeia a pilha x86_64 do Kernel)
// ====================================================================
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax; // 15 Registradores de Propósito Geral (GPR)
    uint64_t int_no, err_code;                  // Código de Erro / Interrupção
    uint64_t rip, cs, rflags, rsp, ss;          // IRETQ Frame (Pilha de Hardware x86_64)
} interrupt_frame_t;

// A mesma estrutura exportada para o Ring 3 (Gerenciador de Tarefas / Task Manager)
typedef struct {
    uint64_t pid;
    char name[16];
    uint32_t state;
    uint64_t cr3;
} TProcessInfo;

// --- Configurações de Memória e Limites ---
#define STACK_SIZE        8192
#define MAX_PROCESS_NAME  32
#define MAX_PROCESSES     64    

// ====================================================================
// LIMITES DA PILHA DO USUÁRIO (DYNAMIC STACK GROWTH)
// ====================================================================
// Topo seguro e canônico no x86_64 (Muito abaixo do limite proibido)
#define USER_STACK_TOP          0x00007FFFF0000000ULL 
// Tamanho Inicial: 6 páginas (24 KB)
#define USER_STACK_INIT_PAGES   6
#define USER_STACK_INIT_BYTES   (USER_STACK_INIT_PAGES * 4096)

// Limite Máximo Dinâmico (Ex: 8 MB = 2048 páginas de 4KB)
#define USER_STACK_MAX_BYTES    (8 * 1024 * 1024) 
#define USER_STACK_BOTTOM_LIMIT (USER_STACK_TOP - USER_STACK_MAX_BYTES)

// --- Estados do Processo ---
#define PROCESS_ZOMBIE    0    // Morto / Descartado (Aguardando limpeza)
#define PROCESS_READY     1    // Pronto para rodar pelo Escalonador
#define PROCESS_RUNNING   2    // Atualmente em execução na CPU
#define PROCESS_SLEEPING  3    // Bloqueado por Tempo / Syscall (Aguardando wake_up_time)
#define PROCESS_WAITING   4    // Bloqueado por I/O

// Mantendo compatibilidade com código antigo
#define PROCESS_DEAD      PROCESS_ZOMBIE
#define PROCESS_ALIVE     PROCESS_READY

// --- Níveis de Privilégio (Ring) ---
#define RING0             0
#define RING3             3

// Definições de Sinais (Bitmask)
#define SIG_EVENT_MOUSE     (1 << 0)  // 0x01
#define SIG_EVENT_KEYBOARD  (1 << 1)  // 0x02
#define SIG_EVENT_TERMINATE (1 << 2)  // 0x04
#define SIG_EVENT_IPC       (1 << 3)  // 0x08

/**
 * @struct process_t
 * @brief Bloco de Controle de Processo (PCB)
 */
typedef struct process {
    uint64_t pid;                // Identificador único
    char     name[MAX_PROCESS_NAME];
    
    // Unificação de CR3 e PML4 Physical
    union {
        uint64_t cr3;
        uint64_t pml4_physical;
    };
    uint64_t heap_end;          // Uso do malloc/sys_sbrk
    uint64_t stack_top;         // RSP salvo (Ponteiro para o interrupt_frame_t)
    void* stack_mem;         // Endereço base da pilha do kernel (para kfree)
    int      privilege;         // RING0 ou RING3
    int      state;             // ZOMBIE, READY, RUNNING, SLEEPING
    uint64_t wake_up_time;      // Momento de acordar em ticks do sistema
    int      is_foreground;     // 1: Foco do teclado/mouse, 0: Background
    uint64_t parent_pid;
    uint64_t pending_signals;   // Flags de eventos
    void* message_queue;
    struct process* next;       // Próximo processo na lista encadeada
    uint64_t raw_mem_ptr;       // Ponteiro para kfree da estrutura base
    uint64_t exec_result;
    uint8_t  fpu_context[512] __attribute__((aligned(16))); // Contexto SSE/FPU
} process_t;

/* --- Variáveis Globais Exportadas --- */
extern process_t* head;
extern process_t* current_process;
extern process_t* foreground_process;

// --- Variáveis de Comunicação Syscall <-> task_d ---
extern volatile int g_exec_pending_flag;
extern char g_pending_elf_path[128];
extern uint64_t g_last_exec_pid;

/* --- Interface do Escalonador e Processos --- */

// Inicialização e Ciclo de Vida
void scheduler_init(void);
uint64_t create_process(void (*entry_point)(), int privilege_level, const char* name, uint64_t cr3);
uint64_t create_elf_process(const char* path);
void terminate_current_process(void);

// Gerenciamento, Troca de Contexto e Busca
uint64_t schedule(uint64_t current_rsp);
void force_reschedule(void);
process_t* get_current_process(void);          
process_t* find_process_by_pid(uint64_t pid);  
int kill_process(uint64_t pid);
void list_processes(void); 

// Syscalls e Utilitários de Processo
uint64_t sys_get_param(uint64_t id);
int sys_set_param(uint64_t id, uint64_t value);
void sys_sleep(uint64_t ms);
uint64_t get_current_pid(void);
int get_process_info_list(TProcessInfo* user_buffer, int max_items);
void process_cleanup_zombies(void);

#endif // PROC_H
