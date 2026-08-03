#ifndef AHCI_HAL_H
#define AHCI_HAL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa completamente o subsistema SATA/AHCI detectando e configurando todos os discos disponíveis.
 * @return true se uma controladora foi configurada e há pelo menos um disco pronto para uso.
 */
bool ahci_hal_inicializar(void);

/**
 * @brief Função padronizada para ler setores de um disco SATA específico.
 * @param dev_id ID lógico do dispositivo SATA (0 para C:, 1 para D:, etc.).
 * @param lba Setor lógico inicial.
 * @param count Quantidade de setores a ler.
 * @param buffer Ponteiro (virtual) de destino onde os dados serão armazenados.
 * @return 0 em caso de sucesso, ou um código de erro negativo se falhar.
 */
int ahci_hal_ler(uint8_t dev_id, uint64_t lba, uint32_t count, void* buffer);

/**
 * @brief Função padronizada para escrever setores em um disco SATA específico.
 * @param dev_id ID lógico do dispositivo SATA.
 * @param lba Setor lógico inicial.
 * @param count Quantidade de setores a escrever.
 * @param buffer Ponteiro (virtual) de origem contendo os dados a serem gravados.
 * @return 0 em caso de sucesso, ou um código de erro negativo se falhar.
 */
int ahci_hal_escrever(uint8_t dev_id, uint64_t lba, uint32_t count, const void* buffer);

#endif // AHCI_HAL_H
