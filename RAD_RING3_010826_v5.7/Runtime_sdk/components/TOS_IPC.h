#ifndef TOS_IPC_H
#define TOS_IPC_H

#include "../system/liblib.h"
#include "../gui/wm.h" // A única fonte da verdade para o IPC

// --- MAPEAMENTO DA VRAM COMPARTILHADA EXPANDIDA ---
#define VRAM_SHARED_BASE    0x0A000000  // Endereço virtual inicial global
#define APP_TOTAL_MEM       0x1000000   // 16 MB totais reservados por slot de aplicativo
#define APP_BUFFER_SIZE     0x800000    // 8 MB para cada Buffer (Suporta até 1080p @ 32bpp)

// Assinaturas das funções que o software vai usar
int  OS_IPC_RegisterApp(const char* title, int width, int height);
void OS_IPC_FlipBuffers(int slot, int width, int height);

#endif
