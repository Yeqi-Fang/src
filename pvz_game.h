/* ------------------------------------------------------------ */
/*                  PVZ Game Header (Optimized)                 */
/* ------------------------------------------------------------ */
#ifndef PVZ_GAME_H
#define PVZ_GAME_H

#include "xil_types.h"

/* Screen parameters */
#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  480

/* Game grid parameters */
#define GRID_ROWS      5
#define GRID_COLS      9
#define GRID_START_X   146
#define GRID_START_Y   63
#define GRID_WIDTH     65
#define GRID_HEIGHT    79

/* Plant display size */
#define PLANT_SIZE     58

/* Animation parameters */
#define ANIMATION_FRAMES     25
#define ANIMATION_FPS        12
#define TIMER_FREQ_HZ        100
#define FRAMES_PER_UPDATE    8
#define SPRITE_SHEET_SIZE    400
#define FRAME_SIZE           80
#define SPRITE_COLS          5
#define SPRITE_ROWS          5

/* Sun parameters */
#define SUN_SIZE             40
#define MAX_SUNS             20
#define SUN_SPAWN_INTERVAL   2500
#define SUN_LIFETIME         800
#define SUN_VALUE            25
#define SUN_GRAVITY          0.12f
#define SUN_INITIAL_VY       -2.5f
#define SUN_INITIAL_VX       1.2f
#define SUN_LANDING_HEIGHT   380
#define SUN_ERASE_MARGIN     20

/* UI position parameters */
#define SEEDBANK_X     220
#define SEEDBANK_Y     5
#define SUNBANK_X      10
#define SUNBANK_Y      10

/* UI protection areas - DO NOT ERASE */
#define UI_SEEDBANK_X      220
#define UI_SEEDBANK_Y      5
#define UI_SEEDBANK_WIDTH  (NUM_CARDS * (CARD_WIDTH + CARD_SPACING) + 20)
#define UI_SEEDBANK_HEIGHT (CARD_HEIGHT + 10)

#define UI_SUNBANK_X       10
#define UI_SUNBANK_Y       10
#define UI_SUNBANK_WIDTH   100
#define UI_SUNBANK_HEIGHT  50

/* UI region sizes (for redraw protection) */
#define SEEDBANK_WIDTH   240
#define SEEDBANK_HEIGHT  70
#define SUNBANK_WIDTH    80
#define SUNBANK_HEIGHT   35

/* Card parameters */
#define CARD_WIDTH     45
#define CARD_HEIGHT    63
#define CARD_SPACING   2
#define NUM_CARDS      2

/* Plant icon scaling */
#define PLANT_ICON_SIZE  35

/* Plant types */
typedef enum {
    PLANT_NONE = 0,
    PLANT_SUNFLOWER = 1,
    PLANT_PEASHOOTER = 2
} PlantType;

/* Plant costs */
#define SUNFLOWER_COST   50
#define PEASHOOTER_COST  100

/* Card status */
typedef struct {
    PlantType type;
    int cost;
    u8 selected;
} SeedCard;

/* Grid cell */
typedef struct {
    PlantType plant;
    int animation_frame;
    int sun_spawn_counter;
    int shoot_counter;
} GridCell;

/* Sun object for collection */
typedef struct {
    float x, y;
    float vx, vy;
    int prev_x, prev_y;
    u8 active;
    u8 landed;
    u16 lifetime;
} Sun;

/* Zombie parameters */
#define ZOMBIE_WIDTH         80
#define ZOMBIE_HEIGHT        139
#define ZOMBIE_SHEET_WIDTH   640
#define ZOMBIE_SHEET_HEIGHT  834
#define ZOMBIE_ROWS          6
#define ZOMBIE_COLS          8
#define MAX_ZOMBIES          10
#define ZOMBIE_SPEED         0.2f
#define ZOMBIE_SPAWN_X       800
#define ZOMBIE_ANIMATION_FPS 8
#define ZOMBIE_FRAMES_PER_UPDATE  (TIMER_FREQ_HZ / ZOMBIE_ANIMATION_FPS)
#define ZOMBIE_SPAWN_INTERVAL 1000
#define ZOMBIE_ERASE_MARGIN  5

/* Zombie rendering adjustments */
#define ZOMBIE_Y_OFFSET      -15
#define ZOMBIE_SCALE         0.8f
#define ZOMBIE_DISPLAY_WIDTH  ((int)(ZOMBIE_WIDTH * ZOMBIE_SCALE))
#define ZOMBIE_DISPLAY_HEIGHT ((int)(ZOMBIE_HEIGHT * ZOMBIE_SCALE))

/* Bite animation parameters */
#define BITE_FRAME_WIDTH     80
#define BITE_FRAME_HEIGHT    139
#define BITE_SHEET_WIDTH     640
#define BITE_SHEET_HEIGHT    695
#define BITE_ROWS            5
#define BITE_COLS            8
#define BITE_ANIMATION_FRAMES 40
#define BITE_ANIMATION_FPS   12
#define BITE_FRAMES_PER_UPDATE (TIMER_FREQ_HZ / BITE_ANIMATION_FPS)
#define BITE_DURATION        300

/* Zombie state */
typedef enum {
    ZOMBIE_WALKING = 0,
    ZOMBIE_BITING = 1
} ZombieState;

/* Zombie object */
typedef struct {
    float x, y;
    int prev_x, prev_y;
    int row;
    int animation_frame;
    u8 active;
    int health;
    ZombieState state;
    int target_col;
    int bite_timer;
    int bite_anim_frame;
} Zombie;

/* Pea projectile parameters */
#define PEA_SIZE             24
#define MAX_PEAS             50
#define PEA_SPEED            3.0f
#define PEA_DAMAGE           1
#define PEA_SHOOT_INTERVAL   145
#define PEA_ERASE_MARGIN     12
#define ZOMBIE_MAX_HEALTH    10

/* Pea projectile object */
typedef struct {
    float x, y;
    int prev_x, prev_y;
    int row;
    u8 active;
} Pea;

/* Game play state enum */
typedef enum {
    GAME_PLAYING = 0,
    GAME_FADING_TO_BLACK = 1,
    GAME_SHOWING_DEFEAT = 2,
    GAME_RESTARTING = 3
} GamePlayState;

/* Game over animation parameters */
#define FADE_TO_BLACK_DURATION  200
#define DEFEAT_SCALE_DURATION   150
#define DEFEAT_IMAGE_WIDTH      800
#define DEFEAT_IMAGE_HEIGHT     480
#define DEFEAT_MIN_SCALE        0.02f
#define DEFEAT_MAX_SCALE        1.0f

/* Game state */
typedef struct {
    int sun_count;
    SeedCard cards[NUM_CARDS];
    GridCell grid[GRID_ROWS][GRID_COLS];
    int selected_card;
    int animation_counter;
    int prev_sun_count;
    int prev_selected_card;
    Sun suns[MAX_SUNS];
    int num_active_suns;
    Zombie zombies[MAX_ZOMBIES];
    int num_active_zombies;
    int zombie_spawn_counter;
    int zombie_animation_counter;
    Pea peas[MAX_PEAS];
    int num_active_peas;
    int bite_animation_counter;

    /* Game over state */
    GamePlayState play_state;
    int game_over_timer;
    float fade_progress;
    float defeat_scale;
} GameState;

/* Function declarations */
void game_init(GameState *game);
void game_draw_full(GameState *game, u8 *framebuf);
void game_draw_animation(GameState *game, u8 *framebuf);
void game_draw_suns(GameState *game, u8 *framebuf);
void game_handle_touch(GameState *game, int x, int y);
int game_update_animation(GameState *game);
void game_update_suns(GameState *game);
void game_spawn_sun(GameState *game, int source_x, int source_y);
int game_check_sun_click(GameState *game, int x, int y);
void draw_single_plant_cell(GameState *game, u8 *framebuf, int row, int col);
void draw_sprite_scaled(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                        const u8 *sprite_data, int src_w, int src_h);
void draw_sprite_scaled_transparent(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                                    const u8 *sprite_data, int src_w, int src_h);
void draw_sprite_from_sheet(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                            const u8 *sheet_data, int frame_index);
void draw_sprite(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h);
void draw_sprite_transparent(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h);
void draw_number(u8 *framebuf, int x, int y, int number);
void restore_background_rect(u8 *framebuf, int x, int y, int w, int h);
void draw_sprite_darkened(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h);

/* Helper functions */
int is_in_ui_protected_area(int x, int y, int w, int h);
void restore_background_rect_safe(u8 *framebuf, int x, int y, int w, int h);
void redraw_ui_if_overlapped(GameState *game, u8 *framebuf, int erase_x, int erase_y, int erase_w, int erase_h);

/* UI redraw functions */
void redraw_ui_if_overlapped(GameState *game, u8 *framebuf, int erase_x, int erase_y, int erase_w, int erase_h);

/* Zombie functions */
void game_spawn_zombie(GameState *game);
void game_update_zombies(GameState *game);
void game_draw_zombies(GameState *game, u8 *framebuf);
void draw_zombie_sprite(u8 *framebuf, int dst_x, int dst_y, const u8 *sheet_data, int frame_index);
void draw_bite_sprite(u8 *framebuf, int dst_x, int dst_y, const u8 *sheet_data, int frame_index);

/* Pea functions */
void game_shoot_pea(GameState *game, int row, int col);
void game_update_peas(GameState *game);
void game_draw_peas(GameState *game, u8 *framebuf);
void game_check_pea_zombie_collision(GameState *game);

/* Game over functions */
int game_check_defeat(GameState *game);
void game_trigger_defeat(GameState *game);
void game_update_gameover(GameState *game);
void game_draw_fade_to_black(u8 *framebuf, float progress);
void game_fill_black(u8 *framebuf);  /* BUG FIX: Added missing function declaration */
void game_draw_defeat_image(u8 *framebuf, float scale);
void game_reset(GameState *game);

#endif // PVZ_GAME_H
