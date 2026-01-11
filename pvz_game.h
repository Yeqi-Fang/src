/* ------------------------------------------------------------ */
/*                  PVZ Game Header                             */
/* ------------------------------------------------------------ */
#ifndef PVZ_GAME_H
#define PVZ_GAME_H

#include "xil_types.h"

/* Screen parameters */
#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  480

/* Game grid parameters - adjusted based on actual measurements */
#define GRID_ROWS      5
#define GRID_COLS      9
#define GRID_START_X   146
#define GRID_START_Y   63
#define GRID_WIDTH     65    // (727-146)/9 = 64.56, rounded to 65
#define GRID_HEIGHT    79    // (456-63)/5 = 78.6, rounded to 79

/* Plant display size - fit to grid, centered */
#define PLANT_SIZE     58    // Leaves margin for centering in 65x79 grid

/* Animation parameters */
#define ANIMATION_FRAMES     25    // Total frames in sprite sheet
#define ANIMATION_FPS        12    // Frames per second
#define TIMER_FREQ_HZ        100   // Timer interrupt frequency (10ms)
#define FRAMES_PER_UPDATE    8     // Update animation every 8 timer ticks (100/12 ¡Ö 8.33)
#define SPRITE_SHEET_SIZE    400   // Sprite sheet dimension (400x400)
#define FRAME_SIZE           80    // Each frame size (80x80)
#define SPRITE_COLS          5     // Columns in sprite sheet
#define SPRITE_ROWS          5     // Rows in sprite sheet

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
    u8 selected;     // 0=not selected, 1=selected
} SeedCard;

/* Grid cell */
typedef struct {
    PlantType plant;
    int animation_frame;  // Current animation frame (0-24)
} GridCell;

/* Game state */
typedef struct {
    int sun_count;
    SeedCard cards[NUM_CARDS];
    GridCell grid[GRID_ROWS][GRID_COLS];
    int selected_card;  // -1 means no selection
    int animation_counter;  // Counter for animation timing
} GameState;

/* Function declarations */
void game_init(GameState *game);
void game_draw(GameState *game, u8 *framebuf);
void game_handle_touch(GameState *game, int x, int y);
int game_update_animation(GameState *game);  // Returns 1 if animation frame changed
void draw_sprite_scaled(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                        const u8 *sprite_data, int src_w, int src_h);
void draw_sprite_scaled_transparent(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                                    const u8 *sprite_data, int src_w, int src_h);
void draw_sprite_from_sheet(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                            const u8 *sheet_data, int frame_index);  // Extract and draw frame from sprite sheet
void draw_sprite(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h);
void draw_sprite_transparent(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h);
void draw_number(u8 *framebuf, int x, int y, int number);

#endif // PVZ_GAME_H
