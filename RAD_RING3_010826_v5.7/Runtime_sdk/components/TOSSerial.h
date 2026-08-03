#ifndef TOS_SERIAL_H
#define TOS_SERIAL_H

#include "../system/liblib.h"

typedef struct {
    int Handle;
    bool Active;
    int BaudRate;
    int PortNumber;
} TOSSerial;

// Interface simplificada estilo objeto
TOSSerial* OS_CreateSerial(int port, int baud);
bool       OS_Serial_Open(TOSSerial* serial);
void       OS_Serial_Close(TOSSerial* serial);
int        OS_Serial_Write(TOSSerial* serial, const char* data);
int        OS_Serial_Read(TOSSerial* serial, uint8_t* buffer, int max_len);
void       OS_Serial_SetBaud(TOSSerial* serial, int new_baud);

#endif
