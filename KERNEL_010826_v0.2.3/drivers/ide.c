/**
 * ============================================================================
 * IDE PIO DRIVER - VERSÃO 1.1 (Sincronizada)
 * ============================================================================
 */

#include "drivers/ide.h"
#include "io.h" 

/* * REGISTRADORES DO CONTROLADOR IDE (Primary Bus)
 */
#define IDE_DATA         0x1F0 // Porta de dados (16-bit)
#define IDE_ERROR        0x1F1 // Erro (Leitura) / Precomp (Escrita)
#define IDE_SECTORCOUNT  0x1F2 // Número de setores para ler/escrever
#define IDE_LBA_LOW      0x1F3 // LBA bits 0-7
#define IDE_LBA_MID      0x1F4 // LBA bits 8-15
#define IDE_LBA_HIGH     0x1F5 // LBA bits 16-23
#define IDE_DEVICE       0x1F6 // Seleção de Drive e bits LBA 24-27
#define IDE_STATUS       0x1F7 // Status atual do controlador (Leitura)
#define IDE_COMMAND      0x1F7 // Comando para o controlador (Escrita)

/**
 * @brief ide_wait_busy
 * Aguarda até que o bit BSY (Busy) seja limpo.
 */
static void ide_wait_busy() {
    // 0x80 é o bit BSY (Busy)
    while (inb(IDE_STATUS) & 0x80);// Espera o bit BUSY limpar
    while (!(inb(0x1F7) & 0x40)); // Espera o bit READY setar
}

/**
 * @brief ide_wait_ready
 * Aguarda até que o bit DRQ (Data Request) esteja pronto.
 */
static void ide_wait_ready() {
    uint8_t status;
    // Pequeno atraso (400ns): O padrão ATA pede para ler o status 
    // algumas vezes para dar tempo do hardware reagir ao comando.
    inb(IDE_STATUS); inb(IDE_STATUS);
    inb(IDE_STATUS); inb(IDE_STATUS);

    while (1) {
        status = inb(IDE_STATUS);
        // O bit 0x01 (ERR) indica que o disco falhou. 
        // Se houver erro, a gente precisa sair do loop.
        if (status & 0x01) break; 
        if (!(status & 0x80) && (status & 0x08)) break;
    }
}

/**
 * @brief ide_read_sector
 * Lê 512 bytes do disco.
 */
int ide_read_sector(uint32_t lba, uint8_t *buffer) {
    ide_wait_busy();

    // Configura o endereçamento LBA para o Drive Master (0xE0)
    outb(IDE_DEVICE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IDE_SECTORCOUNT, 1);
    outb(IDE_LBA_LOW, (uint8_t)lba);
    outb(IDE_LBA_MID, (uint8_t)(lba >> 8));
    outb(IDE_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // Comando 0x20: Read Sectors
    outb(IDE_COMMAND, 0x20);

    ide_wait_ready();

    // Transferência 16-bit: 256 words = 512 bytes
    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(IDE_DATA);
    }

    return 0;
}

/**
 * @brief ide_write_sector
 * Escreve 512 bytes no disco.
 */
int ide_write_sector(uint32_t lba, uint8_t *buffer) {
    ide_wait_busy();

    outb(IDE_DEVICE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IDE_SECTORCOUNT, 1);
    outb(IDE_LBA_LOW, (uint8_t)lba);
    outb(IDE_LBA_MID, (uint8_t)(lba >> 8));
    outb(IDE_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // Comando 0x30: Write Sectors
    outb(IDE_COMMAND, 0x30);

    ide_wait_ready();

    // Envia os dados
    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(IDE_DATA, ptr[i]);
    }

    // Flush do cache para garantir a persistência física
    outb(IDE_COMMAND, 0xE7); 
    ide_wait_busy();

    return 0;
}
