#ifndef EHCI_STORAGE_H
#define EHCI_STORAGE_H

#include <stdint.h>

// Definições de Classe USB Mass Storage
#define USB_MSC_CLASS            0x08
#define USB_MSC_SUBCLASS_SCSI    0x06
#define USB_MSC_PROTO_BOT        0x50  // Bulk-Only Transport

// Assinaturas mágicas exigidas pela especificação USB BOT
#define CBW_SIGNATURE            0x43425355 // "USBC" (Little Endian)
#define CSW_SIGNATURE            0x53425355 // "USBS" (Little Endian)

// Comandos SCSI Essenciais
#define SCSI_CMD_INQUIRY          0x12
#define SCSI_CMD_TEST_UNIT_READY  0x00
#define SCSI_CMD_READ_CAPACITY_10 0x25
#define SCSI_CMD_READ_10          0x28
#define SCSI_CMD_WRITE_10         0x2A

// Flags de direção do CBW
#define CBW_DIR_OUT               0x00
#define CBW_DIR_IN                0x80

// Estrutura do Command Block Wrapper (CBW) - 31 Bytes obrigatórios
typedef struct {
    uint32_t signature;          // Deve ser CBW_SIGNATURE
    uint32_t tag;                // ID único gerado pelo host para rastreio
    uint32_t data_transfer_len;  // Número de bytes que serão transferidos a seguir
    uint8_t  flags;              // Bit 7: Direção (0=Out, 1=In)
    uint8_t  lun;                // Logical Unit Number (geralmente 0)
    uint8_t  cb_length;          // Tamanho real do comando SCSI interno (de 1 a 16)
    uint8_t  cb[16];             // O comando SCSI propriamente dito
} __attribute__((packed)) ehci_cbw_t;

// Estrutura do Command Status Wrapper (CSW) - 13 Bytes obrigatórios
typedef struct {
    uint32_t signature;          // Deve ser CSW_SIGNATURE
    uint32_t tag;                // Deve bater com o tag enviado no CBW
    uint32_t data_residue;       // Quantidade de dados que faltou transferir
    uint8_t  status;             // 0=Sucesso, 1=Falha, 2=Fase de erro
} __attribute__((packed)) ehci_csw_t;

// Protótipos das funções do Driver de Armazenamento USB

// Mantido 'addr' pois é usado internamente no escaneamento de baixo nível
int ehci_storage_init(uint8_t addr, uint32_t* out_max_lba, uint32_t* out_block_size);
int ehci_storage_read_capacity(uint8_t addr, uint32_t* max_lba, uint32_t* block_size);

// SINCRONIZADO COM O ECOSSISTEMA: Primeiro parâmetro alterado para dev_id
int ehci_storage_read_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, void* buffer);
int ehci_storage_write_sectors(uint8_t dev_id, uint32_t lba, uint32_t count, const void* buffer);

// Mantido 'addr' pois interage direto com o endpoint e os anéis de QH/qTD do chip
int ehci_enviar_bulk(uint8_t addr, uint8_t endpoint, void* data_buf, uint32_t data_len, int direction_in);

#endif // EHCI_STORAGE_H
