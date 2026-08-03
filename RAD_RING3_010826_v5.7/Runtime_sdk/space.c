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
 * 👾 SPACE INVADERS V4 - PROGRESSÃO DINÂMICA DE FASES (V1 -> V2 -> V3)
 * ============================================================================ */
#define OFFSET_X 25
#define OFFSET_Y 60  // Ajustado para cima, padronizado com o Snake
#define GAME_W 500
#define GAME_H 320

#define ALIEN_ROWS 3
#define ALIEN_COLS 8
#define ALIEN_W 20
#define ALIEN_H 15

#define MAX_ALIEN_LASERS 5
#define NUM_BARRIERS 4
#define BARRIER_BLOCKS_X 5
#define BARRIER_BLOCKS_Y 3
#define BLOCK_SIZE 6

typedef struct {
    int x, y;
    int w, h;
    int alive;
} Alien;

typedef struct {
    int x, y;
    int active;
} Laser;

typedef struct {
    int x, y;
    int hp; // Vida do bloco da barreira (3 = intacto, 0 = destruído)
} BarrierBlock;

// Estado Global do Jogo
static int player_x = OFFSET_X + 230;
static int player_y = OFFSET_Y + 290;
static int player_w = 30;
static int player_h = 12;
static int player_lives = 3;
static int score = 0;
static int fase = 1;

static Laser player_laser = {0, 0, 0};
static Laser alien_lasers[MAX_ALIEN_LASERS];
static Alien aliens[ALIEN_ROWS][ALIEN_COLS];
static BarrierBlock barriers[NUM_BARRIERS][BARRIER_BLOCKS_Y][BARRIER_BLOCKS_X];

static int alien_dir = 1; // 1 = Direita, -1 = Esquerda
static int alien_move_timer = 0;
static int alien_move_speed = 15; // Menor = Mais rápido
static int game_over = 0;
static int game_won = 0;

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
 * FUNÇÕES DE INICIALIZAÇÃO E RESETS DO JOGO
 * ============================================================================ */
void Init_Barriers(void) {
    if (fase == 1) {
        for (int b = 0; b < NUM_BARRIERS; b++)
            for (int r = 0; r < BARRIER_BLOCKS_Y; r++)
                for (int c = 0; c < BARRIER_BLOCKS_X; c++)
                    barriers[b][r][c].hp = 0;
        return;
    }

    int spacing = (GAME_W - (NUM_BARRIERS * BARRIER_BLOCKS_X * BLOCK_SIZE)) / (NUM_BARRIERS + 1);
    for (int b = 0; b < NUM_BARRIERS; b++) {
        int start_x = OFFSET_X + spacing + b * (BARRIER_BLOCKS_X * BLOCK_SIZE + spacing);
        int start_y = OFFSET_Y + 220;
        for (int r = 0; r < BARRIER_BLOCKS_Y; r++) {
            for (int c = 0; c < BARRIER_BLOCKS_X; c++) {
                if (r == BARRIER_BLOCKS_Y - 1 && (c == 1 || c == 2 || c == 3)) {
                    barriers[b][r][c].hp = 0; 
                } else {
                    barriers[b][r][c].x = start_x + c * BLOCK_SIZE;
                    barriers[b][r][c].y = start_y + r * BLOCK_SIZE;
                    barriers[b][r][c].hp = 3;
                }
            }
        }
    }
}

void Init_Aliens(void) {
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            aliens[r][c].x = OFFSET_X + 20 + c * (ALIEN_W + 15);
            aliens[r][c].y = OFFSET_Y + 20 + r * (ALIEN_H + 15);
            aliens[r][c].w = ALIEN_W;
            aliens[r][c].h = ALIEN_H;
            aliens[r][c].alive = 1;
        }
    }
    
    alien_move_speed = 15 - (fase * 2);
    if (alien_move_speed < 3) alien_move_speed = 3; 
    
    alien_dir = 1;
    player_laser.active = 0;
    for (int i = 0; i < MAX_ALIEN_LASERS; i++) alien_lasers[i].active = 0;
}

void Reset_Full_Game(void) {
    player_lives = 3;
    score = 0;
    fase = 1;
    game_over = 0;
    game_won = 0;
    player_x = OFFSET_X + 230;
    Init_Barriers();
    Init_Aliens();
}

void Next_Fase(void) {
    fase++;
    game_won = 0;
    player_x = OFFSET_X + 230;
    Init_Barriers(); 
    Init_Aliens();
}

/* ============================================================================
 * LÓGICA DE ATUALIZAÇÃO E COLISÕES
 * ============================================================================ */
void Update_Game(void) {
    if (game_over || game_won) return;

    // 1. Atualizar Laser do Jogador
    if (player_laser.active) {
        player_laser.y -= 8;
        if (player_laser.y < OFFSET_Y) player_laser.active = 0;

        // Colisão Laser Jogador vs Barreiras
        if (player_laser.active && fase > 1) {
            for (int b = 0; b < NUM_BARRIERS; b++) {
                for (int r = 0; r < BARRIER_BLOCKS_Y; r++) {
                    for (int c = 0; c < BARRIER_BLOCKS_X; c++) {
                        BarrierBlock* block = &barriers[b][r][c];
                        if (block->hp > 0 &&
                            player_laser.x >= block->x && player_laser.x <= block->x + BLOCK_SIZE &&
                            player_laser.y >= block->y && player_laser.y <= block->y + BLOCK_SIZE) {
                            block->hp--;
                            player_laser.active = 0;
                            break;
                        }
                    }
                    if (!player_laser.active) break;
                }
                if (!player_laser.active) break;
            }
        }

        // Colisão Laser Jogador vs Aliens
        if (player_laser.active) {
            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    Alien* a = &aliens[r][c];
                    if (a->alive &&
                        player_laser.x >= a->x && player_laser.x <= a->x + a->w &&
                        player_laser.y >= a->y && player_laser.y <= a->y + a->h) {
                        a->alive = 0;
                        player_laser.active = 0;
                        score += 100 * fase;

                        int remaining = 0;
                        for (int xr = 0; xr < ALIEN_ROWS; xr++)
                            for (int xc = 0; xc < ALIEN_COLS; xc++)
                                if (aliens[xr][xc].alive) remaining++;
                        
                        if (remaining == 0) game_won = 1;
                        break;
                    }
                }
                if (!player_laser.active) break;
            }
        }
    }

    // 2. Movimentação dos Aliens
    alien_move_timer++;
    if (alien_move_timer >= alien_move_speed) {
        alien_move_timer = 0;

        int touch_edge = 0;
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                if (aliens[r][c].alive) {
                    if ((alien_dir == 1 && aliens[r][c].x + ALIEN_W >= OFFSET_X + GAME_W - 10) ||
                        (alien_dir == -1 && aliens[r][c].x <= OFFSET_X + 10)) {
                        touch_edge = 1;
                        break;
                    }
                }
            }
            if (touch_edge) break;
        }

        if (touch_edge) {
            alien_dir = -alien_dir;
            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    aliens[r][c].y += 10;
                    if (aliens[r][c].alive && aliens[r][c].y + ALIEN_H >= player_y - 10) {
                        game_over = 1;
                    }
                }
            }
        } else {
            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    aliens[r][c].x += alien_dir * 6;
                }
            }
        }

        // Lógica de Tiro dos Aliens
        int max_allowed_lasers = 0;
        if (fase == 2) max_allowed_lasers = 1;             
        else if (fase >= 3) max_allowed_lasers = MAX_ALIEN_LASERS; 
        
        if (max_allowed_lasers > 0) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
                    if (aliens[r][c].alive) {
                        if ((get_system_ticks() % 100) < (10 + fase * 2)) {  
                            int current_active = 0;
                            for (int i = 0; i < MAX_ALIEN_LASERS; i++) {
                                if (alien_lasers[i].active) current_active++;
                            }

                            if (current_active < max_allowed_lasers) {
                                for (int i = 0; i < MAX_ALIEN_LASERS; i++) {
                                    if (!alien_lasers[i].active) {
                                        alien_lasers[i].x = aliens[r][c].x + ALIEN_W / 2;
                                        alien_lasers[i].y = aliens[r][c].y + ALIEN_H;
                                        alien_lasers[i].active = 1;
                                        break;
                                    }
                                }
                            }
                        }
                        break; 
                    }
                }
            }
        }
    }

    // 3. Atualizar Lasers dos Aliens
    for (int i = 0; i < MAX_ALIEN_LASERS; i++) {
        if (alien_lasers[i].active) {
            alien_lasers[i].y += 5;
            if (alien_lasers[i].y > OFFSET_Y + GAME_H) alien_lasers[i].active = 0;

            // Colisão com Barreiras
            if (alien_lasers[i].active && fase > 1) {
                for (int b = 0; b < NUM_BARRIERS; b++) {
                    for (int r = 0; r < BARRIER_BLOCKS_Y; r++) {
                        for (int c = 0; c < BARRIER_BLOCKS_X; c++) {
                            BarrierBlock* block = &barriers[b][r][c];
                            if (block->hp > 0 &&
                                alien_lasers[i].x >= block->x && alien_lasers[i].x <= block->x + BLOCK_SIZE &&
                                alien_lasers[i].y >= block->y && alien_lasers[i].y <= block->y + BLOCK_SIZE) {
                                block->hp--;
                                alien_lasers[i].active = 0;
                                break;
                            }
                        }
                        if (!alien_lasers[i].active) break;
                    }
                    if (!alien_lasers[i].active) break;
                }
            }

            // Colisão com Jogador
            if (alien_lasers[i].active) {
                if (alien_lasers[i].x >= player_x && alien_lasers[i].x <= player_x + player_w &&
                    alien_lasers[i].y >= player_y && alien_lasers[i].y <= player_y + player_h) {
                    alien_lasers[i].active = 0;
                    player_lives--;
                    if (player_lives <= 0) game_over = 1;
                }
            }
        }
    }
}

/* ============================================================================
 * RENDERING GRÁFICO
 * ============================================================================ */
void Render_Game(void) {
    uint32_t* buf = (uint32_t*)graphics_get_buffer();
    if (!buf) return;

    graphics_fill_rect(OFFSET_X, OFFSET_Y, GAME_W, GAME_H, 0x000001); 
    graphics_draw_rect(OFFSET_X - 1, OFFSET_Y - 1, GAME_W + 2, GAME_H + 2, 0x222222);

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

    // Jogador
    graphics_fill_rect(player_x, player_y, player_w, player_h, 0x00FF00);
    graphics_fill_rect(player_x + player_w / 2 - 2, player_y - 4, 4, 4, 0x00FF00);

    // Laser Jogador
    if (player_laser.active) {
        graphics_fill_rect(player_laser.x - 1, player_laser.y, 3, 8, 0xFFFF00);
    }

    // Aliens
    for (int r = 0; r < ALIEN_ROWS; r++) {
        uint32_t color = (r == 0) ? 0xFF0055 : (r == 1) ? 0xFFAA00 : 0x00CCFF;
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (aliens[r][c].alive) {
                graphics_fill_rect(aliens[r][c].x, aliens[r][c].y, ALIEN_W, ALIEN_H, color);
                graphics_fill_rect(aliens[r][c].x + 4, aliens[r][c].y + 4, 3, 3, 0x000001);
                graphics_fill_rect(aliens[r][c].x + ALIEN_W - 7, aliens[r][c].y + 4, 3, 3, 0x000001);
            }
        }
    }

    // Lasers Aliens
    for (int i = 0; i < MAX_ALIEN_LASERS; i++) {
        if (alien_lasers[i].active) {
            graphics_fill_rect(alien_lasers[i].x - 1, alien_lasers[i].y, 3, 6, 0xFF0000);
        }
    }

    // Barreiras
    for (int b = 0; b < NUM_BARRIERS; b++) {
        for (int r = 0; r < BARRIER_BLOCKS_Y; r++) {
            for (int c = 0; c < BARRIER_BLOCKS_X; c++) {
                BarrierBlock* block = &barriers[b][r][c];
                if (block->hp > 0) {
                    uint32_t b_color = (block->hp == 3) ? 0x00FFFF : (block->hp == 2) ? 0x00AAAA : 0x005555;
                    graphics_fill_rect(block->x, block->y, BLOCK_SIZE, BLOCK_SIZE, b_color);
                }
            }
        }
    }

    // Overlays
    if (game_over) {
        graphics_fill_rect(OFFSET_X + 100, OFFSET_Y + 120, 300, 80, 0x440000); 
        sys_draw_string(OFFSET_X + 170, OFFSET_Y + 140, "GAME OVER!", 0xFFFFFF, 1);
        sys_draw_string(OFFSET_X + 120, OFFSET_Y + 170, "Pressione 'R' para Reiniciar", 0xFFFF00, 1);
    } else if (game_won) {
        graphics_fill_rect(OFFSET_X + 100, OFFSET_Y + 120, 300, 80, 0x004400); 
        sys_draw_string(OFFSET_X + 150, OFFSET_Y + 140, "FASE CONCLUIDA!", 0xFFFFFF, 1);
        sys_draw_string(OFFSET_X + 115, OFFSET_Y + 170, "Pressione 'ESPACO' p/ proxima", 0xFFFF00, 1);
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

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_VISIBLE, 0);
    }

    uint32_t* b0 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_0;
    uint32_t* b1 = (uint32_t*)(uintptr_t)IPC_WINDOW_LIST[my_app_slot].buffer_ptr_1;

    if (b0) memset(b0, 0, winWidth * winHeight * 4);
    if (b1) memset(b1, 0, winWidth * winHeight * 4);

    IPC_WINDOW_LIST[my_app_slot].is_active = 0;
    sys_sleep(50); 
}

/* ============================================================================
 * MAIN (LOOP PRINCIPAL)
 * ============================================================================ */
int main(int argc, char* argv[]) {
    static bool primeiro_desenho = true;
    static bool ultimo_estado_foco = false;

    graphics_init_app(winWidth, winHeight);
    wm_init();

    my_app_slot = OS_IPC_RegisterApp("Space Invaders LBF", winWidth, winHeight);
    if (my_app_slot == -1) return -1;

    graphics_set_slot(my_app_slot);
    GUI_InitApplication(&MyApp, my_app_slot, "Space Invaders V4", winWidth, winHeight);

    if (MyApp.MainWindow) {
        gui_set_prop(MyApp.MainWindow, PROP_COLOR, 0x000000); 
    }

    Reset_Full_Game();
    Flush_Grafico_Janela();

    while (1) {
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
            char key = Obter_Tecla_Entrada();
            if (key != 0) {
                if (key == 'a' || key == 'A' || key == '4') {
                    if (player_x > OFFSET_X + 5) player_x -= 12;
                } else if (key == 'd' || key == 'D' || key == '6') {
                    if (player_x + player_w < OFFSET_X + GAME_W - 5) player_x += 12;
                } else if (key == ' ' || key == 'w' || key == 'W' || key == '5') {
                    if (game_won) {
                        Next_Fase();
                    } else if (game_over) {
                        Reset_Full_Game();
                    } else if (!player_laser.active) {
                        player_laser.x = player_x + player_w / 2;
                        player_laser.y = player_y - 4;
                        player_laser.active = 1;
                    }
                } else if (key == 'r' || key == 'R') {
                    Reset_Full_Game();
                }
                precisa_redesenhar = true;
            }

            // O jogo só atualiza se estiver com o foco ativo!
            Update_Game();
            precisa_redesenhar = true;
        }

        if (precisa_redesenhar) {
            Flush_Grafico_Janela();
        }
        
        sys_sleep(16);
    }

    sys_exit();
    return 0;
}
