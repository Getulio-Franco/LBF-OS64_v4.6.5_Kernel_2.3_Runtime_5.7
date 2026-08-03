/**
 * ============================================================================
 * FAT32 COPY - NATIVE AHCI/SATA IMPLEMENTATION
 * ============================================================================
 * Descrição: Operações de clonagem de arquivos integradas ao barramento SATA.
 * Localização: fs/fat32_copy.c
 * ============================================================================
 */

#include "fs/fat32.h"
#include "fs/fat32_logic.h"
#include "fs/fat32_file.h"
#include "drivers/hw/ahci_hal.h"  // Nova abstração estável do hardware SATA
#include "drivers/video.h"        // Feedback visual na tela VESA
#include "util/string.h"
#include "mem/heap.h"
#include "drivers/pd/storage.h"

extern fat32_bpb_t disk_bpb;
extern uint32_t current_dir_cluster;
extern uint8_t fat32_current_dev_id;

/**
 * @brief Copia um arquivo usando as abstrações estáveis do sistema de arquivos.
 */
int fat32_copy_file(const char* source, const char* destination) {
    fat32_directory_entry_t entry;
    
    // Feedback inicial usando as funções do seu terminal gráfico VESA
    terminal_print("Copiando: ");
    terminal_print(source);
    terminal_print(" -> ");
    terminal_print(destination);
    terminal_print("\n");

    // 1. Busca a entrada do arquivo de origem para saber o tamanho correto
    if (fat32_find_entry(source, &entry) != 0) {
        terminal_print("Erro: Arquivo origem nao encontrado.\n");
        return FS_NOT_FOUND;
    }

    uint32_t file_size = entry.file_size;

    // Alocação dinâmica para acomodar o arquivo na memória temporariamente
    uint8_t* data_buffer = (uint8_t*)kmalloc(file_size);
    if (!data_buffer) {
        terminal_print("Erro: Memoria insuficiente para o buffer de copia (Heap Full).\n");
        return -1;
    }

    // 2. LEITURA REVISADA: Usa a função nativa estável do seu sistema de arquivos!
    // Ela já cuida de toda a paginação de clusters e setores perfeitamente.
    if (fat32_read_file(source, data_buffer, file_size) != FS_SUCCESS) {
        kfree(data_buffer);
        terminal_print("Erro critico de leitura no arquivo de origem.\n");
        return FS_ERROR_READ;
    }

    // 3. GRAVAÇÃO: Usa a função que corrigimos com suporte a subpastas (sistema/tamp.elf)
    int result = fat32_write_file(destination, data_buffer, file_size);

    if (result == FS_SUCCESS) {
        terminal_print("Copia concluida com sucesso.\n");
    } else {
        terminal_print("Erro ao gravar arquivo de destino.\n");
    }

    // Libera a memória do buffer temporário
    kfree(data_buffer);
    return result;
}
