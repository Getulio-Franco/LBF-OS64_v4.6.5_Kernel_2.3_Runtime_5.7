#include "TOS_IPC.h"
#include "../system/string.h"
#include "../system/graphics.h"
#include "../system/liblib.h"

#ifndef IPC_CTRL_ADDR
#define IPC_CTRL_ADDR (IPC_SHARED_ADDR + (uintptr_t)(sizeof(AppWindowInfo) * MAX_EXTERNAL_APPS))
#endif

int OS_IPC_RegisterApp(const char* title, int width, int height) {
    // 🔒 TRANCA
    ipc_lock(&IPC_CONTROL->lock);

    // 🔥 Começa em 5 para proteger os slots de sistema (0 a 4)
    for (int i = 5; i < MAX_EXTERNAL_APPS; i++) {
        if (IPC_WINDOW_LIST[i].is_active == 0) {
            
            // Define dimensões e posicionamento básico baseado no slot
            int visual_index = i - 5; // Subtrai 5 apenas para o cálculo visual em cascata
            IPC_WINDOW_LIST[i].width = width;
            IPC_WINDOW_LIST[i].height = height;
            IPC_WINDOW_LIST[i].x = 150 + (visual_index * 20); 
            IPC_WINDOW_LIST[i].y = 100 + (visual_index * 20);
            
            // 🌐 ALOCAÇÃO COMPATÍVEL:
            uint64_t base_mem = VRAM_SHARED_BASE + ((uint64_t)i * APP_TOTAL_MEM); 
            
            IPC_WINDOW_LIST[i].buffer_ptr_0 = base_mem;
            IPC_WINDOW_LIST[i].buffer_ptr_1 = base_mem + APP_BUFFER_SIZE;
            IPC_WINDOW_LIST[i].active_buffer = 0;
            
            IPC_WINDOW_LIST[i].local_click_x = 0;
            IPC_WINDOW_LIST[i].local_click_y = 0;
            IPC_WINDOW_LIST[i].has_click_event = 0;

            IPC_WINDOW_LIST[i].pid = sys_get_pid(); 

            strncpy((char*)IPC_WINDOW_LIST[i].title, title, 31);
            IPC_WINDOW_LIST[i].title[31] = '\0';

            IPC_WINDOW_LIST[i].is_active = 1;
            
            graphics_set_slot(i);
            
            // 🔑 LIBERA
            ipc_unlock(&IPC_CONTROL->lock);
            return i; 
        }
    }

    ipc_unlock(&IPC_CONTROL->lock);
    return -1; 
}

void OS_IPC_FlipBuffers(int slot, int width, int height) {
    // 🔥 Proteção de limites ajustada para slots externos (5 a 19)
    if (slot < 5 || slot >= MAX_EXTERNAL_APPS) return;
    if (!IPC_WINDOW_LIST[slot].is_active) return;

    int back_idx = (IPC_WINDOW_LIST[slot].active_buffer == 0) ? 1 : 0;
    
    uint8_t* shared_ptr = (back_idx == 0) 
        ? (uint8_t*)(uintptr_t)IPC_WINDOW_LIST[slot].buffer_ptr_0 
        : (uint8_t*)(uintptr_t)IPC_WINDOW_LIST[slot].buffer_ptr_1;
    
    uint8_t* local_ptr = graphics_get_buffer();
    
    if (shared_ptr && local_ptr) {
        memcpy(shared_ptr, local_ptr, width * height * 4); 
        IPC_WINDOW_LIST[slot].active_buffer = back_idx;
    }
}
