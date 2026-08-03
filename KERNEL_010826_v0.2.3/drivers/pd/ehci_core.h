#ifndef EHCI_CORE_H
#define EHCI_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Constantes de tempo USB (microsegundos aproximados)
#define EHCI_RESET_DELAY_MS         50      // 50ms reset pulse
#define EHCI_TRSTRCY_DELAY_MS       10      // 10ms reset recovery
#define EHCI_PORT_RESET_RECOVERY    200000  // ~200ms recovery
#define EHCI_SETUP_RETRIES          3

// Constantes de estado da porta
#define EHCI_PORT_CONNECTED         (1 << 0)
#define EHCI_PORT_ENABLE            (1 << 2)
#define EHCI_PORT_RESET             (1 << 8)
#define EHCI_PORT_SUSPEND           (1 << 12)
#define EHCI_PORT_POWER             (1 << 12)
#define EHCI_PORT_OWNER             (1 << 13)
#define EHCI_PORT_CHANGE_CONNECT    (1 << 17)
#define EHCI_PORT_CHANGE_ENABLE     (1 << 18)
#define EHCI_PORT_LINE_STATUS       (3 << 10)

// Estado do dispositivo USB
#define USB_DEVICE_ADDRESS_DEFAULT  0
#define USB_DEVICE_ADDRESS_MAX      127

// --- CORREÇÃO CRÍTICA: PIDs de 2 bits para o Token do qTD ---
#define EHCI_PID_OUT                0x00
#define EHCI_PID_IN                 0x01
#define EHCI_PID_SETUP              0x02

// Macros para os bits do Token qTD
#define EHCI_T_DATA0                (0U << 31)
#define EHCI_T_DATA1                (1U << 31)
#define EHCI_T_IOC                  (1 << 15) // Interrupt On Complete
#define EHCI_T_ACTIVE               (1 << 7)
#define EHCI_T_HALTED               (1 << 6)
#define EHCI_T_XACTERR              (1 << 3)

// Timeout para transferências
#define EHCI_TRANSFER_TIMEOUT       10000000

// Estrutura do qTD (Queue Element Transfer Descriptor - 32 bytes)
typedef struct ehci_qtd {
    volatile uint32_t next_qtd;      // Ponteiro para próximo qTD (endereço físico - 32 bits)
    volatile uint32_t alt_next_qtd;  // Alternativo para erro/curto-circuito
    volatile uint32_t token;         // Status, bytes, PID, toggle
    volatile uint32_t buffer[5];     // Ponteiros para buffer de dados (até 4KB)
} __attribute__((packed, aligned(32))) ehci_qtd_t;

// Estrutura do QH (Queue Head - 48 bytes + overlay de 32 bytes = 80 bytes)
typedef struct ehci_qh {
    volatile uint32_t horizontal_link; // Link para próximo QH (32 bits)
    volatile uint32_t endpoint_char;   // Endpoint characteristics
    volatile uint32_t endpoint_caps;   // Endpoint capabilities
    volatile uint32_t current_qtd;     // Current qTD pointer
    ehci_qtd_t overlay;                // Overlay qTD (8 DWords)
} __attribute__((packed, aligned(32))) ehci_qh_t;

// Estrutura do Pacote de Setup USB (8 Bytes)
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

// Device Descriptor (18 bytes)
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

// Protótipos
int ehci_core_init(void);
int ehci_detectar_dispositivos(void);
int ehci_reset_porta(uint8_t porta_idx);
int ehci_init_async_list(void);
int ehci_enviar_controle(uint8_t addr, usb_setup_packet_t* setup, void* data_buf, uint16_t data_len);
int ehci_ler_identificacao(void);
int ehci_set_address(uint8_t old_addr, uint8_t new_addr);
void ehci_wait_ms(uint32_t ms);
void ehci_dump_qh(const char* name, ehci_qh_t* qh);
void ehci_dump_qtd(const char* name, ehci_qtd_t* qtd);

static inline uint32_t ehci_ptr_to_32(void* ptr) {
    return (uint32_t)(uint64_t)ptr;
}

#endif // EHCI_CORE_H
