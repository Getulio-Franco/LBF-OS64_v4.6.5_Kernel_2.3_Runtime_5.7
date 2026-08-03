#include "elf.h"
#include "fs/fat32_file.h"
#include "mem/vmm.h"
#include "mem/pmm.h"
#include "mem/heap.h"
#include "util/string.h"
#include "drivers/video.h"
#include "drivers/proc.h"
#include <stdbool.h> 

//#define USER_STACK_VADDR 0x2000000
//#define USER_STACK_PAGES 6 // 6 x 4KB = 24KB 

void elf_unload_failed_pml4(uint64_t* pml4_phys) {
    if (!pml4_phys || (uint64_t)pml4_phys == (read_cr3() & ~0xFFFULL)) return; 
    
    uint64_t* pml4 = (uint64_t*)pml4_phys;
    
    // 1. Limpa a área recém-isolada que criamos na entrada 0
    if (pml4[0] & PAGE_PRESENT) {
        uint64_t* pdpt = (uint64_t*)(pml4[0] & ~0xFFFULL);
        if (pdpt[0] & PAGE_PRESENT) {
            uint64_t* pd = (uint64_t*)(pdpt[0] & ~0xFFFULL);
            
            // Libera as tabelas de páginas do usuário (índices 2 a 15)
            for (int j = 2; j <= 15; j++) {
                if (pd[j] & PAGE_PRESENT) {
                    uint64_t* pt = (uint64_t*)(pd[j] & ~0xFFFULL);
                    
                    // =========================================================================
                    // CORREÇÃO CRÍTICA: LIBERA AS PÁGINAS FÍSICAS DE RAM (FIM DO VAZAMENTO!)
                    // =========================================================================
                    for (int k = 0; k < 512; k++) {
                        if (pt[k] & PAGE_PRESENT) {
                            uint64_t phys_page = pt[k] & ~0xFFFULL;
                            // Só libera se não for endereço nulo e estiver na área de usuário
                            if (phys_page != 0) {
                                pmm_free_block((void*)phys_page); 
                            }
                        }
                    }
                    
                    pmm_free_block(pt); // Libera a tabela de páginas em si
                }
            }
            pmm_free_block(pd);
        }
        pmm_free_block(pdpt);
    }

    // 2. Limpa as outras áreas de usuário
    for (int i = 1; i < 256; i++) {
        if (pml4[i] & PAGE_PRESENT) {
            uint64_t* pdpt = (uint64_t*)(pml4[i] & ~0xFFFULL);
            pmm_free_block(pdpt);
        }
    }
    pmm_free_block(pml4_phys);
}

uint64_t elf_load_file(const char* path, uint64_t* pml4_dest) {
    Elf64_Ehdr elf_header;
    
    // 1. Lê o cabeçalho principal
    if (fat32_read_file_at_offset(path, (uint8_t*)&elf_header, sizeof(Elf64_Ehdr), 0) != FS_SUCCESS) {
        vga_print_string("\n[ELF v9_v3] ERRO: Falha ao ler cabecalho no FAT32.", 0, 39);
        return 0;
    }

    if (!elf_is_valid(&elf_header)) {
        vga_print_string("\n[ELF v9_v3] ERRO: Arquivo ELF invalido!", 0, 39);
        return 0;
    }

    // 2. Leitura dos Program Headers
    uint32_t phdr_size = elf_header.e_phnum * elf_header.e_phentsize;
    Elf64_Phdr* phdr = (Elf64_Phdr*)kmalloc(phdr_size); 
    if (!phdr) {
        vga_print_string("\n[ELF v9_v3] ERRO: Falha de memoria ao alocar Phdr.", 0, 39);
        return 0;
    }

    if (fat32_read_file_at_offset(path, (uint8_t*)phdr, phdr_size, elf_header.e_phoff) != FS_SUCCESS) {
        vga_print_string("\n[ELF v9_v3] ERRO: Falha ao ler Program Headers.", 0, 39);
        kfree(phdr);
        return 0;
    }

    // 3. Mapeamento dos Segmentos
    for (int i = 0; i < elf_header.e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t page_start = phdr[i].p_vaddr & ~0xFFFULL;
            uint64_t page_end = (phdr[i].p_vaddr + phdr[i].p_memsz + 0xFFF) & ~0xFFFULL;
            for (uint64_t curr = page_start; curr < page_end; curr += 4096) {
                if (curr < 0x100000) continue; 
                void* phys = pmm_alloc_block();
                if (!phys) {
                    // SE FALHAR AQUI, A LIMPEZA OCORRE NO create_elf_process VIA elf_unload_failed_pml4
                    kfree(phdr);
                    return 0;
                }
                memset(phys, 0, 4096);
                vmm_map_page_to_pml4(pml4_dest, curr, (uint64_t)phys, PAGE_PRESENT | PAGE_USER | PAGE_WRITE);
            }
        }
    }

    // 4. Mapeamento da Pilha do Usuário
  /*  for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t vaddr = (USER_STACK_VADDR - (USER_STACK_PAGES * 4096)) + (i * 4096);
        void* phys_stack = pmm_alloc_block();
        if (!phys_stack) {
            kfree(phdr);
            return 0;
        }
        vmm_map_page_to_pml4(pml4_dest, vaddr, (uint64_t)phys_stack, PAGE_PRESENT | PAGE_USER | PAGE_WRITE);
    }*/
    
    // 4. Mapeamento Inicial da Pilha do Usuário (24KB Dinâmica)
    // Mapeia apenas as páginas iniciais; o restante até 8MB crescerá via Page Fault.
    //vga_print_string("[ELF] Mapeando Stack Inicial do Usuario (24KB)...\n", 0, 37);
    for (uint64_t i = 0; i < USER_STACK_INIT_PAGES; i++) {
        uint64_t vaddr = (USER_STACK_TOP - USER_STACK_INIT_BYTES) + (i * 4096);
        void* phys_stack = pmm_alloc_block();
        if (phys_stack) {
            memset(phys_stack, 0, 4096);
            vmm_map_page_to_pml4(pml4_dest, vaddr, (uint64_t)phys_stack, PAGE_PRESENT | PAGE_USER | PAGE_WRITE);
        }
    }

    // --- BLOCO DE ESCRITA CRÍTICA ---
    __asm__ volatile("cli");

    for (int i = 0; i < elf_header.e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            
            if (phdr[i].p_filesz > 0) {
                uint8_t* bounce_buffer = (uint8_t*)kmalloc(4096); 
                if (bounce_buffer) {
                    uint32_t remaining = phdr[i].p_filesz;
                    uint32_t offset = phdr[i].p_offset;
                    uint64_t dest_vaddr = phdr[i].p_vaddr;

                    while (remaining > 0) {
                        uint32_t to_read = (remaining > 4096) ? 4096 : remaining;
                        
                        // fat32_read_file_at_offset agora possui Spinlock ativado lá no VFS. Seguro.
                        fat32_read_file_at_offset(path, bounce_buffer, to_read, offset);

                        uint64_t old_cr3 = read_cr3();
                        write_cr3((uint64_t)pml4_dest);
                        
                        memcpy((void*)dest_vaddr, bounce_buffer, to_read);
                        
                        write_cr3(old_cr3);

                        remaining -= to_read;
                        offset += to_read;
                        dest_vaddr += to_read;
                    }
                    kfree(bounce_buffer);
                } else {
                    __asm__ volatile("sti");
                    kfree(phdr);
                    return 0;
                }
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                elf_clear_bss(pml4_dest, phdr[i].p_vaddr, phdr[i].p_filesz, phdr[i].p_memsz);
            }
        }
    }
    
    __asm__ volatile("sti");
    
    kfree(phdr); 
    return elf_header.e_entry;
}

uint64_t create_elf_process(const char* filename) {
    if (!filename || filename[0] == '\0') {
        return 0; // ETAPA 1: Sanitização de entrada inicial
    }

    // 🛡️ CORREÇÃO DE SEGURANÇA: Cria um cache local fixo para o nome do arquivo.
    char safe_filename[64];
    for (int k = 0; k < 63; k++) {
        safe_filename[k] = filename[k];
        if (filename[k] == '\0') break;
    }
    safe_filename[63] = '\0';

   // vga_print_string("\n[ELF v9_v3] Criando processo para: ", 0, 39);
   // vga_print_string(safe_filename, 0, 39); 

    // ETAPA 2: Alocação e Inicialização Isolada das Páginas
    uint64_t* new_pml4 = (uint64_t*)pmm_alloc_block();
    if (!new_pml4) {
        vga_print_string("\n[ELF v9_v3] ERRO: Falha PML4!", 0, 39);
        return 0; // Aborta direto, pois a árvore não foi montada.
    }
    memset(new_pml4, 0, 4096);

    uint64_t* kernel_pml4 = (uint64_t*)(read_cr3() & ~0xFFFULL);

    // Mapear o Kernel (High Half)
    for(int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }

    // Isolamento do Identity Mapping (0 a 512GB)
    uint64_t* kernel_pdpt = (uint64_t*)(kernel_pml4[0] & ~0xFFFULL);
    
    uint64_t* new_pdpt = (uint64_t*)pmm_alloc_block();
    if (!new_pdpt) {
        pmm_free_block(new_pml4); // Limpa o que já alocou
        return 0;
    }
    
    memset(new_pdpt, 0, 4096);
    new_pml4[0] = ((uint64_t)new_pdpt) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE; 

    for (int i = 0; i < 512; i++) {
        if (kernel_pdpt[i] & PAGE_PRESENT) {
            if (i == 0) {
                uint64_t* new_pd = (uint64_t*)pmm_alloc_block();
                if (!new_pd) {
                    pmm_free_block(new_pdpt);
                    pmm_free_block(new_pml4);
                    return 0;
                }
                memset(new_pd, 0, 4096);
                new_pdpt[0] = ((uint64_t)new_pd) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;

                uint64_t* kernel_pd = (uint64_t*)(kernel_pdpt[0] & ~0xFFFULL);
                
                for (int j = 0; j < 512; j++) {
                    if (j >= 2 && j <= 15) {
                        new_pd[j] = 0;
                    } else if (kernel_pd[j] & PAGE_PRESENT) {
                        new_pd[j] = kernel_pd[j] | PAGE_USER; 
                    }
                }
            } else {
                new_pdpt[i] = kernel_pdpt[i] | PAGE_USER;
            }
        }
    }

    // ETAPA 3: Carregamento do ELF (Protegido por Lock de Disco no VFS)
    uint64_t entry = elf_load_file(safe_filename, new_pml4);
    
    if (entry > 0) {
        vga_print_string("\n[ELF v9_v3] ELF carregado com sucesso. Chamando create_process...", 0, 39);
        uint64_t pid = create_process((void(*)())entry, 3, safe_filename, (uint64_t)new_pml4);
        
        if (pid == 0) {
            // Falha ao criar a task internamente. Deleta a memória alocada do ELF
            elf_unload_failed_pml4(new_pml4);
            return 0;
        }
        
        return pid;
    } else {
        vga_print_string("\n[ELF v9_v3] ERRO: Falha em elf_load_file!", 0, 39);
        // ROLLBACK: Limpa toda a estrutura de memória recém construída
        elf_unload_failed_pml4(new_pml4);
        return 0; 
    }
}

bool elf_is_valid(Elf64_Ehdr* header) {
    if (header->e_ident[EI_MAG0] != 0x7F ||
        header->e_ident[EI_MAG1] != 'E'  ||
        header->e_ident[EI_MAG2] != 'L'  ||
        header->e_ident[EI_MAG3] != 'F') {
        return false;
    }

    if (header->e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }

    if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
        return false;
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        return false;
    }

    return true;
}

void elf_clear_bss(uint64_t* pml4_dest, uint64_t vaddr, uint64_t file_size, uint64_t mem_size) {
    if (mem_size <= file_size) return;

    uint64_t bss_start = vaddr + file_size;
    uint64_t bss_size = mem_size - file_size;

    uint64_t old_cr3 = read_cr3();
    write_cr3((uint64_t)pml4_dest);

    memset((void*)bss_start, 0, bss_size);

    write_cr3(old_cr3);
}
