#ifndef HW_PS2_H
#define HW_PS2_H

#include <stdint.h>

/* Portas de I/O do Controlador Intel 8042 */
#define PS2_DATA_PORT         0x60
#define PS2_STATUS_PORT       0x64
#define PS2_COMMAND_PORT      0x64

/* Comandos de Controle de Linha */
#define PS2_CMD_READ_CONFIG   0x20
#define PS2_CMD_WRITE_CONFIG  0x60
#define PS2_CMD_DISABLE_MOUSE 0xA7
#define PS2_CMD_ENABLE_MOUSE  0xA8
#define PS2_CMD_DISABLE_KBD   0xAD
#define PS2_CMD_ENABLE_KBD    0xAE

/**
 * @brief Inicializa o barramento i8042 e chama as funções nativas de inicialização
 * do teclado e do mouse que o seu sistema operacional já possui.
 */
void ps2_bus_init(void);

#endif
