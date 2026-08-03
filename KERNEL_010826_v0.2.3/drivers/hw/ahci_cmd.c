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
    if (!port || port_no != 0) { // Trava para impedir leitura de portas fantasmas
        return false;
    }

    port->is = 0xFFFFFFFF; // Limpa erros
    int slot = 0;

    uint64_t clb_phys = ((uint64_t)port->clbu << 32) | port->clb;
    
    // 🛡️ ESCUDO DE MEMÓRIA: Verifica se o CLB lido bate com a arquitetura definida em ahci_mem.c
    uint64_t expected_clb = AHCI_MEM_SAFE_BASE + (port_no * 0x100000);
    if (clb_phys != expected_clb) {
        vga_print_string("\n[AHCI_CMD] ERRO: Corrupcao detectada! CLB nao bate com AHCI_MEM_SAFE_BASE", 0, 39);
        return false;
    }

    ahci_cmd_header_t* cmd_header = &((ahci_cmd_header_t*)(uintptr_t)clb_phys)[slot];
    uint64_t ct_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;

    // 🛡️ ESCUDO DE MEMÓRIA: Verifica se o CT_PHYS lido é o esperado (CLB + 8192 bytes)
   /* uint64_t expected_ct = expected_clb + 8192 + (slot * 256);
    if (ct_phys != expected_ct) {
        vga_print_string("\n[AHCI_CMD] ERRO: Corrupcao detectada no CTBA do cabecalho!", 0, 39);
        return false; // Aborta antes do memset para SALVAR o S.O.
    }*/
    
    // 🛡️ ESCUDO DE MEMÓRIA: Verifica se o CT_PHYS lido é o esperado (CLB + 8192 bytes)
    uint64_t expected_ct = expected_clb + 8192 + (slot * 256);
    if (ct_phys != expected_ct) {
        vga_print_string("\n[AHCI_CMD] AVISO: CTBA corrompido! Aplicando auto-recuperacao...", 0, 39);
        
        // 🛠️ AUTO-RECUPERAÇÃO: Reescreve os ponteiros vitais de volta no cabeçalho na RAM
        cmd_header->ctba = (uint32_t)(expected_ct & 0xFFFFFFFF);
        cmd_header->ctbau = (uint32_t)(expected_ct >> 32);
        cmd_header->prdtl = 1;
        cmd_header->cfl = 5;
        
        // Atualiza a variável local para usar o endereço correto reconstruído
        ct_phys = expected_ct; 
    }

    // Se passou pelo escudo, a memória é 100% segura para sofrer memset
    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)(uintptr_t)ct_phys;
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    cmd_table->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
    
    uint32_t total_bytes = count * 512;
    cmd_table->prdt_entry[0].dbc = (total_bytes - 1) | (1U << 31);

    cmd_header->prdtl = 1;
    cmd_header->w = 0; 

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmd_table->cfis;
    fis->fis_type = 0x27;
    fis->c = 1;           

    bool lba48 = (lba > 0x0FFFFFFF) || (count > 256);
    if (lba48) {
        fis->command = 0x25;
        fis->device = 0x40;  
        fis->lba3 = (uint8_t)(lba >> 24);
        fis->lba4 = (uint8_t)(lba >> 32);
        fis->lba5 = (uint8_t)(lba >> 40);
    } else {
        fis->command = 0xC8; 
        fis->device = 0xE0 | ((lba >> 24) & 0x0F); 
    }

    fis->lba0 = (uint8_t)(lba >> 0);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    cmd_header->cfl = 5;
    __asm__ volatile("mfence" ::: "memory");

    port->ci = (1 << slot);
    __asm__ volatile("mfence" ::: "memory");

    uint32_t timeout = 2000000; 
    while (timeout--) {
        if (!(port->ci & (1 << slot))) {
            break;
        }
        
        if ((port->is & (1 << 30)) || (port->tfd & 0x01)) {
            port->is = 0xFFFFFFFF;
            return false; 
        }
        __asm__ volatile("pause");
    }

    if (timeout == 0) return false; 

    return true; 
}


bool ahci_cmd_escrever_setores(volatile ahci_port_reg_t* port, int port_no, uint64_t lba, uint32_t count, uint64_t buffer_phys) {
    if (!port || port_no != 0) return false;

    port->is = 0xFFFFFFFF;
    int slot = 0;

    uint64_t clb_phys = ((uint64_t)port->clbu << 32) | port->clb;
    
    // 🛡️ ESCUDO DE MEMÓRIA DA ESCRITA
    uint64_t expected_clb = AHCI_MEM_SAFE_BASE + (port_no * 0x100000);
    if (clb_phys != expected_clb) return false;

    ahci_cmd_header_t* cmd_header = &((ahci_cmd_header_t*)(uintptr_t)clb_phys)[slot];
    uint64_t ct_phys = ((uint64_t)cmd_header->ctbau << 32) | cmd_header->ctba;

    uint64_t expected_ct = expected_clb + 8192 + (slot * 256);
    if (ct_phys != expected_ct) return false; // Bloqueia corrupção

    ahci_cmd_table_t* cmd_table = (ahci_cmd_table_t*)(uintptr_t)ct_phys;
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    cmd_table->prdt_entry[0].dba = (uint32_t)(buffer_phys & 0xFFFFFFFF);
    cmd_table->prdt_entry[0].dbau = (uint32_t)(buffer_phys >> 32);
    
    uint32_t total_bytes = count * 512;
    cmd_table->prdt_entry[0].dbc = (total_bytes - 1) | (1U << 31);

    cmd_header->prdtl = 1;
    cmd_header->w = 1; 

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)cmd_table->cfis;
    fis->fis_type = 0x27; 
    fis->c = 1;           

    bool lba48 = (lba > 0x0FFFFFFF) || (count > 256);
    if (lba48) {
        fis->command = 0x35; 
        fis->device = 0x40;  
        fis->lba3 = (uint8_t)(lba >> 24);
        fis->lba4 = (uint8_t)(lba >> 32);
        fis->lba5 = (uint8_t)(lba >> 40);
    } else {
        fis->command = 0xCA; 
        fis->device = 0xE0 | ((lba >> 24) & 0x0F); 
    }

    fis->lba0 = (uint8_t)(lba >> 0);
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    cmd_header->cfl = 5;
    __asm__ volatile("mfence" ::: "memory");

    port->ci = (1 << slot);
    __asm__ volatile("mfence" ::: "memory");

    uint32_t timeout = 2000000; 
    while (timeout--) {
        if (!(port->ci & (1 << slot))) {
            break;
        }
        
        if ((port->is & (1 << 30)) || (port->tfd & 0x01)) {
            port->is = 0xFFFFFFFF; 
            return false; 
        }
        __asm__ volatile("pause");
    }

    if (timeout == 0) return false; 

    return true; 
}
