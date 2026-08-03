/**
 * ============================================================================
 * IDE PIO DRIVER HEADER - VERSÃO 1.1
 * ============================================================================
 */

#ifndef IDE_H
#define IDE_H

#include <stdint.h>

/**
 * @brief Lê um setor (512 bytes) do disco rígido via barramento IDE.
 * * @param lba    Endereço Lógico do Bloco (setor de 512 bytes).
 * @param buffer Ponteiro para a memória que receberá os dados.
 * @return int   Retorna 0 em caso de sucesso.
 */
int ide_read_sector(uint32_t lba, uint8_t *buffer);

/**
 * @brief Escreve um setor (512 bytes) no disco rígido.
 * * @param lba    Endereço Lógico do Bloco onde os dados serão gravados.
 * @param buffer Ponteiro para os dados a serem enviados.
 * @return int   Retorna 0 em caso de sucesso.
 * * Nota: Esta função executa um Cache Flush (0xE7) automaticamente.
 */
int ide_write_sector(uint32_t lba, uint8_t *buffer);

/* * NOTA DE ARQUITETURA:
 * As funções ide_wait_busy() e ide_wait_ready() foram removidas do header
 * e tornadas static no ide.c para evitar poluição do namespace global e 
 * garantir que apenas o driver IDE gerencie seus próprios tempos de espera.
 */

#endif /* IDE_H */
