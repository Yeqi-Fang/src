/* ------------------------------------------------------------ */
/*            PVZ Game Logic (Optimized Version)                */
/* ------------------------------------------------------------ */
#include "pvz_game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Include image header files
extern const unsigned char gImage_background1_hd[];
#include "SeedPacket.h"
#include "SunBank.h"
#include "Sun.h"
#include "SeedBank.h"
#include "PeaShooter.h"
#include "SunFlower.h"
#include "PeaShooter_ani.h"
#include "SunFlower_ani.h"
#include "walk_ani.h"

// Forward declarations
int rects_overlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

/* Number font - simple 7-segment style digits (10x16) */
static const u8 digit_patterns[10][16] = {
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x66,0x06,0x06,0x0C,0x18,0x30,0x60,0x60,0x60,0x60,0x66,0x66,0x7E,0x00}, // 2
    {0x3C,0x66,0x66,0x06,0x06,0x0C,0x1C,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00}, // 3
    {0x0C,0x1C,0x1C,0x2C,0x2C,0x4C,0x4C,0x7E,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00}, // 4
    {0x7E,0x60,0x60,0x60,0x60,0x7C,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00}, // 5
    {0x3C,0x66,0x60,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x06,0x06,0x0C,0x0C,0x18,0x18,0x18,0x18,0x30,0x30,0x30,0x30,0x30,0x00}, // 7
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}  // 9
};

/**
 * Initialize game state
 */
void game_init(GameState *game)
{
    int i, j;
    
    game->sun_count = 150;
    game->selected_card = -1;
    game->animation_counter = 0;
    game->prev_sun_count = 150;
    game->prev_selected_card = -1;
    game->num_active_suns = 0;
    
    // Initialize cards
    game->cards[0].type = PLANT_SUNFLOWER;
    game->cards[0].cost = SUNFLOWER_COST;
    game->cards[0].selected = 0;
    
    game->cards[1].type = PLANT_PEASHOOTER;
    game->cards[1].cost = PEASHOOTER_COST;
    game->cards[1].selected = 0;
    
    // Initialize grid
    for (i = 0; i < GRID_ROWS; i++) {
        for (j = 0; j < GRID_COLS; j++) {
            game->grid[i][j].plant = PLANT_NONE;
            game->grid[i][j].animation_frame = 0;
            game->grid[i][j].sun_spawn_counter = 0;
        }
    }
    
    // Initialize suns
    for (i = 0; i < MAX_SUNS; i++) {
        game->suns[i].active = 0;
        game->suns[i].prev_x = -1;
    }

    // Initialize zombies
    game->num_active_zombies = 0;
    game->zombie_spawn_counter = 0;
    game->zombie_animation_counter = 0;
    for (i = 0; i < MAX_ZOMBIES; i++) {
        game->zombies[i].active = 0;
        game->zombies[i].prev_x = -1;
        game->zombies[i].prev_y = -1;
    }

    printf("Game initialized: sun=%d\n", game->sun_count);
}

/**
 * Draw sprite (original size)
 */
void draw_sprite(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    
    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            sprite_idx = (i * w + j) * 3;
            
            framebuf[fb_idx]     = sprite_data[sprite_idx];
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1];
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2];
        }
    }
}

/**
 * Draw sprite with transparency
 */
void draw_sprite_transparent(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    u8 b, g, r;

    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            sprite_idx = (i * w + j) * 3;

            b = sprite_data[sprite_idx];
            g = sprite_data[sprite_idx + 1];
            r = sprite_data[sprite_idx + 2];

            if (b == 0 && g == 0 && r == 0) {
                continue;
            }

            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            framebuf[fb_idx]     = b;
            framebuf[fb_idx + 1] = g;
            framebuf[fb_idx + 2] = r;
        }
    }
}

/**
 * Draw scaled sprite
 */
void draw_sprite_scaled(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                        const u8 *sprite_data, int src_w, int src_h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    int src_x, src_y;
    
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            src_x = (j * src_w) / dst_w;
            src_y = (i * src_h) / dst_h;
            
            fb_idx = ((dst_y + i) * SCREEN_WIDTH + (dst_x + j)) * 3;
            sprite_idx = (src_y * src_w + src_x) * 3;
            
            framebuf[fb_idx]     = sprite_data[sprite_idx];
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1];
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2];
        }
    }
}

/**
 * Draw scaled sprite with transparency
 */
void draw_sprite_scaled_transparent(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                                    const u8 *sprite_data, int src_w, int src_h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    int src_x, src_y;
    u8 b, g, r;

    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;

    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            src_x = (j * src_w) / dst_w;
            src_y = (i * src_h) / dst_h;

            sprite_idx = (src_y * src_w + src_x) * 3;

            b = sprite_data[sprite_idx];
            g = sprite_data[sprite_idx + 1];
            r = sprite_data[sprite_idx + 2];

            if (b == 0 && g == 0 && r == 0) {
                continue;
            }

            fb_idx = ((dst_y + i) * SCREEN_WIDTH + (dst_x + j)) * 3;
            framebuf[fb_idx]     = b;
            framebuf[fb_idx + 1] = g;
            framebuf[fb_idx + 2] = r;
        }
    }
}

/**
 * Draw number
 */
void draw_number(u8 *framebuf, int x, int y, int number)
{
    char str[16];
    int len, i, digit, bit, row, col;
    u32 fb_idx;
    
    sprintf(str, "%d", number);
    len = strlen(str);
    
    for (i = 0; i < len; i++) {
        digit = str[i] - '0';
        
        for (row = 0; row < 16; row++) {
            for (bit = 0; bit < 8; bit++) {
                if (digit_patterns[digit][row] & (1 << (7 - bit))) {
                    col = x + i * 12 + bit;
                    if (col >= 0 && col < SCREEN_WIDTH && y + row >= 0 && y + row < SCREEN_HEIGHT) {
                        fb_idx = ((y + row) * SCREEN_WIDTH + col) * 3;
                        framebuf[fb_idx]     = 0;
                        framebuf[fb_idx + 1] = 0;
                        framebuf[fb_idx + 2] = 0;
                    }
                }
            }
        }
    }
}

/**
 * Extract and draw frame from sprite sheet with scaling and transparency
 */
void draw_sprite_from_sheet(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                            const u8 *sheet_data, int frame_index)
{
    int i, j;
    u32 fb_idx;
    int src_x_offset, src_y_offset;
    int src_x, src_y;
    u32 sheet_idx;
    u8 b, g, r;

    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;

    int frame_row = frame_index / SPRITE_COLS;
    int frame_col = frame_index % SPRITE_COLS;

    src_x_offset = frame_col * FRAME_SIZE;
    src_y_offset = frame_row * FRAME_SIZE;

    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            src_x = src_x_offset + (j * FRAME_SIZE) / dst_w;
            src_y = src_y_offset + (i * FRAME_SIZE) / dst_h;

            sheet_idx = (src_y * SPRITE_SHEET_SIZE + src_x) * 3;

            b = sheet_data[sheet_idx];
            g = sheet_data[sheet_idx + 1];
            r = sheet_data[sheet_idx + 2];

            if (b == 0 && g == 0 && r == 0) {
                continue;
            }

            fb_idx = ((dst_y + i) * SCREEN_WIDTH + (dst_x + j)) * 3;
            framebuf[fb_idx]     = b;
            framebuf[fb_idx + 1] = g;
            framebuf[fb_idx + 2] = r;
        }
    }
}

/**
 * Update animation for all plants
 */
int game_update_animation(GameState *game)
{
    int i, j;
    int frame_changed = 0;

    game->animation_counter++;

    if (game->animation_counter >= FRAMES_PER_UPDATE) {
        game->animation_counter = 0;

        for (i = 0; i < GRID_ROWS; i++) {
            for (j = 0; j < GRID_COLS; j++) {
                if (game->grid[i][j].plant != PLANT_NONE) {
                    game->grid[i][j].animation_frame++;
                    if (game->grid[i][j].animation_frame >= ANIMATION_FRAMES) {
                        game->grid[i][j].animation_frame = 0;
                    }
                    frame_changed = 1;
                }
            }
        }
    }

    return frame_changed;
}

/**
 * Draw darkened sprite (for selected cards)
 */
void draw_sprite_darkened(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    
    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            sprite_idx = (i * w + j) * 3;
            
            framebuf[fb_idx]     = sprite_data[sprite_idx] / 2;
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1] / 2;
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2] / 2;
        }
    }
}

/**
 * Restore background rectangle from original background image
 */
void restore_background_rect(u8 *framebuf, int x, int y, int w, int h)
{
    int i, j;
    u32 fb_idx;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            framebuf[fb_idx]     = gImage_background1_hd[fb_idx];
            framebuf[fb_idx + 1] = gImage_background1_hd[fb_idx + 1];
            framebuf[fb_idx + 2] = gImage_background1_hd[fb_idx + 2];
        }
    }
}

/**
 * Check if a rectangle overlaps with UI protected areas
 * Returns 1 if overlaps with UI, 0 otherwise
 */
int is_in_ui_protected_area(int x, int y, int w, int h)
{
    // Two rectangles overlap if:
    // x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2
    // They do NOT overlap if:
    // x1 >= x2 + w2 || x1 + w1 <= x2 || y1 >= y2 + h2 || y1 + h1 <= y2

    // Check overlap with seed bank (card area)
    if (!(x >= UI_SEEDBANK_X + UI_SEEDBANK_WIDTH ||
          x + w <= UI_SEEDBANK_X ||
          y >= UI_SEEDBANK_Y + UI_SEEDBANK_HEIGHT ||
          y + h <= UI_SEEDBANK_Y)) {
        return 1;  // Overlaps with seed bank
    }

    // Check overlap with sun bank (sun counter)
    if (!(x >= UI_SUNBANK_X + UI_SUNBANK_WIDTH ||
          x + w <= UI_SUNBANK_X ||
          y >= UI_SUNBANK_Y + UI_SUNBANK_HEIGHT ||
          y + h <= UI_SUNBANK_Y)) {
        return 1;  // Overlaps with sun bank
    }

    return 0;  // No overlap with any UI area
}

/**
 * Restore background rectangle safely (avoiding UI areas)
 * CRITICAL: Completely skips pixels in UI protected areas
 * UI areas are treated as immutable - they are NEVER erased
 */
void restore_background_rect_safe(u8 *framebuf, int x, int y, int w, int h)
{
    int i, j;
    u32 fb_idx;

    // Boundary check
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;

    if (w <= 0 || h <= 0) return;

    // Restore pixel by pixel, skipping UI areas
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            int px = x + j;
            int py = y + i;

            // Check if this pixel is in SEED BANK area
            if (px >= UI_SEEDBANK_X && px < UI_SEEDBANK_X + UI_SEEDBANK_WIDTH &&
                py >= UI_SEEDBANK_Y && py < UI_SEEDBANK_Y + UI_SEEDBANK_HEIGHT) {
                continue;  // Skip - don't touch seed bank pixels
            }

            // Check if this pixel is in SUN BANK area
            if (px >= UI_SUNBANK_X && px < UI_SUNBANK_X + UI_SUNBANK_WIDTH &&
                py >= UI_SUNBANK_Y && py < UI_SUNBANK_Y + UI_SUNBANK_HEIGHT) {
                continue;  // Skip - don't touch sun bank pixels
            }

            // Safe to restore this pixel
            fb_idx = (py * SCREEN_WIDTH + px) * 3;
            framebuf[fb_idx]     = gImage_background1_hd[fb_idx];
            framebuf[fb_idx + 1] = gImage_background1_hd[fb_idx + 1];
            framebuf[fb_idx + 2] = gImage_background1_hd[fb_idx + 2];
        }
    }
}

/**
 * Check if two rectangles overlap
 */
int rects_overlap(int x1, int y1, int w1, int h1,
                  int x2, int y2, int w2, int h2)
{
    return !(x1 + w1 <= x2 || x2 + w2 <= x1 ||
             y1 + h1 <= y2 || y2 + h2 <= y1);
}

/**
 * Redraw UI elements if they were erased
 * This prevents zombies/suns from clearing the UI
 */
void redraw_ui_if_overlapped(GameState *game, u8 *framebuf,
                             int erase_x, int erase_y, int erase_w, int erase_h)
{
    int i;
    int card_x, card_y;
    const u8 *plant_data;
    int bank_width = 357;

    // Check if erase area overlaps with sun bank
    if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                     SUNBANK_X, SUNBANK_Y, 93, 70)) {
        // Redraw sun bank
        restore_background_rect(framebuf, SUNBANK_X, SUNBANK_Y, 93, 70);
        draw_sprite_transparent(framebuf, SUNBANK_X, SUNBANK_Y, gImage_SunBank, 63, 70);
        draw_number(framebuf, SUNBANK_X + 10, SUNBANK_Y + 45, game->sun_count);
    }

    // Check if erase area overlaps with seed bank
    if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                     SEEDBANK_X, SEEDBANK_Y, bank_width, 70)) {
        // Redraw seed bank background
        restore_background_rect(framebuf, SEEDBANK_X, SEEDBANK_Y, bank_width, 70);
        draw_sprite_transparent(framebuf, SEEDBANK_X, SEEDBANK_Y, gImage_SeedBank, bank_width, 70);

        // Redraw seed cards
        for (i = 0; i < NUM_CARDS; i++) {
            card_x = SEEDBANK_X + 10 + i * (CARD_WIDTH + CARD_SPACING);
            card_y = SEEDBANK_Y + 5;

            // Draw card background
            if (game->cards[i].selected) {
                draw_sprite_darkened(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
            } else {
                draw_sprite(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
            }

            // Draw plant icon
            plant_data = (game->cards[i].type == PLANT_SUNFLOWER) ? gImage_SunFlower : gImage_PeaShooter;
            int icon_x = card_x + (CARD_WIDTH - PLANT_ICON_SIZE) / 2;
            int icon_y = card_y + 5;

            draw_sprite_scaled_transparent(framebuf, icon_x, icon_y, PLANT_ICON_SIZE, PLANT_ICON_SIZE,
                                          plant_data, 90, 90);

            if (game->cards[i].selected) {
                int px, py;
                for (py = 0; py < PLANT_ICON_SIZE; py++) {
                    for (px = 0; px < PLANT_ICON_SIZE; px++) {
                        u32 idx = ((icon_y + py) * SCREEN_WIDTH + (icon_x + px)) * 3;
                        framebuf[idx] /= 2;
                        framebuf[idx + 1] /= 2;
                        framebuf[idx + 2] /= 2;
                    }
                }
            }
        }
    }
}

/**
 * OPTIMIZED: Draw only plants (for animation updates)
 * This function only redraws grid cells that have plants
 * Skips UI elements and empty cells completely
 */
void game_draw_animation(GameState *game, u8 *framebuf)
{
    int i, j;

    // Only iterate through grid cells that have plants
    for (i = 0; i < GRID_ROWS; i++) {
        for (j = 0; j < GRID_COLS; j++) {
            if (game->grid[i][j].plant != PLANT_NONE) {
                // Calculate cell position
                int cell_x = GRID_START_X + j * GRID_WIDTH;
                int cell_y = GRID_START_Y + i * GRID_HEIGHT;

                // Restore background for this cell only
                restore_background_rect(framebuf, cell_x, cell_y, GRID_WIDTH, GRID_HEIGHT);

                // Draw the animated plant
                int plant_x = cell_x + (GRID_WIDTH - PLANT_SIZE) / 2;
                int plant_y = cell_y + (GRID_HEIGHT - PLANT_SIZE) / 2;

                const u8 *sheet_data = (game->grid[i][j].plant == PLANT_SUNFLOWER) ?
                                       gImage_SunFlower_ani : gImage_PeaShooter_ani;

                draw_sprite_from_sheet(framebuf, plant_x, plant_y, PLANT_SIZE, PLANT_SIZE,
                                      sheet_data, game->grid[i][j].animation_frame);
            }
        }
    }

    // DO NOT update tracking variables here - only full draw should do that
    // This prevents interfering with change detection in timer handler
}

/**
 * FULL REDRAW: Draw complete game state (UI + all plants)
 * Called when: initial draw, planting, or UI state changes
 */
void game_draw_full(GameState *game, u8 *framebuf)
{
    int i, j;
    int card_x, card_y;
    const u8 *plant_data;

    // Draw UI elements (sun bank and seed cards)
    
    // Draw sun bank area
    restore_background_rect(framebuf, SUNBANK_X, SUNBANK_Y, 93, 70);
    draw_sprite_transparent(framebuf, SUNBANK_X, SUNBANK_Y, gImage_SunBank, 63, 70);
    draw_number(framebuf, SUNBANK_X + 10, SUNBANK_Y + 45, game->sun_count);

    // Draw seed bank area
    int bank_width = 357;
    restore_background_rect(framebuf, SEEDBANK_X, SEEDBANK_Y, bank_width, 70);
    draw_sprite_transparent(framebuf, SEEDBANK_X, SEEDBANK_Y, gImage_SeedBank, bank_width, 70);

    // Draw seed cards
    for (i = 0; i < NUM_CARDS; i++) {
        card_x = SEEDBANK_X + 10 + i * (CARD_WIDTH + CARD_SPACING);
        card_y = SEEDBANK_Y + 5;
        
        // Draw card background
        if (game->cards[i].selected) {
            draw_sprite_darkened(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
        } else {
            draw_sprite(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
        }
        
        // Draw plant icon
        plant_data = (game->cards[i].type == PLANT_SUNFLOWER) ? gImage_SunFlower : gImage_PeaShooter;
        int icon_x = card_x + (CARD_WIDTH - PLANT_ICON_SIZE) / 2;
        int icon_y = card_y + 5;
        
        draw_sprite_scaled_transparent(framebuf, icon_x, icon_y, PLANT_ICON_SIZE, PLANT_ICON_SIZE,
                                       plant_data, 90, 90);

        if (game->cards[i].selected) {
            for (int py = 0; py < PLANT_ICON_SIZE; py++) {
                for (int px = 0; px < PLANT_ICON_SIZE; px++) {
                    u32 idx = ((icon_y + py) * SCREEN_WIDTH + (icon_x + px)) * 3;
                    framebuf[idx] /= 2;
                    framebuf[idx + 1] /= 2;
                    framebuf[idx + 2] /= 2;
                }
            }
        }
    }

    // Draw all plants (iterate all cells, but only draw if plant exists)
    for (i = 0; i < GRID_ROWS; i++) {
        for (j = 0; j < GRID_COLS; j++) {
            int cell_x = GRID_START_X + j * GRID_WIDTH;
            int cell_y = GRID_START_Y + i * GRID_HEIGHT;

            // Restore background for this cell
            restore_background_rect(framebuf, cell_x, cell_y, GRID_WIDTH, GRID_HEIGHT);

            // Draw plant if exists
            if (game->grid[i][j].plant != PLANT_NONE) {
                int plant_x = cell_x + (GRID_WIDTH - PLANT_SIZE) / 2;
                int plant_y = cell_y + (GRID_HEIGHT - PLANT_SIZE) / 2;
                
                const u8 *sheet_data = (game->grid[i][j].plant == PLANT_SUNFLOWER) ?
                                       gImage_SunFlower_ani : gImage_PeaShooter_ani;

                draw_sprite_from_sheet(framebuf, plant_x, plant_y, PLANT_SIZE, PLANT_SIZE,
                                      sheet_data, game->grid[i][j].animation_frame);
            }
        }
    }

    // Update tracking variables
    game->prev_sun_count = game->sun_count;
    game->prev_selected_card = game->selected_card;
}

/**
 * Handle touch input
 */
void game_handle_touch(GameState *game, int x, int y)
{
    int i, card_x, card_y;
    int grid_row, grid_col;
    
    printf("Touch: x=%d, y=%d\n", x, y);
    
    // Check if plant card is clicked
    for (i = 0; i < NUM_CARDS; i++) {
        card_x = SEEDBANK_X + 10 + i * (CARD_WIDTH + CARD_SPACING);
        card_y = SEEDBANK_Y + 5;
        
        if (x >= card_x && x < card_x + CARD_WIDTH &&
            y >= card_y && y < card_y + CARD_HEIGHT) {
            
            if (game->sun_count >= game->cards[i].cost) {
                for (int j = 0; j < NUM_CARDS; j++) {
                    game->cards[j].selected = 0;
                }
                
                game->cards[i].selected = 1;
                game->selected_card = i;
                
                printf("Selected card %d (type=%d)\n", i, game->cards[i].type);
            } else {
                printf("Not enough sun! need=%d, current=%d\n", game->cards[i].cost, game->sun_count);
            }
            return;
        }
    }
    
    // Check if game grid is clicked (plant)
    if (game->selected_card >= 0 && 
        x >= GRID_START_X && x < GRID_START_X + GRID_COLS * GRID_WIDTH &&
        y >= GRID_START_Y && y < GRID_START_Y + GRID_ROWS * GRID_HEIGHT) {
        
        grid_col = (x - GRID_START_X) / GRID_WIDTH;
        grid_row = (y - GRID_START_Y) / GRID_HEIGHT;
        
        if (game->grid[grid_row][grid_col].plant == PLANT_NONE) {
            int plant_x = GRID_START_X + grid_col * GRID_WIDTH + (GRID_WIDTH - PLANT_SIZE) / 2;
            int plant_y = GRID_START_Y + grid_row * GRID_HEIGHT + (GRID_HEIGHT - PLANT_SIZE) / 2;

            game->grid[grid_row][grid_col].plant = game->cards[game->selected_card].type;
            game->sun_count -= game->cards[game->selected_card].cost;
            
            game->cards[game->selected_card].selected = 0;
            game->selected_card = -1;
            
            printf("Plant in (%d, %d), plant_pos=(%d, %d), remaining sun=%d\n",
                   grid_row, grid_col, plant_x, plant_y, game->sun_count);
        } else {
            printf("Grid already has plant!\n");
        }
    }
}

/**
 * Helper: Draw a single plant cell (for when sun erases a plant)
 */
void draw_single_plant_cell(GameState *game, u8 *framebuf, int row, int col)
{
    if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS)
        return;

    int cell_x = GRID_START_X + col * GRID_WIDTH;
    int cell_y = GRID_START_Y + row * GRID_HEIGHT;

    // Restore background
    restore_background_rect(framebuf, cell_x, cell_y, GRID_WIDTH, GRID_HEIGHT);

    // Draw plant if exists
    if (game->grid[row][col].plant != PLANT_NONE) {
        int plant_x = cell_x + (GRID_WIDTH - PLANT_SIZE) / 2;
        int plant_y = cell_y + (GRID_HEIGHT - PLANT_SIZE) / 2;

        const u8 *sheet_data = (game->grid[row][col].plant == PLANT_SUNFLOWER) ?
                               gImage_SunFlower_ani : gImage_PeaShooter_ani;

        draw_sprite_from_sheet(framebuf, plant_x, plant_y, PLANT_SIZE, PLANT_SIZE,
                              sheet_data, game->grid[row][col].animation_frame);
    }
}

/**
 * Spawn a new sun from a sunflower or from sky
 */
void game_spawn_sun(GameState *game, int source_x, int source_y)
{
    int i;

    // Find an inactive sun slot
    for (i = 0; i < MAX_SUNS; i++) {
        if (!game->suns[i].active) {
            game->suns[i].active = 1;
            game->suns[i].landed = 0;  // Start flying
            game->suns[i].x = (float)source_x;
            game->suns[i].y = (float)source_y;
            game->suns[i].prev_x = source_x;
            game->suns[i].prev_y = source_y;

            // Physics: arc to the right
            game->suns[i].vx = SUN_INITIAL_VX;
            game->suns[i].vy = SUN_INITIAL_VY;
            game->suns[i].lifetime = SUN_LIFETIME;

            game->num_active_suns++;
            printf("Sun spawned at (%d, %d), active suns: %d\n", source_x, source_y, game->num_active_suns);
            break;
        }
    }
}

/**
 * Update sun physics and spawn new suns from sunflowers
 * Called every timer tick (100Hz)
 */
void game_update_suns(GameState *game)
{
    int i, row, col;

    // Update existing suns (physics simulation)
    for (i = 0; i < MAX_SUNS; i++) {
        if (game->suns[i].active) {
            // Store previous position for dirty rect
//            game->suns[i].prev_x = (int)game->suns[i].x;
//            game->suns[i].prev_y = (int)game->suns[i].y;

            // Only apply physics if sun hasn't landed yet
            if (!game->suns[i].landed) {
                // Apply gravity
                game->suns[i].vy += SUN_GRAVITY;

                // Update position
                game->suns[i].x += game->suns[i].vx;
                game->suns[i].y += game->suns[i].vy;

                // Check if sun has reached landing height
                if (game->suns[i].y >= SUN_LANDING_HEIGHT) {
                    game->suns[i].y = (float)SUN_LANDING_HEIGHT;
                    game->suns[i].vx = 0.0f;
                    game->suns[i].vy = 0.0f;
                    game->suns[i].landed = 1;
                    printf("Sun %d landed at height %d\n", i, SUN_LANDING_HEIGHT);
                }
            }

            // Decrease lifetime (for both flying and landed suns)
            game->suns[i].lifetime--;
            if (game->suns[i].lifetime <= 0) {
                game->suns[i].active = 0;
                game->num_active_suns--;
                printf("Sun %d expired, active suns: %d\n", i, game->num_active_suns);
            }
        }
    }

    // Check sunflowers for sun production
    for (row = 0; row < GRID_ROWS; row++) {
        for (col = 0; col < GRID_COLS; col++) {
            if (game->grid[row][col].plant == PLANT_SUNFLOWER) {
                game->grid[row][col].sun_spawn_counter++;

                // Spawn sun every SUN_SPAWN_INTERVAL ticks (25 seconds)
                if (game->grid[row][col].sun_spawn_counter >= SUN_SPAWN_INTERVAL) {
                    game->grid[row][col].sun_spawn_counter = 0;

                    // Spawn sun above the sunflower
                    int cell_x = GRID_START_X + col * GRID_WIDTH;
                    int cell_y = GRID_START_Y + row * GRID_HEIGHT;
                    int spawn_x = cell_x + GRID_WIDTH / 2 - SUN_SIZE / 2;
                    int spawn_y = cell_y - SUN_SIZE;  // Above the plant

                    game_spawn_sun(game, spawn_x, spawn_y);
                }
            }
        }
    }
}

/**
 * Check if a touch/click hit a sun and collect it
 * Returns 1 if sun was collected, 0 otherwise
 */
int game_check_sun_click(GameState *game, int x, int y)
{
    int i;

    for (i = 0; i < MAX_SUNS; i++) {
        if (game->suns[i].active) {
            int sun_x = (int)game->suns[i].x;
            int sun_y = (int)game->suns[i].y;

            // Check if click is within sun bounds
            if (x >= sun_x && x < sun_x + SUN_SIZE &&
                y >= sun_y && y < sun_y + SUN_SIZE) {

                // Collect sun
                game->sun_count += SUN_VALUE;
                game->suns[i].active = 0;
                game->num_active_suns--;

                printf("Sun collected! Total sun: %d\n", game->sun_count);
                return 1;
            }
        }
    }

    return 0;
}

/**
 * OPTIMIZED: Draw suns with dirty rectangle optimization
 * Uses two-phase approach: erase all, then draw all
 * This prevents suns from erasing each other
 */
void game_draw_suns(GameState *game, u8 *framebuf)
{
    int i, j, row, col;
    extern const unsigned char gImage_Sun[];

    // --- PHASE 1: ERASE ALL OLD POSITIONS ---
    for (i = 0; i < MAX_SUNS; i++) {
        int prev_x = game->suns[i].prev_x;
        int prev_y = game->suns[i].prev_y;
        int need_erase = 0;

        // Determine if we need to erase:
        // 1. Sun is active (active=1): need to erase previous position
        // 2. Sun just died (active=0 but prev_x!=-1): need to erase last frame
        if (game->suns[i].active) {
            need_erase = 1;
        } else if (prev_x != -1) {
            need_erase = 1;
        }

        if (need_erase) {
            int erase_x = prev_x - SUN_ERASE_MARGIN;
            int erase_y = prev_y - SUN_ERASE_MARGIN;
            int erase_w = SUN_SIZE + (SUN_ERASE_MARGIN * 2);
            int erase_h = SUN_SIZE + (SUN_ERASE_MARGIN * 2);

            // Boundary check
            if (erase_x < 0) { erase_w += erase_x; erase_x = 0; }
            if (erase_y < 0) { erase_h += erase_y; erase_y = 0; }
            if (erase_x + erase_w > SCREEN_WIDTH) erase_w = SCREEN_WIDTH - erase_x;
            if (erase_y + erase_h > SCREEN_HEIGHT) erase_h = SCREEN_HEIGHT - erase_y;

            // 1. Restore background (SAFE - avoids UI areas)
            restore_background_rect_safe(framebuf, erase_x, erase_y, erase_w, erase_h);

            // 2. Redraw UI elements if they were in the erase area (CRITICAL!)
            redraw_ui_if_overlapped(game, framebuf, erase_x, erase_y, erase_w, erase_h);

            // 3. Redraw plants that were covered
            for (row = 0; row < GRID_ROWS; row++) {
                for (col = 0; col < GRID_COLS; col++) {
                    if (game->grid[row][col].plant != PLANT_NONE) {
                        int cell_x = GRID_START_X + col * GRID_WIDTH;
                        int cell_y = GRID_START_Y + row * GRID_HEIGHT;

                        if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                                        cell_x, cell_y, GRID_WIDTH, GRID_HEIGHT)) {
                            draw_single_plant_cell(game, framebuf, row, col);
                        }
                    }
                }
            }

            // 4. Redraw zombies that were covered
            for (j = 0; j < MAX_ZOMBIES; j++) {
                if (game->zombies[j].active) {
                    int zombie_x = (int)game->zombies[j].x;
                    int zombie_y = (int)game->zombies[j].y + ZOMBIE_Y_OFFSET;

                    if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                                    zombie_x, zombie_y, ZOMBIE_DISPLAY_WIDTH, ZOMBIE_DISPLAY_HEIGHT)) {
                        draw_zombie_sprite(framebuf, (int)game->zombies[j].x, (int)game->zombies[j].y,
                                         gImage_walk_ani, game->zombies[j].animation_frame);
                    }
                }
            }
        }
    }

    // --- PHASE 2: DRAW ALL NEW SUN POSITIONS ---
    for (i = 0; i < MAX_SUNS; i++) {
        if (game->suns[i].active) {
            int curr_x = (int)game->suns[i].x;
            int curr_y = (int)game->suns[i].y;

            if (curr_x >= 0 && curr_y >= 0 &&
                curr_x + SUN_SIZE <= SCREEN_WIDTH &&
                curr_y + SUN_SIZE <= SCREEN_HEIGHT) {
                draw_sprite_transparent(framebuf, curr_x, curr_y, gImage_Sun, SUN_SIZE, SUN_SIZE);
            }

            // Update position tracking
            game->suns[i].prev_x = curr_x;
            game->suns[i].prev_y = curr_y;
        }
        else {
            // Sun died and erased, set position to -1 to avoid re-erase
            game->suns[i].prev_x = -1;
        }
    }
}

/* ============================================================ */
/*                    ZOMBIE FUNCTIONS                          */
/* ============================================================ */

/**
 * Spawn a new zombie at random row
 */
void game_spawn_zombie(GameState *game)
{
    int i;

    // Find an inactive zombie slot
    for (i = 0; i < MAX_ZOMBIES; i++) {
        if (!game->zombies[i].active) {
            // Spawn zombie
            game->zombies[i].active = 1;
            game->zombies[i].x = (float)ZOMBIE_SPAWN_X;

            // Random row (0-4)
            game->zombies[i].row = rand() % GRID_ROWS;

            // Calculate Y position based on row
            game->zombies[i].y = (float)(GRID_START_Y + game->zombies[i].row * GRID_HEIGHT);

            // Initialize position tracking
            game->zombies[i].prev_x = ZOMBIE_SPAWN_X;
            game->zombies[i].prev_y = (int)game->zombies[i].y;

            // Start at random animation frame for variety
            game->zombies[i].animation_frame = rand() % (ZOMBIE_ROWS * ZOMBIE_COLS);

            game->num_active_zombies++;

            printf("Zombie spawned at row %d\n", game->zombies[i].row);
            break;
        }
    }
}

/**
 * Update zombie positions and animation
 * Returns 1 if any zombie moved, 0 otherwise
 */
void game_update_zombies(GameState *game)
{
    int i;

    // Update spawn counter
    game->zombie_spawn_counter++;
    if (game->zombie_spawn_counter >= ZOMBIE_SPAWN_INTERVAL) {
        game->zombie_spawn_counter = 0;
        game_spawn_zombie(game);
    }

    // Update animation counter
    game->zombie_animation_counter++;
    if (game->zombie_animation_counter >= ZOMBIE_FRAMES_PER_UPDATE) {
        game->zombie_animation_counter = 0;

        // Update animation frame for all active zombies
        for (i = 0; i < MAX_ZOMBIES; i++) {
            if (game->zombies[i].active) {
                game->zombies[i].animation_frame++;
                if (game->zombies[i].animation_frame >= ZOMBIE_ROWS * ZOMBIE_COLS) {
                    game->zombies[i].animation_frame = 0;
                }
            }
        }
    }

    // Update positions
    for (i = 0; i < MAX_ZOMBIES; i++) {
        if (game->zombies[i].active) {
            // Move left
            game->zombies[i].x -= ZOMBIE_SPEED;

            // Check if zombie went off screen (use display width)
            if (game->zombies[i].x + ZOMBIE_DISPLAY_WIDTH < 0) {
                game->zombies[i].active = 0;
                game->num_active_zombies--;
                printf("Zombie left screen\n");
            }
        }
    }
}

/**
 * Draw zombie sprite from sprite sheet with scaling and black transparency
 * CRITICAL: Does NOT draw in UI protected areas
 */
void draw_zombie_sprite(u8 *framebuf, int dst_x, int dst_y,
                       const u8 *sheet_data, int frame_index)
{
    int row, col, i, j;
    int src_x, src_y;
    u32 fb_idx, sheet_idx;
    float scale = ZOMBIE_SCALE;

    // Apply Y offset for better visual positioning
    dst_y += ZOMBIE_Y_OFFSET;

    // Boundary check (use display size for bounds)
    if (dst_x + ZOMBIE_DISPLAY_WIDTH < 0 || dst_x >= SCREEN_WIDTH ||
        dst_y + ZOMBIE_DISPLAY_HEIGHT < 0 || dst_y >= SCREEN_HEIGHT) {
        return;
    }

    // Calculate source position in sprite sheet
    row = frame_index / ZOMBIE_COLS;
    col = frame_index % ZOMBIE_COLS;
    src_x = col * ZOMBIE_WIDTH;
    src_y = row * ZOMBIE_HEIGHT;

    // Pre-calculate UI boundaries for fast checking
    int seedbank_right = UI_SEEDBANK_X + UI_SEEDBANK_WIDTH;
    int seedbank_bottom = UI_SEEDBANK_Y + UI_SEEDBANK_HEIGHT;
    int sunbank_right = UI_SUNBANK_X + UI_SUNBANK_WIDTH;
    int sunbank_bottom = UI_SUNBANK_Y + UI_SUNBANK_HEIGHT;

    // Draw with scaling and transparency (black background = transparent)
    for (i = 0; i < ZOMBIE_DISPLAY_HEIGHT; i++) {
        int pixel_y = dst_y + i;

        // Skip if row is outside screen
        if (pixel_y < 0 || pixel_y >= SCREEN_HEIGHT)
            continue;

        // Calculate source row (inverse scaling)
        int src_i = (int)(i / scale);
        if (src_i >= ZOMBIE_HEIGHT)
            src_i = ZOMBIE_HEIGHT - 1;

        for (j = 0; j < ZOMBIE_DISPLAY_WIDTH; j++) {
            int pixel_x = dst_x + j;

            // Skip if column is outside screen
            if (pixel_x < 0 || pixel_x >= SCREEN_WIDTH)
                continue;

            // CRITICAL: Skip if pixel is in UI protected area
            // Check seedbank
            if (pixel_x >= UI_SEEDBANK_X && pixel_x < seedbank_right &&
                pixel_y >= UI_SEEDBANK_Y && pixel_y < seedbank_bottom) {
                continue;  // Don't draw on seedbank!
            }
            // Check sunbank
            if (pixel_x >= UI_SUNBANK_X && pixel_x < sunbank_right &&
                pixel_y >= UI_SUNBANK_Y && pixel_y < sunbank_bottom) {
                continue;  // Don't draw on sunbank!
            }

            // Calculate source column (inverse scaling)
            int src_j = (int)(j / scale);
            if (src_j >= ZOMBIE_WIDTH)
                src_j = ZOMBIE_WIDTH - 1;

            sheet_idx = ((src_y + src_i) * ZOMBIE_SHEET_WIDTH + (src_x + src_j)) * 3;

            // Check for BLACK background (0, 0, 0) - skip transparent pixels
            if (sheet_data[sheet_idx] == 0 &&
                sheet_data[sheet_idx + 1] == 0 &&
                sheet_data[sheet_idx + 2] == 0) {
                continue;
            }

            fb_idx = (pixel_y * SCREEN_WIDTH + pixel_x) * 3;
            framebuf[fb_idx]     = sheet_data[sheet_idx];
            framebuf[fb_idx + 1] = sheet_data[sheet_idx + 1];
            framebuf[fb_idx + 2] = sheet_data[sheet_idx + 2];
        }
    }
}

/**
 * Draw all zombies with dirty rect optimization
 * Uses two-phase approach: erase all, then draw all
 */
void game_draw_zombies(GameState *game, u8 *framebuf)
{
    int i, j, row, col;

    // --- PHASE 1: ERASE ALL OLD POSITIONS ---
    for (i = 0; i < MAX_ZOMBIES; i++) {
        int prev_x = game->zombies[i].prev_x;
        int prev_y = game->zombies[i].prev_y;
        int need_erase = 0;

        // Determine if we need to erase:
        // 1. Zombie is active (active=1): need to erase previous position
        // 2. Zombie just died (active=0 but prev_x!=-1): need to erase last frame
        if (game->zombies[i].active) {
            need_erase = 1;
        } else if (prev_x != -1) {
            need_erase = 1;
        }

        if (need_erase) {
            // Account for Y offset and use display size
            int erase_x = prev_x - ZOMBIE_ERASE_MARGIN;
            int erase_y = (prev_y + ZOMBIE_Y_OFFSET) - ZOMBIE_ERASE_MARGIN;
            int erase_w = ZOMBIE_DISPLAY_WIDTH + (ZOMBIE_ERASE_MARGIN * 2);
            int erase_h = ZOMBIE_DISPLAY_HEIGHT + (ZOMBIE_ERASE_MARGIN * 2);

            // Boundary check
            if (erase_x < 0) { erase_w += erase_x; erase_x = 0; }
            if (erase_y < 0) { erase_h += erase_y; erase_y = 0; }
            if (erase_x + erase_w > SCREEN_WIDTH) erase_w = SCREEN_WIDTH - erase_x;
            if (erase_y + erase_h > SCREEN_HEIGHT) erase_h = SCREEN_HEIGHT - erase_y;

            // 1. Restore background (SAFE - avoids UI areas)
            restore_background_rect_safe(framebuf, erase_x, erase_y, erase_w, erase_h);

            // 2. Redraw UI elements if they were in the erase area (CRITICAL!)
            redraw_ui_if_overlapped(game, framebuf, erase_x, erase_y, erase_w, erase_h);

            // 3. Redraw plants that were covered
            for (row = 0; row < GRID_ROWS; row++) {
                for (col = 0; col < GRID_COLS; col++) {
                    if (game->grid[row][col].plant != PLANT_NONE) {
                        int cell_x = GRID_START_X + col * GRID_WIDTH;
                        int cell_y = GRID_START_Y + row * GRID_HEIGHT;

                        if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                                        cell_x, cell_y, GRID_WIDTH, GRID_HEIGHT)) {
                            draw_single_plant_cell(game, framebuf, row, col);
                        }
                    }
                }
            }

            // 4. Redraw suns that were covered (CRITICAL: zombie cannot erase suns)
            for (j = 0; j < MAX_SUNS; j++) {
                if (game->suns[j].active) {
                    int sun_x = (int)game->suns[j].x;
                    int sun_y = (int)game->suns[j].y;

                    if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                                    sun_x, sun_y, SUN_SIZE, SUN_SIZE)) {
                        draw_sprite_transparent(framebuf, sun_x, sun_y,
                                              gImage_Sun, SUN_SIZE, SUN_SIZE);
                    }
                }
            }

            // 5. Redraw other zombies that were covered (CRITICAL: zombie cannot erase other zombies)
            int k;
            for (k = 0; k < MAX_ZOMBIES; k++) {
                // Don't redraw the zombie we're currently processing (i)
                // Also skip zombies that will be erased in this phase
                if (k != i && game->zombies[k].active) {
                    int other_x = (int)game->zombies[k].x;
                    int other_y = (int)game->zombies[k].y + ZOMBIE_Y_OFFSET;

                    if (rects_overlap(erase_x, erase_y, erase_w, erase_h,
                                    other_x, other_y, ZOMBIE_DISPLAY_WIDTH, ZOMBIE_DISPLAY_HEIGHT)) {
                        draw_zombie_sprite(framebuf, (int)game->zombies[k].x, (int)game->zombies[k].y,
                                         gImage_walk_ani, game->zombies[k].animation_frame);
                    }
                }
            }
        }
    }

    // --- PHASE 2: DRAW ALL NEW ZOMBIE POSITIONS ---
    for (i = 0; i < MAX_ZOMBIES; i++) {
        if (game->zombies[i].active) {
            int curr_x = (int)game->zombies[i].x;
            int curr_y = (int)game->zombies[i].y;

            // Draw zombie sprite
            draw_zombie_sprite(framebuf, curr_x, curr_y,
                             gImage_walk_ani, game->zombies[i].animation_frame);

            // Update position tracking
            game->zombies[i].prev_x = curr_x;
            game->zombies[i].prev_y = curr_y;
        }
        else {
            // Zombie died and erased, set position to -1 to avoid re-erase
            game->zombies[i].prev_x = -1;
            game->zombies[i].prev_y = -1;
        }
    }
}
