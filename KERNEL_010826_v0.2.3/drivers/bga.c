/**
 * ============================================================================
 * LBF OS - BGA Driver Implementation
 * ============================================================================
 */

#include "bga.h"
#include "io.h" // Certifique-se de ajustar o caminho correto para o seu io.h (ex: "drivers/io.h")

/**
 * @brief Escreve um valor de 16 bits em um registrador interno do chip BGA.
 */
static void bga_write_register(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

/**
 * @brief Lê um valor de 16 bits de um registrador interno do chip BGA.
 */
static uint16_t bga_read_register(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

int bga_is_available(void) {
    uint16_t id = bga_read_register(VBE_DISPI_INDEX_ID);
    // Verifica os padrões de IDs válidos injetados pelo Bochs/VirtualBox
    return (id >= 0xB0C0 && id <= 0xB0C5);
}

void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    // 1. Desativa o chip temporariamente para evitar corrupção visual durante o ajuste
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    
    // 2. Injeta a geometria da tela diretamente nos registradores do chip
    bga_write_register(VBE_DISPI_INDEX_XRES, width);
    bga_write_register(VBE_DISPI_INDEX_YRES, height);
    bga_write_register(VBE_DISPI_INDEX_BPP, bpp);
    
    // 3. Reativa o chip forçando o mapeamento em Linear Framebuffer (LFB)
    bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}
