#include "double_buffering.h"
#include "../drivers/video.h"
#include "../drivers/proc.h"

// --- VARIÁVEIS EXTERNAS ---
extern volatile int mouse_needs_update; 
extern int refresh_screen;
extern volatile uint64_t system_ticks;

// NOVA VARIÁVEL CONTROLE DE ESTADO OTIMIZADO
volatile int vga_ring0_enabled = 1;

static uint64_t fps_frames = 0;      
static uint64_t current_fps = 0;    
static uint64_t last_fps_time = 0; 

void db_swap_buffers() {
    // 🚀 OTIMIZAÇÃO CRÍTICA: Se o Explorer/Modo Gráfico já estiver ativo,
    // o Ring 0 sai imediatamente sem gastar 1 único ciclo de CPU ou VRAM.
    if (!vga_ring0_enabled) {
        return;
    }

    uint64_t current_time = system_ticks * 10; 
    
    if (current_time - last_fps_time >= 1000) {
        current_fps = fps_frames;
        fps_frames = 0;
        last_fps_time = current_time;
    }

    if (mouse_needs_update || refresh_screen) {
        __asm__ volatile ("cli"); 
        
        mouse_needs_update = 0;
        refresh_screen = 0;
        
        // Só faz o flush pesado se realmente estivermos na fase de boot (modo texto)
        video_flush(); 
        
        __asm__ volatile ("sti");
        fps_frames++;
    }
}

uint64_t db_get_fps() { return current_fps; }
