/**
 * ============================================================================
 * SHELL COMMAND INTERPRETER - VESA/LFB EDITION (FIXED)
 * ============================================================================
 * Descrição: Processa e despacha os comandos do usuário.
 * Localização: drivers/shell/shell_commands.c
 * ============================================================================
 */

#include "shell_commands.h"
#include "shell_prompt.h"
#include "shell.h"         
#include "drivers/proc.h"  
#include "drivers/video.h" 
#include "drivers/syscall.h" 
#include "fs/fat32.h"        
#include "util/string.h"

static void cmd_print(const char* str) {
    if (!str) return;
    while (*str) {
        terminal_putc(*str++);
    }
}

void shell_execute_command(char* input_buffer) {
    // 1. Limpeza inicial de espaços em branco
    char* current_ptr = input_buffer;
    while (*current_ptr == ' ') {
        current_ptr++;
    }

    if (*current_ptr == '\0') {
        return;
    }

    // Remove o '\n' ou '\r' do final da string, se houver, para não quebrar o strcmp
    int len = strlen(current_ptr);
    while (len > 0 && (current_ptr[len - 1] == '\n' || current_ptr[len - 1] == '\r')) {
        current_ptr[len - 1] = '\0';
        len--;
    }

    // 2. Despacho de Comandos Nativos (Interface via Syscalls com 5 argumentos)
    
    // COMANDO: CLS / CLEAR
    if (strcmp(current_ptr, "cls") == 0 || strcmp(current_ptr, "clear") == 0) {
        syscall_handler(SYS_CLEAR, 0, 0, 0, 0, 0);
    }

    // COMANDO: HELP
    else if (strcmp(current_ptr, "help") == 0) {
        cmd_print("Comandos Disponiveis (Modo VESA):\n");
        cmd_print("LS, DIR, CD, CD.., MKDIR, MD, CAT, TYPE, CP, COPY, RM, PS, KILL, EXEC, LSPCI\n");
    }
   
    else if (strcmp(current_ptr, "ehci") == 0) {
         cmd_print("Shell: Ativando interface Bulk-Only Transport...\n");
         syscall_handler(SYS_EHCI, 0, 0, 0, 0, 0);
    }
    
    // COMANDO: LSPCI / PCI (Gatilho puro para o Kernel processar e exibir)
    else if (strcmp(current_ptr, "lspci") == 0 || strcmp(current_ptr, "pci") == 0) {
        syscall_handler(SYS_LSPCI_TERMINAL, 0, 0, 0, 0, 0);
    }
    
    // COMANDO: LS / DIR
    else if (strcmp(current_ptr, "ls") == 0 || strcmp(current_ptr, "dir") == 0) {
        syscall_handler(SYS_FATLS, 0, 0, 0, 0, 0);
    }

    // COMANDO: CAT / TYPE (Exibir conteúdo)
    else if (strncmp(current_ptr, "cat ", 4) == 0 || strncmp(current_ptr, "type ", 5) == 0) {
        char* target_file = (current_ptr[0] == 'c') ? current_ptr + 4 : current_ptr + 5;
        while (*target_file == ' ') target_file++;
        
        if (*target_file != '\0') {
            syscall_handler(SYS_FATCAT, (uint64_t)target_file, 0, 0, 0, 0);
        }
    }

    // COMANDO: MKDIR / MD
    else if (strncmp(current_ptr, "mkdir ", 6) == 0 || strncmp(current_ptr, "md ", 3) == 0) {
        char* new_dir = (current_ptr[1] == 'k') ? current_ptr + 6 : current_ptr + 3;
        while (*new_dir == ' ') new_dir++;
    
        // Executa a syscall do Kernel
        int status = (int)syscall_handler(SYS_MKDIR, (uint64_t)new_dir, 0, 0, 0, 0);
    
        // MELHORIA CRÍTICA AQUI: Testa se o status foi diferente de sucesso real
       /* if (status != FS_SUCCESS) {
            cmd_print("Erro: Nao foi possivel criar o diretorio (ou ja existe).\n");
        }*/
        
        if (status != 0) {
           cmd_print("Erro: Nao foi possivel criar o diretorio (ou ja existe).\n");
        }
    }

    // COMANDO: CD (Navegação)
    else if (strncmp(current_ptr, "cd ", 3) == 0 || strcmp(current_ptr, "cd..") == 0) {
        char* path_target;
        if (strcmp(current_ptr, "cd..") == 0) {
            path_target = "..";
        } else {
            path_target = current_ptr + 3;
            while (*path_target == ' ') path_target++;
        }
        
        int status = (int)syscall_handler(SYS_CHDIR, (uint64_t)path_target, 0, 0, 0, 0);
        if (status == 0) {
            update_shell_path(path_target);
        } else {
            cmd_print("Erro: Caminho nao encontrado no VFS/FAT32.\n");
        }
    }
   
    // COMANDO: RM / DEL (Remover arquivo/diretório no FAT32)
    else if (strncmp(current_ptr, "rm ", 3) == 0 || strncmp(current_ptr, "del ", 4) == 0) {
        // Se o segundo caractere for 'm' (de rm), pula 3. Se não, é 'del ', então pula 4.
        char* target = (current_ptr[1] == 'm') ? current_ptr + 3 : current_ptr + 4;
    
        // Pula espaços extras se o usuário digitou "rm    arquivo.txt"
        while (*target == ' ') target++;
    
        // Executa a syscall do Kernel
        int status = (int)syscall_handler(SYS_FATRM, (uint64_t)target, 0, 0, 0, 0);
    
        // Se o seu FS_SUCCESS for 0, mantemos o teste atual:
        if (status != 0) {
            cmd_print("Erro: Nao foi possivel remover o item.\n");
        }
    }

    // COMANDO: CP / COPY (Copiar Arquivos) - CORRIGIDO E HIGIENIZADO
    else if (strncmp(current_ptr, "cp ", 3) == 0 || strncmp(current_ptr, "copy ", 5) == 0) {
        char* args = (current_ptr[1] == 'p') ? current_ptr + 3 : current_ptr + 5;
        while (*args == ' ') args++;
        
        // Separa os dois argumentos básicos (origem e destino) por espaço
        char* src = args;
        char* dest = strchr(args, ' ');
        
        if (dest) {
            *dest = '\0'; // Corta a string para isolar o 'src'
            dest++;
            while (*dest == ' ') dest++; // Ignora espaços múltiplos entre os argumentos
            
            // --- HIGIENIZAÇÃO DO SEGUNDO ARGUMENTO (dest) ---
            // Remove quebras de linha (\n, \r) ou espaços que sobram no final do comando
            char* end = dest;
            while (*end != '\0' && *end != '\n' && *end != '\r') {
                end++;
            }
            *end = '\0'; // Finaliza a string de destino de forma limpa!

            // Garante que o destino não ficou vazio após a limpeza
            if (strlen(dest) == 0) {
                cmd_print("Uso: copy <origem> <destino>\n");
            } else {
                // Dispara a syscall com as strings 100% limpas
                int status = (int)syscall_handler(SYS_FATCP, (uint64_t)src, (uint64_t)dest, 0, 0, 0);
                if (status != 0) {
                    cmd_print("Erro ao copiar arquivo.\n");
                }
            }
        } else {
            cmd_print("Uso: copy <origem> <destino>\n");
        }
    }
    
    // COMANDO: APPEND (Anexar texto a um arquivo existente)
    else if (strncmp(current_ptr, "append ", 7) == 0) {
        char* args = current_ptr + 7;
        while (*args == ' ') args++;

        // Separa o nome do arquivo do texto (procurando o primeiro espaço)
        char* texto = args;
        while (*texto != ' ' && *texto != '\0') texto++;
    
        if (*texto == ' ') {
            *texto = '\0'; // Corta a string para isolar o nome do arquivo
            texto++;
            while (*texto == ' ') texto++; // Pula espaços até o início do texto
        
            // Calcula o tamanho do texto (incluindo uma quebra de linha se quiser)
            uint32_t len = 0;
            while (texto[len] != '\0') len++;

            // Dispara a sua nova syscall 14!
            int status = (int)syscall_handler(14, (uint64_t)args, (uint64_t)texto, (uint64_t)len, 0, 0);
        
            if (status != 0) {
                cmd_print("Erro: Nao foi possivel anexar os dados.\n");
            }
        } else {
            cmd_print("Uso: append <arquivo> <texto>\n");
        }
    }

    // COMANDO: EXEC (Forçado manual)
    else if (strncmp(current_ptr, "exec ", 5) == 0) {
        char* elf_path = current_ptr + 5;
        while (*elf_path == ' ') elf_path++;
        
        cmd_print("LFB: Carregando executavel...\n");
        if (syscall_handler(SYS_EXEC, (uint64_t)elf_path, 0, 0, 0, 0) == (uint64_t)-1) {
            cmd_print("Erro: Falha ao carregar binario ELF.\n");
        }
    }
    
    // =========================================================================
    // COMANDO: Chaveamento de Unidades (C:, D: ou E:) // syscall SYS_SET_DRIVE
    // =========================================================================
    else if (strcmp(current_ptr, "c:") == 0 || strcmp(current_ptr, "C:") == 0) {
        // Dispara a Syscall pedindo o Disco 0 (SATA)
        int status = (int)syscall_handler(SYS_SET_DRIVE, 0, 0, 0, 0, 0);
        if (status == 1) {
            cmd_print("Unidade alterada para C:\\\n");
        } else {
            cmd_print("Erro: Unidade C: nao disponível.\n");
        }
    }
    else if (strcmp(current_ptr, "d:") == 0 || strcmp(current_ptr, "D:") == 0) {
        // Dispara a Syscall pedindo o Disco 1 (USB Porto 1)
        int status = (int)syscall_handler(SYS_SET_DRIVE, 1, 0, 0, 0, 0);
        if (status == 1) {
            cmd_print("Unidade alterada para D:\\\n");
        } else {
            cmd_print("Erro: Unidade D: nao disponível (Verifique o Pendrive).\n");
        }
    }
    else if (strcmp(current_ptr, "e:") == 0 || strcmp(current_ptr, "E:") == 0) {
        // Dispara a Syscall pedindo o Disco 2 (USB Porto 2 ou partição secundária)
        int status = (int)syscall_handler(SYS_SET_DRIVE, 2, 0, 0, 0, 0);
        if (status == 1) {
            cmd_print("Unidade alterada para E:\\\n");
        } else {
            cmd_print("Erro: Unidade E: nao disponível (Verifique a conexao USB).\n");
        }
    }

    // COMANDO: PS
    else if (strcmp(current_ptr, "ps") == 0) {
        syscall_handler(SYS_PS, 0, 0, 0, 0, 0);
    }

    // COMANDO: KILL
    else if (strncmp(current_ptr, "kill ", 5) == 0) {
        uint64_t pid = atoi(current_ptr + 5);
        if (syscall_handler(SYS_KILL, pid, 0, 0, 0, 0) != 0) {
            cmd_print("Erro: Nao foi possivel finalizar o PID solicitado.\n");
        }
    }

    // 3. SE NÃO FOR COMANDO NATIVO, TENTA EXECUTAR COMO .ELF DIRETO (FALBACK AUTOMÁTICO)
    else {
        // Se a string termina com ".elf" ou se quisermos tentar rodar qualquer comando desconhecido
        // como um arquivo executável contido no diretório atual do FAT32:
        if (strstr(current_ptr, ".elf") != NULL) {
            cmd_print("FAT32: Executando aplicacao ");
            cmd_print(current_ptr);
            cmd_print("...\n");
            
            if (syscall_handler(SYS_EXEC, (uint64_t)current_ptr, 0, 0, 0, 0) == (uint64_t)-1) {
                cmd_print("Erro: Aplicacao nao respondeu ou arquivo corrompido.\n");
            }
        } else {
            cmd_print("Comando desconhecido: ");
            cmd_print(current_ptr);
            cmd_print("\nUtilize 'help' para a lista de comandos.\n");
        }
    }

    // Pula uma linha para o próximo prompt
    cmd_print("\n");
}
