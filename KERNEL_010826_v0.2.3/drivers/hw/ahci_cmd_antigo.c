#include "ahci_cmd.h"
#include "ahci_mem.h" // Para pegar a constante AHCI_MEM_SAFE_BASE
#include "../../util/string.h"
#include "../../drivers/video.h"

// Estrutura física de uma entrada na tabela PRDT (Physical Region Descriptor Table)
typedef struct {
    uint32_t dba;       // Endereço base de dados (32 bits inferiores)
    uint32_t dbau;      // Endereço base de dados superior (32 bits superiores)
    uint32_t rsv0;      // Reservado
    uint32_t dbc:22;    // Data Byte Count (Número de bytes menos 1)
    uint32_t rsv1:9;    // Reservado
    uint32_t i:1;       // Interrupt on completion bit
} __attribute__((packed)) ahci_prdt_entry_t;

// Estrutura simplificada da Command Table usada para leitura
typedef struct {
    uint8_t  cfis[64];        // Command FIS (20 a 64 bytes)
    uint8_t  acmd[16];        // ATAPI Command (16 bytes)
    uint8_t  rsv[48];         // Reservado
    ahci_prdt_entry_t prdt_entry[1]; // Nossa tabela configurada com 1 entrada no Passo 4
} __attribute__((packed)) ahci_cmd_table_t;

// Estrutura do cabeçalho de comando padrão (mesma mapeada no Passo 4)
typedef struct {
    uint32_t cfl:5; uint32_t a:1; uint32_t w:1; uint32_t p:1;
    uint32_t r:1; uint32_t b:1; uint32_t c:1; uint32_t rsv0:1;
    uint32_t pmp:4; uint32_t prdtl:16; uint32_t prdbc;
    uint32_t ctba; uint32_t ctbau; uint32_t rsv1[4];
} __attribute__((packed)) ahci_cmd_header_t;

// Estrutura padrão de um FIS de Registro do Host para o Dispositivo (H2D)
typedef struct {
    uint8_t  fis_type;   // Tipo do FIS (0x27 para RegH2D)
    uint8_t  pmport:4;   // Port multiplier
    uint8_t  rsv0:3;
    uint8_t  c:1;        // Command/Control bit (1 = Comando, 0 = Controle)
    uint8_t  command;    // Comando ATA (ex: 0x25 para READ DMA EXT)
    uint8_t  featuresl;  // Features low
    uint8_t  lba0;       // LBA byte 0
    uint8_t  lba1;       // LBA byte 1
    uint8_t  lba2;       // LBA byte 2
    uint8_t  device;     // Device register
    uint8_t  lba3;       // LBA byte 3
    uint8_t  lba4;       // LBA byte 4
    uint8_t  lba5;       // LBA byte 5
    uint8_t  featuresh;  // Features high
    uint8_t  countl;     // Sector count low
    uint8_t  counth;     // Sector count high
    uint8_t  rsv1;
    uint8_t  control;    // Control register
    uint8_t  rsv2[4];
} __attribute__((packed)) fis_reg_h2d_t;

bool ahci_cmd_ler_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t buffer_phys) {
    // 1. Limpa qualquer erro residual na porta para não abortar o comando antes de começar
    port->is = 0xFFFFFFFF;
    
    // 2. Procura um slot de comando livre na controladora (Usaremos sempre o Slot 0 para simplificar)
    int slot = 0;

    // 3. Obtém o ponteiro virtual do cabeçalho de comando correspondente (Configurado no Passo 4)
    uint64_t clb_addr = ((uint64_t)port->clbu << 32) | port->clb;
    ahci_cmd_header_t* cmd_header = &((ahci_cmd_header_t*)(uintptr_t)clb_addr)[slot];

    // 4. Obtém o ponteiro virtual da Command Table associada a esse slot
    uint64_t ct_addr = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)(uintptr_t)ct_addr;
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    // 5. Preenche a entrada PRDT (Configurando o destino do DMA físico)
    cmd_table->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
    // IMPORTANTE: dbc deve ser (total_bytes - 1) | bit 31 ligado para sinalizar interrupção ao terminar
    uint32_t total_bytes = count * 512;
    cmd_table->prdt_entry[0].dbc = (total_bytes - 1) | (1U << 31);

    // Atualiza o cabeçalho dizendo que há 1 entrada PRDT válida e que é uma operação de LEITURA (w = 0)
    cmd_header->prdtl = 1;
    cmd_header->w = 0; 

    // 6. Monta o pacote FIS do tipo Registro H2D para o protocolo SATA
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmd_table->cfis;
    fis->fis_type = 0x27; // FIS_TYPE_REG_H2D
    fis->c = 1;           // É um comando de execução

    // Determina se usa LBA28 ou LBA48 baseado na posição do setor ou tamanho
    bool lba48 = (lba > 0x0FFFFFFF) || (count > 256);
    if (lba48) {
        fis->command = 0x25; // ATA_CMD_READ_DMA_EXT
        fis->device = 0x40;  // Modo LBA nativo
        fis->lba3 = (uint8_t)(lba >> 24);
        fis->lba4 = (uint8_t)(lba >> 32);
        fis->lba5 = (uint8_t)(lba >> 40);
    } else {
        fis->command = 0xC8; // ATA_CMD_READ_DMA
        fis->device = 0xE0 | ((lba >> 24) & 0x0F); // Modo LBA28 clássico
    }

    fis->lba0 = (uint8_t)(lba >> 0);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    // Define o tamanho do FIS no cabeçalho (5 dwords = 20 bytes)
    cmd_header->cfl = 5;
    __asm__ volatile("mfence");

    // 7. Avisa o hardware para executar marcando o bit do slot correspondente em Command Issue (CI)
    port->ci = (1 << slot);
    __asm__ volatile("mfence");

    // 8. Aguarda a conclusão pelo hardware (Polling defensivo com timeout)
    uint32_t timeout = 2000000; // Cerca de 2 segundos de limite
    while (timeout--) {
        // Quando a transferência via DMA termina, o hardware limpa automaticamente o bit em port->ci
        if (!(port->ci & (1 << slot))) {
            break;
        }
        
        // Verifica se a controladora reportou algum erro de barramento ou erro no disco (TFES)
        if ((port->is & (1 << 30)) || (port->tfd & 0x01)) {
            port->is = 0xFFFFFFFF; // Limpa erros
            return false; // Abortado por erro de hardware
        }
        __asm__ volatile("pause");
    }

    if (timeout == 0) {
        return false; // Falha por Timeout (o disco congelou)
    }

    return true; // Sucesso absoluto! Dados transferidos via DMA para a RAM.
}

bool ahci_cmd_escrever_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t buffer_phys) {
    (void)port_no; // Evita aviso de variável não utilizada se não for usada no escopo

    // 1. Limpa qualquer erro residual na porta para não abortar o comando antes de começar
    port->is = 0xFFFFFFFF;
    
    // 2. Procura um slot de comando livre na controladora (Mantendo o Slot 0 igual à leitura)
    int slot = 0;

    // 3. Obtém o ponteiro virtual do cabeçalho de comando correspondente
    uint64_t clb_addr = ((uint64_t)port->clbu << 32) | port->clb;
    ahci_cmd_header_t* cmd_header = &((ahci_cmd_header_t*)(uintptr_t)clb_addr)[slot];

    // 4. Obtém o ponteiro virtual da Command Table associada a esse slot
    uint64_t ct_addr = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;
    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)(uintptr_t)ct_addr;
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    // 5. Preenche a entrada PRDT (Configurando a origem do DMA físico - de onde os dados saem)
    cmd_table->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
    
    uint32_t total_bytes = count * 512;
    cmd_table->prdt_entry[0].dbc = (total_bytes - 1) | (1U << 31);

    // Atualiza o cabeçalho dizendo que há 1 entrada PRDT válida e que é uma operação de ESCRITA (w = 1)
    cmd_header->prdtl = 1;
    cmd_header->w = 1; // MODIFICAÇÃO CRÍTICA: w = 1 ativa a direção RAM -> DISCO

    // 6. Monta o pacote FIS do tipo Registro H2D para o protocolo SATA
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmd_table->cfis;
    fis->fis_type = 0x27; // FIS_TYPE_REG_H2D
    fis->c = 1;           // É um comando de execução

    // Determina se usa LBA28 ou LBA48 baseado na posição do setor ou tamanho
    bool lba48 = (lba > 0x0FFFFFFF) || (count > 256);
    if (lba48) {
        fis->command = 0x35; // MODIFICAÇÃO CRÍTICA: ATA_CMD_WRITE_DMA_EXT
        fis->device = 0x40;  // Modo LBA nativo
        fis->lba3 = (uint8_t)(lba >> 24);
        fis->lba4 = (uint8_t)(lba >> 32);
        fis->lba5 = (uint8_t)(lba >> 40);
    } else {
        fis->command = 0xCA; // MODIFICAÇÃO CRÍTICA: ATA_CMD_WRITE_DMA (LBA28 clássico)
        fis->device = 0xE0 | ((lba >> 24) & 0x0F); 
    }

    fis->lba0 = (uint8_t)(lba >> 0);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    // Define o tamanho do FIS no cabeçalho (5 dwords = 20 bytes)
    cmd_header->cfl = 5;
    __asm__ volatile("mfence");

    // 7. Avisa o hardware para executar
    port->ci = (1 << slot);
    __asm__ volatile("mfence");

    // 8. Aguarda a conclusão pelo hardware (Polling defensivo com timeout)
    uint32_t timeout = 2000000; 
    while (timeout--) {
        if (!(port->ci & (1 << slot))) {
            break;
        }
        
        // Verifica erros
        if ((port->is & (1 << 30)) || (port->tfd & 0x01)) {
            port->is = 0xFFFFFFFF; 
            return false; 
        }
        __asm__ volatile("pause");
    }

    if (timeout == 0) {
        return false; // Falha por Timeout
    }

    return true; // Sucesso absoluto! Dados transferidos via DMA para o disco.
}
