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
#define SUN_SIZE             40     // Sun sprite is 30x30
#define MAX_SUNS             20     // Maximum active suns
#define SUN_SPAWN_INTERVAL   2500   // 25 seconds at 100Hz = 2500 ticks
#define SUN_LIFETIME         800    // 8 seconds before disappearing
#define SUN_VALUE            25     // Sun gives 25 points
#define SUN_GRAVITY          0.12f  // Gravity acceleration
#define SUN_INITIAL_VY       -2.5f  // Initial upward velocity
#define SUN_INITIAL_VX       1.2f   // Initial rightward velocity
#define SUN_LANDING_HEIGHT   380    // Y position where sun lands and stops
#define SUN_ERASE_MARGIN     20      // Extra pixels to erase (prevents artifacts)

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
    int sun_spawn_counter;  // Counter for sun production (only for sunflowers)
} GridCell;

/* Sun object for collection */
typedef struct {
    float x, y;           // Current position (float for smooth physics)
    float vx, vy;         // Velocity
    int prev_x, prev_y;   // Previous integer position (for dirty rect tracking)
    u8 active;            // Is this sun active?
    u8 landed;            // Has sun landed and stopped?
    u16 lifetime;         // Remaining lifetime in ticks
} Sun;

/* Zombie parameters */
#define ZOMBIE_WIDTH         80     // Width of zombie sprite
#define ZOMBIE_HEIGHT        139    // Height of zombie sprite
#define ZOMBIE_SHEET_WIDTH   640    // Sprite sheet width
#define ZOMBIE_SHEET_HEIGHT  834    // Sprite sheet height
#define ZOMBIE_ROWS          6      // Rows in sprite sheet
#define ZOMBIE_COLS          8      // Columns in sprite sheet
#define MAX_ZOMBIES          10     // Maximum active zombies
#define ZOMBIE_SPEED         0.3f   // Zombie movement speed (pixels per tick)
#define ZOMBIE_SPAWN_X       800    // Spawn position X (right edge)
#define ZOMBIE_ANIMATION_FPS 8      // Animation speed
#define ZOMBIE_FRAMES_PER_UPDATE  (TIMER_FREQ_HZ / ZOMBIE_ANIMATION_FPS)
#define ZOMBIE_SPAWN_INTERVAL 1000  // 10 seconds at 100Hz = 1000 ticks
#define ZOMBIE_ERASE_MARGIN  5      // Extra pixels to erase (prevents artifacts)

/* Zombie rendering adjustments */
#define ZOMBIE_Y_OFFSET      -15     // Move zombie down by pixels (adjust visual position)
#define ZOMBIE_SCALE         0.85f  // Scale factor (0.85 = 85% of original size)
#define ZOMBIE_DISPLAY_WIDTH  ((int)(ZOMBIE_WIDTH * ZOMBIE_SCALE))   // Scaled width: 68
#define ZOMBIE_DISPLAY_HEIGHT ((int)(ZOMBIE_HEIGHT * ZOMBIE_SCALE))  // Scaled height: 118

/* Zombie object */
typedef struct {
    float x, y;           // Current position (float for smooth movement)
    int prev_x, prev_y;   // Previous integer position (for dirty rect tracking)
    int row;              // Which lawn row the zombie is in (0-4)
    int animation_frame;  // Current animation frame (0-47, 6x8=48 frames)
    u8 active;            // Is this zombie active?
} Zombie;

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

#endif // PVZ_GAME_H
