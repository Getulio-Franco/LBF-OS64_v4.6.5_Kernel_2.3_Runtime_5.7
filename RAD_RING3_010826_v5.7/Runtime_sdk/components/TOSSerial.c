#include "TOSSerial.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Função auxiliar interna para garantir que temos memória para o componente.
// Nota: Como estamos em Ring 3 e encapsulados pelo seu Runtime, 
// usamos o malloc padrão do espaço de usuário, blindando o Kernel.
TOSSerial* OS_CreateSerial(int port, int baud) {
    TOSSerial* serial = (TOSSerial*)malloc(sizeof(TOSSerial));
    if (!serial) return NULL;

    serial->Handle = -1;
    serial->Active = false;
    serial->BaudRate = baud;
    serial->PortNumber = port;

    return serial;
}

bool OS_Serial_Open(TOSSerial* serial) {
    if (!serial || serial->Active) return false;

    // Dispara a chamada nativa do Runtime passando os parâmetros do objeto
    serial->Handle = sys_serial_open(serial->PortNumber, serial->BaudRate);
    
    // Se o retorno for maior ou igual a zero, a porta foi alocada com sucesso
    if (serial->Handle >= 0) {
        serial->Active = true;
        return true;
    }

    return false;
}

void OS_Serial_Close(TOSSerial* serial) {
    if (!serial || !serial->Active || serial->Handle < 0) return;

    // Fecha o canal usando a chamada nativa
    if (sys_serial_close(serial->Handle) == 0) {
        serial->Active = false;
        serial->Handle = -1;
    }
}

int OS_Serial_Write(TOSSerial* serial, const char* data) {
    if (!serial || !serial->Active || serial->Handle < 0 || !data) return -1;

    int len = strlen(data);
    if (len == 0) return 0;

    // Encaminha a escrita de string direta para o driver através do runtime
    return sys_serial_write(serial->Handle, (char*)data, len);
}

int OS_Serial_Read(TOSSerial* serial, uint8_t* buffer, int max_len) {
    if (!serial || !serial->Active || serial->Handle < 0 || !buffer) return -1;

    // Realiza a leitura física do buffer de hardware (ex: captura do Arduino)
    return sys_serial_read(serial->Handle, buffer, max_len);
}

void OS_Serial_SetBaud(TOSSerial* serial, int new_baud) {
    if (!serial) return;

    // Atualiza a propriedade interna do objeto
    serial->BaudRate = new_baud;

    // Hot-swap: se a porta já estiver aberta, nós a reiniciamos 
    // no novo Baud Rate de forma transparente, igualzinho ao código original!
    if (serial->Active && serial->Handle >= 0) {
        sys_serial_close(serial->Handle);
        serial->Handle = sys_serial_open(serial->PortNumber, serial->BaudRate);
    }
}
