#ifndef DOUBLE_BUFFERING_H
#define DOUBLE_BUFFERING_H

#include <stdint.h>

/**
 * Inicializa o backbuffer na RAM
 */
void db_init(void);

/**
 * O Compositor: Verifica flags, chama wm_render_pipeline() e faz o video_flush()
 */
void db_swap_buffers(void);

/**
 * Retorna o FPS calculado no último segundo
 */
uint64_t db_get_fps(void);

/**
 * Retorna o endereço do buffer de desenho na RAM
 */
uint32_t* db_get_backbuffer_address(void);

#endif
