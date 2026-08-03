#include "sdk/libgui.h"
#include "../system/graphics.h"
#include "../gui/wm.h"
#include "../system/string.h"
#include "../system/liblib.h"

// Componentes do Sistema encapsulados
#include "components/TOS_IPC.h"     
#include "components/TOSSerial.h"   

// Protótipos obrigatórios
void gui_draw_form(TForm* form);
void gui_render_form(TForm* form);
extern void events_process_mouse(int x, int y, int pressed, int button);
extern void* g_focused_control;
extern void GUI_Memo_AddStr(TGUIControl* memo, const char* str);
extern void GUI_Memo_Clear(TGUIControl* memo);

// Variáveis da Janela
int my_app_slot = -1;
TGUIEnvironment MyApp;
const int winWidth = 550;
const int winHeight = 410;

/* ============================================================================
 * 🛡️ ESTRUTURA AUXILIAR IPC (Mapeamento de Eventos Estendidos de Teclado)
 * ============================================================================ */
typedef struct {
    uint8_t dummy[sizeof(IPC_WINDOW_LIST[0])]; 
    volatile uint8_t fila_teclado_virtual;
    volatile uint8_t tem_evento_teclado;
} __attribute__((packed)) AppWindowInfoExtended;

char Obter_Tecla_Entrada(void) {
    if (my_app_slot < 0) return 0;
    AppWindowInfoExtended* ext_slot = (AppWindowInfoExtended*)&IPC_WINDOW_LIST[my_app_slot];

    if (ext_slot->tem_evento_teclado == 1) {
        char key = (char)ext_slot->fila_teclado_virtual;
        ext_slot->tem_evento_teclado = 0; 
        return key;
    }
    return 0;
}

/* ============================================================================
 * 🐍 LÓGICA DO JOGO DA COBRINHA - COM FASES (V1 -> V2 -> V3)
 * ============================================================================ */
#define OFFSET_X 25
#define OFFSET_Y 60  
#define TILE_SIZE 20
#define GRID_W 25
#define GRID_H 16
#define GAME_W (GRID_W * TILE_SIZE)
#define GAME_H (GRID_H * TILE_SIZE)

// Estado da Cobrinha e Mapa
static int snake_x[100], snake_y[100];
static int snake_len = 4;
static int dir_x = 1, dir_y = 0;
static bool dir_changed = false; 
static int food_x = 10, food_y = 10;

// Obstáculos (Fase 3)
static int obs_x[2], obs_y[2];
static int num_obs = 0;

// Estado Global do Jogo
static int player_lives = 3;
static int score = 0;
static int fase = 1;
static int foods_collected = 0;
static int game_over = 0;
static int game_won = 0;
static int speed_threshold = 110; 

static uint32_t rng_seed = 12345; 
int lcg_rand() {
    rng_seed = (1103515245 * rng_seed + 12345) % 2147483648;
    return rng_seed;
}

bool Is_Occupied(int x, int y) {
    for (int i = 0; i < snake_len; i++) {
        if (snake_x[i] == x && snake_y[i] == y) return true;
    }
    return false;
}

void Spawn_Items(void) {
    do {
        food_x = lcg_rand() % GRID_W;
        food_y = lcg_rand() % GRID_H;
    } while (Is_Occupied(food_x, food_y));

    if (fase >= 3) {
        num_obs = 2;
        for (int i = 0; i < num_obs; i++) {
            do {
                obs_x[i] = lcg_rand() % GRID_W;
                obs_y[i] = lcg_rand() % GRID_H;
            } while (Is_Occupied(obs_x[i], obs_y[i]) || 
                     (obs_x[i] == food_x && obs_y[i] == food_y) || 
                     (i == 1 && obs_x[0] == obs_x[1] && obs_y[0] == obs_y[1]));
        }
    } else {
        num_obs = 0;
    }
}

void Init_Snake(void) {
    snake_len = 4;
    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = 12 - i;
        snake_y[i] = 8;
    }
    dir_x = 1; dir_y = 0;
    dir_changed = false;
    Spawn_Items();
}

void Reset_Full_Game(void) {
    player_lives = 3;
    score = 0;
    fase = 1;
    foods_collected = 0;
    game_over = 0;
    game_won = 0;
    speed_threshold = 110; 
    Init_Snake();
}

void Next_Fase(void) {
    if (fase < 3) fase++;
    foods_collected = 0;
    game_won = 0;
    speed_threshold = (fase >= 2) ? 95 : 110; 
    Init_Snake();
}

void Update_Game(void) {
    if (game_over || game_won) return;

    for (int i = snake_len - 1; i > 0; i--) {
        snake_x[i] = snake_x[i-1];
        snake_y[i] = snake_y[i-1];
    }
    snake_x[0] += dir_x;
    snake_y[0] += dir_y;
    dir_changed = false;

    bool died = false;
    
    if (snake_x[0] < 0 || snake_x[0] >= GRID_W || snake_y[0] < 0 || snake_y[0] >= GRID_H) died = true;
    
    for (int i = 1; i < snake_len; i++) {
        if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) died = true;
    }

    for (int i = 0; i < num_obs; i++) {
        if (snake_x[0] == obs_x[i] && snake_y[0] == obs_y[i]) died = true;
    }

    if (died) {
        player_lives--;
        if (player_lives <= 0) {
            game_over = 1;
        } else {
            Init_Snake(); 
        }
        return;
    }

    if (snake_x[0] == food_x && snake_y[0] == food_y) {
        if (snake_len < 100) snake_len++;
        score += 10 * fase;
        foods_collected++;

        if (foods_collected >= 16) {
            if (fase == 1) {
                Next_Fase();
            } else {
                game_won = 1;
            }
        } else {
            Spawn_Items(); 
        }
    }
}

/* ============================================================================
 * RENDERING GRÁFICO
 * ============================================================================ */
void Render_Game(void) {
    uint32_t* buf = (uint32_t*)graphics_get_buffer();
    if (!buf) return;

    // 1. Limpa a área do Jogo com Preto Absoluto Seguro (0x000001)
    graphics_fill_rect(OFFSET_X, OFFSET_Y, GAME_W, GAME_H, 0x000001); 

    // Borda do Jogo
    graphics_draw_rect(OFFSET_X - 1, OFFSET_Y - 1, GAME_W + 2, GAME_H + 2, 0x222222);

    // HUD Superior
    char hud_buf[64];
    itoa(score, hud_buf, 10);
    sys_draw_string(OFFSET_X + 10, OFFSET_Y - 25, "SCORE:", 0xFFFFFF, 1);
    sys_draw_string(OFFSET_X + 70, OFFSET_Y - 25, hud_buf, 0xFFFF00, 1); // Amarelo

    itoa(player_lives, hud_buf, 10);
    sys_draw_string(OFFSET_X + 200, OFFSET_Y - 25, "VIDAS:", 0xFFFFFF, 1);
    sys_draw_string(OFFSET_X + 260, OFFSET_Y - 25, hud_buf, 0xFF0000, 1); // Vermelho

    itoa(fase, hud_buf, 10);
    sys_draw_string(OFFSET_X + 380, OFFSET_Y - 25, "FASE:", 0xFFFFFF, 1);
    sys_draw_string(OFFSET_X + 430, OFFSET_Y - 25, hud_buf, 0x00FFFF, 1); // Ciano

    // Comida (Vermelho Vivo)
    graphics_fill_rect(OFFSET_X + food_x * TILE_SIZE + 1, OFFSET_Y + food_y * TILE_SIZE + 1, TILE_SIZE - 2, TILE_SIZE - 2, 0xFF0000);

    // Obstáculos Fase 3 (Ciano)
    for (int i = 0; i < num_obs; i++) {
        graphics_fill_rect(OFFSET_X + obs_x[i] * TILE_SIZE + 1, OFFSET_Y + obs_y[i] * TILE_SIZE + 1, TILE_SIZE - 2, TILE_SIZE - 2, 0x00FFFF);
    }

    // Cobrinha
    for (int i = 0; i < snake_len; i++) {
        uint32_t color = (i == 0) ? ((game_over) ? 0x888888 : 0x00FF00) : 0x008800; 
        graphics_fill_rect(OFFSET_X + snake_x[i] * TILE_SIZE + 1, OFFSET_Y + snake_y[i] * TILE_SIZE + 1, TILE_SIZE - 2, TILE_SIZE - 2, color);
    }

    // Overlays
    if (game_over) {
        graphics_fill_rect(OFFSET_X + 100, OFFSET_Y + 130, 300, 80, 0x440000); 
        sys_draw_string(OFFSET_X + 170, OFFSET_Y + 150, "GAME OVER!", 0xFFFFFF, 1);
        sys_draw_string(OFFSET_X + 120, OFFSET_Y + 180, "Pressione 'R' para Reiniciar", 0xFFFF00, 1);
    } else if (game_won) {
        graphics_fill_rect(OFFSET_X + 100, OFFSET_Y + 130, 300, 80, 0x004400); 
        sys_draw_string(OFFSET_X + 150, OFFSET_Y + 150, "FASE CONCLUIDA!", 0xFFFFFF, 1);
        sys_draw_string(OFFSET_X + 115, OFFSET_Y + 180, "Pressione 'ESPACO' p/ proxima", 0xFFFF00, 1);
    }
}

/* ============================================================================
 * FLUSH E FECHAMENTO
 * ============================================================================ */
void Flush_Grafico_Janela(void) {
    if (my_app_slot == -1) return;

    gui_draw_form((TForm*)MyApp.MainWindow);
    gui_render_form((TForm*)MyApp.MainWindow);

    Render_Game();

    OS_IPC_FlipBuffers(my_app_slot, winWidth, winHeight);
}

void Tratar_Fechamento_Software(void) {
    if (my_app_slot == -1) return;
    
    if (MyApp.MainWindow) gui_set_prop(MyApp.MainWindow, PROP_VISIBLE, 0);
    uint32_t* b0 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_0;
    uint32_t* b1 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_1;
    
    if (b0) memset(b0, 0, winWidth * winHeight * 4);
    if (b1) memset(b1, 0, winWidth * winHeight * 4);
    
    IPC_WINDOW_LIST[my_app_slot].is_active = 0;
    sys_sleep(50); 
}

/* ============================================================================
 * FUNÇÃO PRINCIPAL (MAIN)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;
    int game_tick_timer = 0;

    graphics_init_app(winWidth, winHeight);
    wm_init();
    
    my_app_slot = OS_IPC_RegisterApp("SnakeGame", winWidth, winHeight);
    if (my_app_slot == -1) return -1; 
    
    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "LBF Snake V2", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000); 
    }
   
    Reset_Full_Game();
    Flush_Grafico_Janela();

    while(1) {
        if (IPC_WINDOW_LIST[my_app_slot].is_active == 0) {
            Tratar_Fechamento_Software();
            break;
        }

        bool precisa_redesenhar = false;

        if (primeiro_desenho) {
            primeiro_desenho = false;
            precisa_redesenhar = true;
        }

        bool euTenhoFoco = (IPC_CONTROL->active_focus_slot == my_app_slot);
        if (euTenhoFoco != ultimo_estado_foco) {
            ultimo_estado_foco = euTenhoFoco;
            if (MyApp.MainWindow) {
                ((TForm*)MyApp.MainWindow)->ActiveFocus = euTenhoFoco;
            }
            precisa_redesenhar = true;
        }

        if (euTenhoFoco) {
            // -- LÓGICA DE TECLADO IPC --
            char key = Obter_Tecla_Entrada();
            if (key != 0) {
                if ((key == 'w' || key == 'W' || key == '8') && dir_y == 0 && !dir_changed) { dir_x = 0; dir_y = -1; dir_changed = true; }
                else if ((key == 's' || key == 'S' || key == '2') && dir_y == 0 && !dir_changed) { dir_x = 0; dir_y = 1; dir_changed = true; }
                else if ((key == 'a' || key == 'A' || key == '4') && dir_x == 0 && !dir_changed) { dir_x = -1; dir_y = 0; dir_changed = true; }
                else if ((key == 'd' || key == 'D' || key == '6') && dir_x == 0 && !dir_changed) { dir_x = 1; dir_y = 0; dir_changed = true; }
                else if (key == ' ' || key == '5') {
                    if (game_won) Next_Fase();
                }
                else if (key == 'r' || key == 'R') {
                    Reset_Full_Game();
                }
                precisa_redesenhar = true; 
            }

            // -- TIMER E ATUALIZAÇÃO DA COBRA --
            if (!game_over && !game_won) {
                game_tick_timer += 16;
                if (game_tick_timer >= speed_threshold) {
                    game_tick_timer = 0;
                    Update_Game();
                    precisa_redesenhar = true;
                }
            }
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(16);
    }

    sys_exit(); 
    return 0;
}
