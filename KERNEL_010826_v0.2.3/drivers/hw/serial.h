#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stdbool.h>

/* --- Parâmetros Padrão do Protocolo Serial --- */
#define SERIAL_PARITY_NONE  0
#define SERIAL_PARITY_ODD   1
#define SERIAL_PARITY_EVEN  2

#define SERIAL_STOP_BITS_1  1
#define SERIAL_STOP_BITS_2  2

#define MAX_SERIAL_PORTS    4

// --- Estrutura da Porta Serial ---
typedef struct {
    /* 1. Identificação do Hardware */
    uint16_t port_base; // Ex: 0x3F8 (COM1)
    uint8_t  irq;       // Ex: 4

    /* --- O SEU STATUS DE CONEXÃO (OS LOCK) --- */
    bool in_use;        // true = Algum programa (Ring 3) deu sys_open nesta porta

    /* 2. Configurações do Protocolo */
    uint32_t baud_rate; 
    uint8_t  data_bits; 
    uint8_t  stop_bits; 
    uint8_t  parity;    

    /* 3. Fila de Recepção (RX) */
    volatile char rx_buffer[256];
    volatile int  rx_head; 
    volatile int  rx_tail; 
    
    /* 4. Fila de Transmissão (TX) */
    volatile char tx_buffer[256];
    volatile int  tx_head;
    volatile int  tx_tail;
} SerialPort;

// Vinculação externa do array global
extern SerialPort system_serial_ports[MAX_SERIAL_PORTS];

// --- API Pública do Driver ---
int  driver_serial_init(SerialPort* port, uint16_t base, uint8_t irq, uint32_t baud);
void driver_serial_handle_interrupt(SerialPort* port); 
bool serial_available(SerialPort* port);
bool serial_read(SerialPort* port, char* data);
void serial_write(SerialPort* port, char data);
void serial_print(SerialPort* port, const char* str);

#endif
