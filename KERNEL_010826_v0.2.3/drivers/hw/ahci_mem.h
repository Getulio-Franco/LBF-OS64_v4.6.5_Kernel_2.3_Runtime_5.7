#ifndef AHCI_MEM_H
#define AHCI_MEM_H

#include "ahci_reset.h" // Para herdar a estrutura ahci_port_reg_t
#include <stdint.h>
#include <stdbool.h>

// Endereço físico e virtual base estático (Região segura no seu Identity Map)
#define AHCI_MEM_SAFE_BASE   0x02000000 

/**
 * @brief Realiza o processo de Rebase (alocação e alinhamento de estruturas de comandos)
 * para uma porta SATA específica que foi detectada ativa.
 * @param port Ponteiro volátil para os registradores da porta.
 * @param port_no Número da porta (ex: 0).
 * @return true se o rebase foi bem-sucedido e as tabelas alinhadas, false caso contrário.
 */
bool ahci_mem_configurar_porta(volatile ahci_port_reg_t* port, int port_no);

#endif // AHCI_MEM_H
