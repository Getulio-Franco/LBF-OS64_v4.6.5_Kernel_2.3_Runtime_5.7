/**
 * ============================================================================
 * LBF OS - BGA (Bochs Graphics Adapter) Driver Module
 * Objetivo: Controle direto do chip gráfico emulado (VirtualBox / QEMU)
 * ============================================================================
 */

#ifndef BGA_H
#define BGA_H

#include <stdint.h>

// Portas de Entrada/Saída (I/O) do chip BGA
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

// Índices dos registradores internos do hardware
#define VBE_DISPI_INDEX_ID     0
#define VBE_DISPI_INDEX_XRES   1
#define VBE_DISPI_INDEX_YRES   2
#define VBE_DISPI_INDEX_BPP    3
#define VBE_DISPI_INDEX_ENABLE 4

// Constantes de controle do estado do chip
#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40

/* --- Protótipos das Funções do Driver --- */

/**
 * @brief Verifica se a placa gráfica emulada suporta as extensões BGA.
 * @return 1 se disponível, 0 se não suportado.
 */
int bga_is_available(void);

/**
 * @brief Força o chip gráfico a aplicar uma resolução customizada ignorando a BIOS.
 * @param width Largura desejada em pixels (ex: 1366, 1920)
 * @param height Altura desejada em pixels (ex: 768, 1080)
 * @param bpp Bits por pixel (geralmente 32)
 */
void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp);

#endif // BGA_H
