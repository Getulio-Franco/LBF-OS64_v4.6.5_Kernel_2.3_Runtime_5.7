/**
 * ============================================================================
 * FAT32 FILE OPERATIONS - HEADER V3.3
 * ============================================================================
 * Descrição: Definições de códigos de retorno e protótipos para arquivos.
 * Localização: fs/fat32_file.h
 * ============================================================================
 */

#ifndef FS_FAT32_FILE_H
#define FS_FAT32_FILE_H

#include <stdint.h>

/* --- FILE SYSTEM RETURN CODES --- */
#define FS_SUCCESS          0
#define FS_ERROR_READ      -1
#define FS_ERROR_WRITE     -2
#define FS_DISK_FULL       -3
#define FS_ALREADY_EXISTS  -4
#define FS_NOT_FOUND       -5

/* --- ESTRUTURAS DE DADOS DE INTERCÂMBIO RING0/RING3 --- */
typedef struct {
    uint32_t size;       // Tamanho em bytes
    uint8_t attributes;  // Atributos do FAT32 (0x10 indica se é diretório)
} file_info_t;

/* --- FILE SYSTEM CORE FUNCTIONS --- */

/**
 * @brief Lê e exibe o conteúdo de um arquivo de texto no console VESA.
 * @param filename Nome do arquivo (formato curto 8.3).
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_display_file(const char* filename);

/**
 * @brief Lê um arquivo completo para um buffer de memória.
 * @param filename Nome do arquivo.
 * @param buffer Ponteiro para onde os dados serão copiados.
 * @param max_size Tamanho máximo do buffer (limite de leitura).
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_read_file(const char* filename, uint8_t* buffer, uint32_t max_size);

/**
 * @brief Lê uma porção específica de um arquivo usando um deslocamento (offset).
 * Útil para carregar cabeçalhos de executáveis ELF ou arquivos grandes em partes.
 * @param filename Nome do arquivo.
 * @param buffer Buffer de destino.
 * @param size Quantidade de bytes a ler.
 * @param offset Ponto de partida dentro do arquivo (em bytes).
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_read_file_at_offset(const char* filename, uint8_t* buffer, uint32_t size, uint32_t offset);

/**
 * @brief Grava um novo arquivo no diretório atual. 
 * Se o arquivo já existir, ele será automaticamente sobrescrito.
 * @param filename Nome do arquivo.
 * @param input_buffer Dados a serem gravados.
 * @param size Tamanho dos dados em bytes.
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_write_file(const char* filename, uint8_t* input_buffer, uint32_t size);

/**
 * @brief Remove um arquivo do diretório e libera todos os seus clusters na tabela FAT.
 * @param filename Nome do arquivo a ser excluído.
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_delete_file(const char* filename);

/**
 * @brief Adiciona conteúdo ao final de um arquivo existente.
 * Gerencia dinamicamente a expansão da cadeia de clusters se necessário.
 * @param filename Nome do arquivo alvo.
 * @param data Dados/Buffer a serem anexados.
 * @param data_len Tamanho dos dados em bytes.
 * @return FS_SUCCESS ou código de erro.
 */
int fat32_append_file(const char* filename, uint8_t* data, uint32_t data_len);
int fat32_create_dir(const char* name);
int fat32_rename(const char* old_name, const char* new_name);
int fat32_stat(const char* name, file_info_t* out_info);

#endif /* FS_FAT32_FILE_H */
