#include "serial.h"
#include "../io.h"       // Certifique-se de que este arquivo tenha a função io_wait()
#include "../video.h"    // Para logs via terminal_print

#define UART_RX(port)  (port->port_base + 0)
#define UART_TX(port)  (port->port_base + 0)
#define UART_DLL(port) (port->port_base + 0)
#define UART_DLM(port) (port->port_base + 1)
#define UART_IER(port) (port->port_base + 1)
#define UART_IIR(port) (port->port_base + 2)
#define UART_FCR(port) (port->port_base + 2)
#define UART_LCR(port) (port->port_base + 3)
#define UART_MCR(port) (port->port_base + 4)
#define UART_LSR(port) (port->port_base + 5)

#define LCR_8_DATA_BITS    0x03
#define LCR_1_STOP_BIT     0x00
#define LCR_NO_PARITY      0x00
#define LCR_DLAB_ENABLE    0x80

// Array que guarda o estado real das 4 portas físicas do PC
SerialPort system_serial_ports[MAX_SERIAL_PORTS];

int driver_serial_init(SerialPort* port, uint16_t base, uint8_t irq, uint32_t baud) {
    // Define os padrões se os argumentos forem vazios (0)
    port->port_base = (base == 0) ? 0x3F8 : base; // Default: COM1 (0x3F8)
    port->irq       = (irq == 0)  ? 4     : irq;  // Default: IRQ4 (COM1)
    port->baud_rate = (baud == 0) ? 9600  : baud; // Default: 9600 bps (Igual ao seu Arduino)
    
    // Configurações imutáveis do protocolo básico 8N1
    port->data_bits = 8;
    port->stop_bits = SERIAL_STOP_BITS_1;
    port->parity    = SERIAL_PARITY_NONE;
    
    // Status de controle do S.O.
    port->in_use    = false; 

    // Zera os ponteiros dos Buffers de software
    port->rx_head = 0; port->rx_tail = 0;
    port->tx_head = 0; port->tx_tail = 0;

    // 1. Desabilita interrupções da UART para configuração física de registradores
    outb(UART_IER(port), 0x00);
    io_wait();
    
    // 2. Configura a Velocidade (Ativa DLAB)
    outb(UART_LCR(port), LCR_DLAB_ENABLE); 
    io_wait();
    
    // Calcula e carrega os divisores de Baud Rate baseado no clock da UART (115200)
    uint16_t divisor = (uint16_t)(115200 / port->baud_rate); 
    outb(UART_DLL(port), divisor & 0xFF);
    io_wait();
    outb(UART_DLM(port), (divisor >> 8) & 0xFF);
    io_wait();
    
    // 3. Aplica o formato final: 8 Bits, 1 Stop Bit, Sem Paridade (Isso desativa o DLAB)
    uint8_t protocol_config = LCR_8_DATA_BITS | LCR_1_STOP_BIT | LCR_NO_PARITY;
    outb(UART_LCR(port), protocol_config); 
    io_wait();
    
    // 4. Habilita FIFOs, limpa buffers de hardware e define gatilho de interrupção
    outb(UART_FCR(port), 0xC7);
    io_wait();
    
    // 5. Configura Linhas de controle de modem (DTR, RTS e liga interrupções na placa-mãe)
    outb(UART_MCR(port), 0x0B);
    io_wait();
    
    // 6. Habilita a Interrupção Física de Recebimento de Dados (RX)
    outb(UART_IER(port), 0x01); 
    io_wait();

    return 0;
}

void driver_serial_handle_interrupt(SerialPort* port) {
    // 1. Lemos o IIR apenas uma vez para o VirtualBox saber que reconhecemos o evento
    uint8_t iir = inb(UART_IIR(port));
    
    // 2. Loop direto no LSR: Enquanto o Bit 0 (Data Ready) for 1, há dados reais no chip!
    while (inb(UART_LSR(port)) & 0x01) {
        char c = inb(UART_RX(port)); // Lê o byte físico enviado pelo Arduino
        
        // Calcula a próxima posição da nossa fila circular (Ring 3)
        int next = (port->rx_head + 1) % 256;
        
        // Se a fila não estiver cheia, guarda o caractere
        if (next != port->rx_tail) {
            port->rx_buffer[port->rx_head] = c;
            port->rx_head = next;
        }
    }
}

bool serial_available(SerialPort* port) {
    return (port->rx_head != port->rx_tail);
}

bool serial_read(SerialPort* port, char* data) {
    if (!serial_available(port)) return false;
    *data = port->rx_buffer[port->rx_tail];
    port->rx_tail = (port->rx_tail + 1) % 256;
    return true;
}

void serial_write(SerialPort* port, char data) {
    // Espera o registrador de transmissão ficar vazio (Bit 5 do LSR - THRE)
    volatile uint32_t timeout = 100000;
    while (!(inb(UART_LSR(port)) & 0x20) && --timeout) {
        io_wait();
    }
    outb(UART_TX(port), data);
}

void serial_print(SerialPort* port, const char* str) {
    while (*str) {
        serial_write(port, *str++);
    }
}
