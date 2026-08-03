#ifndef AHCI_CMD_H
#define AHCI_CMD_H

#include "ahci_reset.h" // Pega as definições das portas
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Envia um comando de leitura DMA de setores físicos para uma porta AHCI configurada.
 * @param port Ponteiro volátil para a porta SATA.
 * @param port_no Número da porta (ex: 0).
 * @param lba Endereço lógico do setor no disco (Logical Block Address).
 * @param count Quantidade de setores a ler (1 setor = 512 bytes).
 * @param buffer_phys Endereço FÍSICO da memória RAM onde os dados lidos serão injetados.
 * @return true se a leitura por DMA foi concluída com sucesso pelo hardware, false se deu timeout/erro.
 */
bool ahci_cmd_ler_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t buffer_phys);
bool ahci_cmd_escrever_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t buffer_phys);

#endif // AHCI_CMD_H
