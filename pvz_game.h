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

/* UI position parameters */
#define SEEDBANK_X     220
#define SEEDBANK_Y     5
#define SUNBANK_X      10
#define SUNBANK_Y      10

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
} GridCell;

/* Game state */
typedef struct {
    int sun_count;
    SeedCard cards[NUM_CARDS];
    GridCell grid[GRID_ROWS][GRID_COLS];
    int selected_card;
    int animation_counter;
    int prev_sun_count;         // Track changes for UI optimization
    int prev_selected_card;     // Track changes for UI optimization
} GameState;

/* Function declarations */
void game_init(GameState *game);
void game_draw_full(GameState *game, u8 *framebuf);        // Full redraw (initial/planting)
void game_draw_animation(GameState *game, u8 *framebuf);  // Animation only (optimized)
void game_handle_touch(GameState *game, int x, int y);
int game_update_animation(GameState *game);
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

#endif // PVZ_GAME_H
