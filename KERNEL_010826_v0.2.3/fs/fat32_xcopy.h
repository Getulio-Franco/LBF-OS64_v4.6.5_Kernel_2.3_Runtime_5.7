/**
 * ============================================================================
 * FAT32 COPY - HEADER V3.3 (NATIVE AHCI/SATA SYNC)
 * ============================================================================
 * Descrição: Protótipos para operações de clonagem e cópia de arquivos.
 * Idioma do Código: Inglês | Idioma dos Comentários: Português
 * ============================================================================
 */

#ifndef FS_FAT32_COPY_H
#define FS_FAT32_COPY_H

#include <stdint.h>
#include "fs/fat32_file.h" // Importa os códigos de status FS_SUCCESS, FS_NOT_FOUND, etc.

/**
 * @brief Copia um arquivo de uma origem para um destino dentro do diretório atual.
 * A função lê todo o conteúdo do arquivo de origem para a memória (heap) e o
 * grava em um novo arquivo com o nome de destino.
 * * @param source      Nome do arquivo de origem (ex: "README.TXT").
 * @param destination Nome do novo arquivo (ex: "COPY.TXT").
 * @return int        FS_SUCCESS (0) em caso de sucesso, ou código de erro negativo.
 */
int fat32_copy_file(const char* source, const char* destination);

#endif /* FS_FAT32_COPY_H */
