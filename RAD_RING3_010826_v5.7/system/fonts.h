#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

// =========================================================================
// 🔤 Configuração da Fonte Padrão do Sistema
// =========================================================================
#define FONT_WIDTH   8
#define FONT_HEIGHT  8

// =========================================================================
// 📦 Matriz de Fonte 8x8 (ASCII Completo)
// =========================================================================
// Cada caractere é composto por 8 linhas de 1 byte (1 bit = 1 pixel)
extern uint8_t font_8x8[256][8];

// =========================================================================
// ⚙️ Funções Auxiliares e Inicialização
// =========================================================================

/**
 * @brief Inicializa a tabela de fontes preenchendo caracteres vazios 
 * com o caractere de fallback '?'.
 */
void font_init(void);

/**
 * @brief Retorna a largura em pixels de um caractere.
 */
static inline int font_get_width(void) {
    return FONT_WIDTH;
}

/**
 * @brief Retorna a altura em pixels de um caractere.
 */
static inline int font_get_height(void) {
    return FONT_HEIGHT;
}

#endif // FONTS_H
