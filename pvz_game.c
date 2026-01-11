/* ------------------------------------------------------------ */
/*                  PVZ Game Logic                              */
/* ------------------------------------------------------------ */
#include "pvz_game.h"
#include <stdio.h>
#include <string.h>

// Include image header files (user provided locally)
#include "background1_hd.h"
#include "SeedBank.h"
#include "SeedPacket.h"
#include "SunBank.h"
#include "PeaShooter.h"
#include "SunFlower.h"
#include "PeaShooter_ani.h"   // Animation sprite sheet for PeaShooter
#include "SunFlower_ani.h"    // Animation sprite sheet for SunFlower

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
    
    // Initial sun count
    game->sun_count = 150;
    game->selected_card = -1;
    game->animation_counter = 0;  // Initialize animation counter
    
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
            game->grid[i][j].animation_frame = 0;  // Initialize animation frame
        }
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
    
    // Boundary check
    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            sprite_idx = (i * w + j) * 3;
            
            framebuf[fb_idx]     = sprite_data[sprite_idx];     // B
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1]; // G
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2]; // R
        }
    }
}

/**
 * Draw sprite with transparency (skip pure black pixels)
 */
void draw_sprite_transparent(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    u8 b, g, r;

    // Boundary check
    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;

    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            sprite_idx = (i * w + j) * 3;

            b = sprite_data[sprite_idx];
            g = sprite_data[sprite_idx + 1];
            r = sprite_data[sprite_idx + 2];

            // Skip pure black pixels (transparent area)
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
 * Draw scaled sprite (for plant icons on cards)
 */
void draw_sprite_scaled(u8 *framebuf, int dst_x, int dst_y, int dst_w, int dst_h,
                        const u8 *sprite_data, int src_w, int src_h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    int src_x, src_y;
    
    // Boundary check
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            // Nearest neighbor interpolation
            src_x = (j * src_w) / dst_w;
            src_y = (i * src_h) / dst_h;
            
            fb_idx = ((dst_y + i) * SCREEN_WIDTH + (dst_x + j)) * 3;
            sprite_idx = (src_y * src_w + src_x) * 3;
            
            framebuf[fb_idx]     = sprite_data[sprite_idx];     // B
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1]; // G
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2]; // R
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

    // Boundary check
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;

    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            // Nearest neighbor interpolation
            src_x = (j * src_w) / dst_w;
            src_y = (i * src_h) / dst_h;

            sprite_idx = (src_y * src_w + src_x) * 3;

            b = sprite_data[sprite_idx];
            g = sprite_data[sprite_idx + 1];
            r = sprite_data[sprite_idx + 2];

            // Skip pure black pixels (transparent area)
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
                        framebuf[fb_idx]     = 0;    // B
                        framebuf[fb_idx + 1] = 0;    // G
                        framebuf[fb_idx + 2] = 0;    // R
                    }
                }
            }
        }
    }
}

/**
 * Extract a frame from sprite sheet and draw it scaled with transparency
 * sprite sheet: 400x400, 5x5 grid, each frame 80x80
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

    // Boundary check
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_w > SCREEN_WIDTH || dst_y + dst_h > SCREEN_HEIGHT)
        return;

    // Calculate frame position in sprite sheet (5x5 grid)
    int frame_row = frame_index / SPRITE_COLS;
    int frame_col = frame_index % SPRITE_COLS;

    src_x_offset = frame_col * FRAME_SIZE;  // 80 * col
    src_y_offset = frame_row * FRAME_SIZE;  // 80 * row

    // Draw with scaling and transparency
    for (i = 0; i < dst_h; i++) {
        for (j = 0; j < dst_w; j++) {
            // Nearest neighbor interpolation
            src_x = src_x_offset + (j * FRAME_SIZE) / dst_w;
            src_y = src_y_offset + (i * FRAME_SIZE) / dst_h;

            // Calculate index in sprite sheet (400x400)
            sheet_idx = (src_y * SPRITE_SHEET_SIZE + src_x) * 3;

            b = sheet_data[sheet_idx];
            g = sheet_data[sheet_idx + 1];
            r = sheet_data[sheet_idx + 2];

            // Skip pure black pixels (transparent area)
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
 * Returns 1 if animation frame changed, 0 otherwise
 */
int game_update_animation(GameState *game)
{
    int i, j;
    int frame_changed = 0;

    game->animation_counter++;

    // Update animation frame every FRAMES_PER_UPDATE cycles (approx 12 fps)
    if (game->animation_counter >= FRAMES_PER_UPDATE) {
        game->animation_counter = 0;

        // Update each planted cell
        for (i = 0; i < GRID_ROWS; i++) {
            for (j = 0; j < GRID_COLS; j++) {
                if (game->grid[i][j].plant != PLANT_NONE) {
                    game->grid[i][j].animation_frame++;
                    if (game->grid[i][j].animation_frame >= ANIMATION_FRAMES) {
                        game->grid[i][j].animation_frame = 0;  // Loop animation
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
static void draw_sprite_darkened(u8 *framebuf, int x, int y, const u8 *sprite_data, int w, int h)
{
    int i, j;
    u32 fb_idx, sprite_idx;
    
    if (x < 0 || y < 0 || x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT)
        return;
    
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            fb_idx = ((y + i) * SCREEN_WIDTH + (x + j)) * 3;
            sprite_idx = (i * w + j) * 3;
            
            // Darken by 50%
            framebuf[fb_idx]     = sprite_data[sprite_idx] / 2;
            framebuf[fb_idx + 1] = sprite_data[sprite_idx + 1] / 2;
            framebuf[fb_idx + 2] = sprite_data[sprite_idx + 2] / 2;
        }
    }
}

/**
 * Draw game interface
 */
void game_draw(GameState *game, u8 *framebuf)
{
    int i, j;
    int card_x, card_y;
    const u8 *plant_data;
    
    // 1. Draw background
    memcpy(framebuf, gImage_background1_hd, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    
    // 2. Draw seed bank
    draw_sprite(framebuf, SEEDBANK_X, SEEDBANK_Y, gImage_SeedBank, 357, 70);
    
    // 3. Draw sun bank
    draw_sprite(framebuf, SUNBANK_X, SUNBANK_Y, gImage_SunBank, 63, 70);
    draw_number(framebuf, SUNBANK_X + 10, SUNBANK_Y + 45, game->sun_count);
    
    // 4. Draw plant cards
    for (i = 0; i < NUM_CARDS; i++) {
        card_x = SEEDBANK_X + 10 + i * (CARD_WIDTH + CARD_SPACING);
        card_y = SEEDBANK_Y + 5;
        
        // Draw card background
        if (game->cards[i].selected) {
            draw_sprite_darkened(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
        } else {
            draw_sprite(framebuf, card_x, card_y, gImage_SeedPacket, CARD_WIDTH, CARD_HEIGHT);
        }
        
        // Draw plant icon on card (scaled + transparent)
        plant_data = (game->cards[i].type == PLANT_SUNFLOWER) ? gImage_SunFlower : gImage_PeaShooter;
        
        int icon_x = card_x + (CARD_WIDTH - PLANT_ICON_SIZE) / 2;
        int icon_y = card_y + 5;
        
        // Use transparent drawing
        draw_sprite_scaled_transparent(framebuf, icon_x, icon_y, PLANT_ICON_SIZE, PLANT_ICON_SIZE,
                                      plant_data, 90, 90);

        if (game->cards[i].selected) {
            // Selected state, darken again
            for (int py = 0; py < PLANT_ICON_SIZE; py++) {
                for (int px = 0; px < PLANT_ICON_SIZE; px++) {
                    u32 idx = ((icon_y + py) * SCREEN_WIDTH + (icon_x + px)) * 3;
                    framebuf[idx] = framebuf[idx] / 2;
                    framebuf[idx + 1] = framebuf[idx + 1] / 2;
                    framebuf[idx + 2] = framebuf[idx + 2] / 2;
                }
            }
        }
    }
    
    // 5. Draw planted plants (use animation sprite sheet)
    for (i = 0; i < GRID_ROWS; i++) {
        for (j = 0; j < GRID_COLS; j++) {
            if (game->grid[i][j].plant != PLANT_NONE) {
                // Calculate plant position (centered in grid)
                int plant_x = GRID_START_X + j * GRID_WIDTH + (GRID_WIDTH - PLANT_SIZE) / 2;
                int plant_y = GRID_START_Y + i * GRID_HEIGHT + (GRID_HEIGHT - PLANT_SIZE) / 2;
                
                // Get animation sprite sheet
                const u8 *sheet_data = (game->grid[i][j].plant == PLANT_SUNFLOWER) ?
                                       gImage_SunFlower_ani : gImage_PeaShooter_ani;

                // Draw current animation frame from sprite sheet
                draw_sprite_from_sheet(framebuf, plant_x, plant_y, PLANT_SIZE, PLANT_SIZE,
                                      sheet_data, game->grid[i][j].animation_frame);
            }
        }
    }
}

/**
 * Handle touch input
 */
void game_handle_touch(GameState *game, int x, int y)
{
    int i, card_x, card_y;
    int grid_row, grid_col;
    
    printf("Touch: x=%d, y=%d\n", x, y);
    
    // 1. Check if plant card is clicked
    for (i = 0; i < NUM_CARDS; i++) {
        card_x = SEEDBANK_X + 10 + i * (CARD_WIDTH + CARD_SPACING);
        card_y = SEEDBANK_Y + 5;
        
        if (x >= card_x && x < card_x + CARD_WIDTH &&
            y >= card_y && y < card_y + CARD_HEIGHT) {
            
            // Check if sun is sufficient
            if (game->sun_count >= game->cards[i].cost) {
                // Deselect other cards
                for (int j = 0; j < NUM_CARDS; j++) {
                    game->cards[j].selected = 0;
                }
                
                // Toggle current card state
                game->cards[i].selected = 1;
                game->selected_card = i;
                
                printf("Selected card %d (type=%d)\n", i, game->cards[i].type);
            } else {
                printf("Not enough sun! need=%d, current=%d\n", game->cards[i].cost, game->sun_count);
            }
            return;
        }
    }
    
    // 2. Check if game grid is clicked (plant)
    if (game->selected_card >= 0 && 
        x >= GRID_START_X && x < GRID_START_X + GRID_COLS * GRID_WIDTH &&
        y >= GRID_START_Y && y < GRID_START_Y + GRID_ROWS * GRID_HEIGHT) {
        
        grid_col = (x - GRID_START_X) / GRID_WIDTH;
        grid_row = (y - GRID_START_Y) / GRID_HEIGHT;
        
        // Check if grid is empty
        if (game->grid[grid_row][grid_col].plant == PLANT_NONE) {
            // Calculate actual plant position (for debug)
            int plant_x = GRID_START_X + grid_col * GRID_WIDTH + (GRID_WIDTH - PLANT_SIZE) / 2;
            int plant_y = GRID_START_Y + grid_row * GRID_HEIGHT + (GRID_HEIGHT - PLANT_SIZE) / 2;

            // Plant
            game->grid[grid_row][grid_col].plant = game->cards[game->selected_card].type;
            game->sun_count -= game->cards[game->selected_card].cost;
            
            // Deselect
            game->cards[game->selected_card].selected = 0;
            game->selected_card = -1;
            
            printf("Plant in (%d, %d), plant_pos=(%d, %d), remaining sun=%d\n",
                   grid_row, grid_col, plant_x, plant_y, game->sun_count);
        } else {
            printf("Grid already has plant!\n");
        }
    }
}
