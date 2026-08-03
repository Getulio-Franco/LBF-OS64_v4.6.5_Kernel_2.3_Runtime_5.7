#include "apps.h"
#include "drivers/video.h"
#include "include/elf.h"
#include "drivers/proc.h"
#include "drivers/keyboard.h"
#include <stdint.h>

extern volatile int vga_ring0_enabled;
// Variáveis Globais do Kernel para a ponte Syscall <-> task_d
extern volatile int g_exec_pending_flag;
extern char g_pending_elf_path[128];
extern uint64_t g_last_exec_pid;

void task_a() {
//nunca usar
}

void task_b() {
    while(1) {
        process_cleanup_zombies(); // Ceifador
        for(volatile int i = 0; i < 500000; i++); 
        __asm__ volatile("hlt"); 
    }
}

void task_c() {
    // Aguarda o sistema estabilizar/drivers carregarem
    for(volatile int i = 0; i < 5000000; i++);

    // 1. Cria o processo do Explorer
    uint64_t pid_explorer = create_elf_process("EXPLORER.ELF");

    if (pid_explorer != (uint64_t)-1) {
        __asm__ volatile("cli");
        
        process_t* proc_explorer = find_process_by_pid(pid_explorer);
        
        if (proc_explorer) {
            // Se houver um processo anterior em foreground (Ex: o Shell do terminal), 
            // removemos o foco dele primeiro para não haver duplicidade
            if (foreground_process) {
                foreground_process->is_foreground = 0;
            }

            // 2. Entrega oficialmente o controle do hardware ao Explorer
            proc_explorer->is_foreground = 1;
            foreground_process = proc_explorer;
            
            // O Explorer está pronto e assumiu o foreground. 
            // Desligamos o motor gráfico do Ring 0 imediatamente!
            vga_ring0_enabled = 0; // deliga o db_swap_buffers(); do kernel.c

            // 3. Limpa o lixo residual do buffer do teclado antes da GUI ler
            while(keyboard_pop_char() != 0);
        }
        
        __asm__ volatile("sti");
    }

    // Loop de ociosidade da task de boot
    while(1) {
        __asm__ volatile("hlt");
    }
}

void task_d() {
    while(1) {
        if (g_exec_pending_flag == 1) {
            
            // Ponto 1: Mostra que pegou o nome e vai invocar o ELF loader
            vga_print_string("\n[TASK_D v9_v2] indo para o create_elf_processo: ", 0, 38);
            vga_print_string(g_pending_elf_path, 30, 38);

            uint64_t pid_filho = create_elf_process(g_pending_elf_path);

            if (pid_filho != 0 && pid_filho != (uint64_t)-1) {
                __asm__ volatile("cli");
                
                process_t* proc_filho = find_process_by_pid(pid_filho);
                if (proc_filho) {
                    proc_filho->is_foreground = 1;
                    if (foreground_process) {
                        foreground_process->is_foreground = 0;
                    }
                    foreground_process = proc_filho;
                    
                    proc_filho->state = PROCESS_READY;
                    
                    while(keyboard_pop_char() != 0);
                }
                
                g_last_exec_pid = pid_filho;
                __asm__ volatile("sti");
                
                // Ponto 2A: Sucesso na criação do processo
                vga_print_string("\n[TASK_D v9_v2] Processo executando, indo ao RING3 ", 0, 39);
            } else {
                g_last_exec_pid = (uint64_t)-1;
                
                // Ponto 2B: Falha mapeada no carregamento do ELF
                vga_print_string("\n[TASK_D v9_v2] ERRO: Falha em create_elf_process!", 0, 39);
            }

            g_exec_pending_flag = 0;
        }

        __asm__ volatile("hlt");
    }
}

/*void task_d() {
   // vga_print_string("[TASK_D] Worker de inicializacao de processos ativo...\n", 0, 37);
    while(1) {
        // Se houver pedido de execução pendente vindo do Ring 3
        if (g_exec_pending_flag == 1) {
            
            vga_print_string("[TASK_D] Carregando arquivo: ", 0, 37);
          //  vga_print_string(g_pending_elf_path, 0, 37);
          //  vga_print_string("\n", 0, 37);

            // 1. Cria o processo de forma isolada e segura no Ring 0 (igual à task_c)
            uint64_t pid_filho = create_elf_process(g_pending_elf_path);

            if (pid_filho != 0 && pid_filho != (uint64_t)-1) {
                __asm__ volatile("cli");
                
                process_t* proc_filho = find_process_by_pid(pid_filho);
                if (proc_filho) {
                    // Entrega o foco e coloca em PRONTO para o Escalonador rodar
                    proc_filho->is_foreground = 1;
                    if (foreground_process) {
                        foreground_process->is_foreground = 0;
                    }
                    foreground_process = proc_filho;
                    
                    proc_filho->state = PROCESS_READY;
                    
                    // Limpa resíduos de digitação do teclado
                    while(keyboard_pop_char() != 0);
                }
                
                g_last_exec_pid = pid_filho;
                __asm__ volatile("sti");
                
                vga_print_string("[TASK_D] Processo aberto com sucesso!\n", 0, 37);
            } else {
                g_last_exec_pid = (uint64_t)-1;
                vga_print_string("[TASK_D] Falha ao carregar executavel!\n", 0, 37);
            }

            // Reseta a flag para indicar que o pedido foi atendido
            g_exec_pending_flag = 0;
        }

        // Coloca a CPU em repouso até o próximo pulso do Timer
        __asm__ volatile("hlt");
    }
}*/

/*void task_d() {
    while(1) {
        // Se houver pedido de execução pendente vindo do Ring 3
        if (g_exec_pending_flag == 1) {
            
            // Garante que a string no buffer do kernel é segura antes de usar
            if (g_pending_elf_path[0] != '\0') {
               // vga_print_string("[TASK_D] Carregando arquivo: ", 0, 37);
               // vga_print_string(g_pending_elf_path, 0, 37);
               // vga_print_string("\n", 0, 37);

                // 1. Cria o processo de forma isolada e segura no Ring 0
                // (Como retiramos o cli/sti do elf.c, a leitura do disco ocorre livremente aqui)
                uint64_t pid_filho = create_elf_process(g_pending_elf_path);

                if (pid_filho != 0 && pid_filho != (uint64_t)-1) {
                    
                    // Início da Seção Crítica do Escalonador
                    __asm__ volatile("cli");
                    
                    process_t* proc_filho = find_process_by_pid(pid_filho);
                    if (proc_filho) {
                        // Entrega o foco e coloca em PRONTO para o Escalonador rodar
                        proc_filho->is_foreground = 1;
                        if (foreground_process) {
                            foreground_process->is_foreground = 0;
                        }
                        foreground_process = proc_filho;
                        
                        proc_filho->state = PROCESS_READY;
                        
                        // Limpa resíduos de digitação do teclado de forma segura
                        while(keyboard_pop_char() != 0);
                    }
                    
                    g_last_exec_pid = pid_filho;
                    
                    // Fim da Seção Crítica
                    __asm__ volatile("sti");
                    
                    vga_print_string("[TASK_D] Processo aberto com sucesso!\n", 0, 37);
                } else {
                    g_last_exec_pid = (uint64_t)-1;
                    vga_print_string("[TASK_D] Falha ao carregar executavel!\n", 0, 37);
                }
            } else {
                vga_print_string("[TASK_D] Erro: Caminho do ELF vazio ou invalido!\n", 0, 37);
                g_last_exec_pid = (uint64_t)-1;
            }

            // Reseta a flag e limpa o buffer de caminho para evitar execuções fantasmas
            g_exec_pending_flag = 0;
            g_pending_elf_path[0] = '\0';
        }

        // Coloca a CPU em repouso até o próximo pulso do Timer (Economiza ciclos e estabiliza)
        __asm__ volatile("hlt");
    }
}*/
