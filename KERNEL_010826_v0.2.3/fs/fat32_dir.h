/**
 * ============================================================================
 * FAT32 DIRECTORY MANAGEMENT - HEADER V3.3 (NATIVE AHCI/SATA SYNC)
 * ============================================================================
 * Descrição: Protótipos das funções públicas de gerenciamento de diretórios.
 * ============================================================================
 */

#ifndef FS_DIR_H
#define FS_DIR_H

#include <stdint.h>

/**
 * @brief Lista o conteúdo do diretório atual na tela em modo gráfico VESA.
 * @param cluster O cluster inicial do diretório que se deseja listar (ex: current_dir_cluster).
 */
void fat32_list_directory(uint32_t cluster);

/**
 * @brief Cria um novo subdiretório dentro do diretório atual.
 * @param dir_name Nome do novo diretório (padrão 8.3).
 * @return 0 em caso de sucesso, ou -1 se houver falha (ex: nome já existe ou disco cheio).
 */
int fat32_create_directory(const char* dir_name);

/**
 * @brief Altera o diretório de trabalho atual do sistema (Comando CD).
 * @param folder_name Nome do diretório para onde deseja navegar (use ".." para voltar).
 * @return 0 em caso de sucesso, ou -1 se o diretório não for encontrado.
 */
int fat32_change_directory(const char* folder_name);

/**
 * @brief Busca metadados de uma entrada específica pelo seu índice no diretório atual.
 * @return 1 se encontrou, 0 se chegou ao fim do diretório, -1 se houver erro.
 */
int fat32_get_entry_by_index(uint32_t cluster, int target_index, char* name_out, uint32_t* size_out, uint8_t* attr_out);

#endif /* FS_DIR_H */
