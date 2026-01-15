/* ------------------------------------------------------------ */
/*   PVZ Game - Main Program (VSYNC Synchronized - FIXED)      */
/* ------------------------------------------------------------ */
#include "display_demo.h"
#include "display_ctrl/display_ctrl.h"
#include "i2c/PS_i2c.h"
#include <stdio.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "xscutimer.h"
#include "ax_pwm.h"
#include "touch/touch.h"
#include "pvz_game.h"
#include "background1_hd.h"
#include "touch_event_queue.h"

// Parameter definitions
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID

// Instance declarations
DisplayCtrl DispCtrl_Inst;
XAxiVdma VDMA_Inst;
XIicPs I2C0_Inst;
XScuGic GIC_Inst;
XScuTimer Timer_Inst;

// Frame buffers (must be 2 or 3)
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];

// Global tick counter (incremented by Timer ISR)
volatile u32 g_tick = 0;

// VDMA frame done flag (set by VDMA ISR, cleared by main loop)
volatile int vdma_frame_done = 0;

// Game state
GameState game;

// PWM duty cycle
float PWM_duty;

/* ============================================================ */
/*         Touch Event Queue Implementation                     */
/* ============================================================ */

volatile TouchEvent tq[TQ_SIZE];
volatile u8 tq_w = 0;
volatile u8 tq_r = 0;

void tq_push(u16 x, u16 y, u8 is_down)
{
    u8 next = (tq_w + 1) & (TQ_SIZE - 1);
    if (next == tq_r) return;

    tq[tq_w].x = x;
    tq[tq_w].y = y;
    tq[tq_w].is_down = is_down;

    __asm__ volatile("dmb sy" ::: "memory");

    tq_w = next;
}

int tq_pop(TouchEvent *e)
{
    if (tq_r == tq_w) return 0;

    *e = tq[tq_r];

    __asm__ volatile("dmb sy" ::: "memory");

    tq_r = (tq_r + 1) & (TQ_SIZE - 1);
    return 1;
}

/* ============================================================ */
/*         VDMA Frame Done Interrupt Handler                    */
/* ============================================================ */

/**
 * VDMA MM2S (Read) interrupt handler
 * Called when VDMA completes reading a frame
 */
static void VdmaReadIntrHandler(void *Callback, u32 Mask)
{
    XAxiVdma *VdmaPtr = (XAxiVdma *)Callback;

    /* Check for frame count interrupt (frame done) */
    if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
        vdma_frame_done = 1;
    }

    /* Check for errors */
    if (Mask & XAXIVDMA_IXR_ERROR_MASK) {
        /* Clear errors - FIX: Get channel pointer and use correct signature */
        XAxiVdma_Channel *ReadChannel = XAxiVdma_GetChannel(VdmaPtr, XAXIVDMA_READ);
        XAxiVdma_ClearChannelErrors(ReadChannel, 0xFFFFFFFF);
    }
}

/* ============================================================ */
/*    CRITICAL FIX: Proper VSYNC-synchronized frame present     */
/* ============================================================ */

/**
 * Present a frame at VSYNC boundary (TEAR-FREE)
 *
 * CORRECT ORDER:
 * 1. Wait for current frame to finish scanning (VSYNC/FrameDone)
 * 2. Switch frame pointer at frame boundary
 *
 * WRONG ORDER (causes tearing):
 * 1. Switch frame pointer immediately (may be mid-scan!)
 * 2. Wait for new frame
 */
static inline void present_frame_at_vsync(int frame_index)
{
    /* STEP 1: Wait for current frame to finish scanning */
    while (!vdma_frame_done) {
        __asm__ volatile("wfi");  /* Save power while waiting */
    }
    vdma_frame_done = 0;  /* Clear flag */

    /* STEP 2: Now it's safe - switch at frame boundary (no tearing!) */
    DisplayChangeFrame(&DispCtrl_Inst, frame_index);
}

/* ============================================================ */

// Function declarations
int PS_timer_init(XScuTimer *Timer, u16 DeviceId, u32 timer_load);
int GIC_Init(u16 DeviceId, XScuGic *XScuGicInstancePtr);
int VDMA_Interrupt_Init(XAxiVdma *VdmaPtr, XScuGic *IntcPtr);
static void Timer_IRQ_Handler(void *CallBackRef);

int main(void)
{
    XAxiVdma_Config *vdmaConfig;
    u8 *pFrames[DISPLAY_NUM_FRAMES];
    int i;
    int Status;

    printf("\n========================================\n");
    printf("  Plants vs Zombies - VSYNC Fixed\n");
    printf("  Resolution: 800x480\n");
    printf("  Frame Buffers: %d\n", DISPLAY_NUM_FRAMES);
    printf("========================================\n\n");

    // Verify configuration
    if (DISPLAY_NUM_FRAMES < 2) {
        printf("ERROR: DISPLAY_NUM_FRAMES must be >= 2!\n");
        return XST_FAILURE;
    }

    // Initialize frame buffer pointers
    for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
        pFrames[i] = frameBuf[i];

    // Set PWM for LCD backlight
    PWM_duty = 0.7;
    set_pwm_frequency(XPAR_AX_PWM_0_S00_AXI_BASEADDR, 100000000, 200);
    set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, PWM_duty);

    // GIC initialization
    GIC_Init(XPAR_XSCUTIMER_0_DEVICE_ID, &GIC_Inst);

    // I2C interface initialization
    i2c_init(&I2C0_Inst, XPAR_XIICPS_0_DEVICE_ID, 100000);

    // Touch screen interrupt initialization
    touch_interrupt_init(&I2C0_Inst, &GIC_Inst, 63);

    printf("INFO: Touch queue initialized\n");

    // CPU private timer initialization (10ms period, 100Hz)
    PS_timer_init(&Timer_Inst, XPAR_XSCUTIMER_0_DEVICE_ID,
                  (u32)(XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 200 - 1));

    // VDMA instance initialization
    vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
    XAxiVdma_CfgInitialize(&VDMA_Inst, vdmaConfig, vdmaConfig->BaseAddress);

    // Initialize VDMA, VTC, DynClk modules
    DisplayInitialize(&DispCtrl_Inst, &VDMA_Inst, DISP_VTC_ID,
                      DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);

    // CRITICAL: Initialize VDMA interrupt for frame sync
    Status = VDMA_Interrupt_Init(&VDMA_Inst, &GIC_Inst);
    if (Status != XST_SUCCESS) {
        printf("ERROR: VDMA interrupt initialization failed\n");
        return XST_FAILURE;
    }
    printf("INFO: VDMA interrupt initialized\n");

    DisplayStart(&DispCtrl_Inst);

    printf("Display system initialized\n");

    // Initialize game
    game_init(&game);

    // CRITICAL: Maintain explicit front/back buffer indices
    int current_displayed = 0;  // Frame currently being scanned out
    int next_render = 1;        // Frame we'll render to next

    // Initialize ALL buffers with same content
    printf("Initializing frame buffers...\n");
    for (i = 0; i < DISPLAY_NUM_FRAMES; i++) {
        u8 *init_fb = (u8 *)DispCtrl_Inst.framePtr[i];
        memcpy(init_fb, gImage_background1_hd, DEMO_MAX_FRAME);
        game_draw_full(&game, init_fb);
        Xil_DCacheFlushRange((UINTPTR)init_fb, DEMO_MAX_FRAME);
    }

    // Start displaying first buffer
    DisplayChangeFrame(&DispCtrl_Inst, current_displayed);
    vdma_frame_done = 0;  // Clear flag

    printf("\n========================================\n");
    printf("Game started - TEAR-FREE rendering!\n");
    printf("========================================\n\n");

    // Touch state machine
    int has_down = 0;
    u16 down_x = 0, down_y = 0;

    // Tick accumulator
    u32 tick_accum = 0;

    // Game over rendering state
    static int fade_needs_black_transition = 0;
    static GamePlayState prev_play_state = GAME_PLAYING;

    // Main loop
    while (1) {
        // ===== STEP 1: Atomically consume ticks =====
        Xil_ExceptionDisable();
        tick_accum += g_tick;
        g_tick = 0;
        Xil_ExceptionEnable();

        // Redraw flags
        u32 flags = 0;
        #define F_FULL   (1u << 0)
        #define F_ANIM   (1u << 1)
        #define F_SUN    (1u << 2)
        #define F_ZOMBIE (1u << 3)
        #define F_PEA    (1u << 4)

        // ===== STEP 2: Fixed-timestep update =====
        const u32 MAX_STEPS_PER_FRAME = 3;
        u32 steps = 0;

        while (tick_accum && steps < MAX_STEPS_PER_FRAME) {
            tick_accum--;
            steps++;

            game_update_gameover(&game);

            if (game.play_state == GAME_PLAYING) {
                int prev_anim_changed = (game.animation_counter % FRAMES_PER_UPDATE == 0);
                if (game_update_animation(&game)) {
                    flags |= F_ANIM;
                }
                int now_anim_changed = (game.animation_counter % FRAMES_PER_UPDATE == 0);
                if (prev_anim_changed || now_anim_changed) {
                    flags |= F_ANIM;
                }

                game_check_pea_zombie_collision(&game);
            }

            int prev_peas = game.num_active_peas;
            game_update_peas(&game);
            if (game.num_active_peas || prev_peas) {
                flags |= F_PEA;
            }

            int prev_suns = game.num_active_suns;
            game_update_suns(&game);
            if (game.num_active_suns || prev_suns) {
                flags |= F_SUN;
            }

            int prev_zombies = game.num_active_zombies;
            game_update_zombies(&game);
            if (game.num_active_zombies || prev_zombies) {
                flags |= F_ZOMBIE;
            }
        }

        // ===== STEP 3: Process touch events =====
        TouchEvent ev;
        while (tq_pop(&ev)) {
            if (ev.is_down) {
                has_down = 1;
                down_x = ev.x;
                down_y = ev.y;
            }
            else {
                if (has_down) {
                    int prev_sun = game.sun_count;
                    int prev_card = game.selected_card;

                    int sun_clicked = game_check_sun_click(&game, down_x, down_y);

                    if (sun_clicked) {
                        flags |= F_FULL;
                    }
                    else {
                        game_handle_touch(&game, down_x, down_y);

                        if (game.sun_count != prev_sun || game.selected_card != prev_card) {
                            flags |= F_FULL;
                        }
                    }
                }
                has_down = 0;
            }
        }

        // ===== STEP 4: Render to back buffer =====
        // Get pointer to the buffer we'll render to (NOT currently displayed)
        u8 *fb = (u8 *)DispCtrl_Inst.framePtr[next_render];

        // Single flag: do we need to present this frame?
        int need_present = 0;

        if (game.play_state == GAME_PLAYING) {
            if (prev_play_state != GAME_PLAYING) {
                set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, PWM_duty);

                // Full redraw to clear defeat image
                memcpy(fb, gImage_background1_hd, DEMO_MAX_FRAME);
                game_draw_full(&game, fb);
                game_draw_suns(&game, fb);
                game_draw_peas(&game, fb);
                game_draw_zombies(&game, fb);

                need_present = 1;
                prev_play_state = GAME_PLAYING;
                fade_needs_black_transition = 0;
            }
            else if (flags & F_FULL) {
                memcpy(fb, gImage_background1_hd, DEMO_MAX_FRAME);
                game_draw_full(&game, fb);
                game_draw_suns(&game, fb);
                game_draw_peas(&game, fb);
                game_draw_zombies(&game, fb);

                need_present = 1;
            }
            else if (flags) {
                // Incremental: copy from currently displayed buffer
                memcpy(fb, (u8 *)DispCtrl_Inst.framePtr[current_displayed], DEMO_MAX_FRAME);

                if (flags & F_ANIM)   game_draw_animation(&game, fb);
                if (flags & F_SUN)    game_draw_suns(&game, fb);
                if (flags & F_PEA)    game_draw_peas(&game, fb);
                if (flags & F_ZOMBIE) game_draw_zombies(&game, fb);

                need_present = 1;
            }
        }
        else if (game.play_state == GAME_FADING_TO_BLACK) {
            if (prev_play_state != GAME_FADING_TO_BLACK) {
                memcpy(fb, gImage_background1_hd, DEMO_MAX_FRAME);
                game_draw_full(&game, fb);
                game_draw_suns(&game, fb);
                game_draw_peas(&game, fb);
                game_draw_zombies(&game, fb);

                prev_play_state = GAME_FADING_TO_BLACK;
                fade_needs_black_transition = 1;
                need_present = 1;
            }
            else if (flags & F_ZOMBIE) {
                // Copy from current display, update zombies
                memcpy(fb, (u8 *)DispCtrl_Inst.framePtr[current_displayed], DEMO_MAX_FRAME);
                game_draw_zombies(&game, fb);
                need_present = 1;
            }

            // PWM fade (runs every frame during fade state)
            float duty = PWM_duty + (1.0f - PWM_duty) * game.fade_progress;
            if (duty > 1.0f) duty = 1.0f;
            set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, duty);

            // Transition to black screen when fade completes
            if (game.fade_progress >= 0.99f && fade_needs_black_transition) {
                // This will be shown in the NEXT frame after zombies
                fade_needs_black_transition = 0;
                // Don't render black here - let it happen in next loop iteration
                // when state transitions to GAME_SHOWING_DEFEAT
            }
        }
        else if (game.play_state == GAME_SHOWING_DEFEAT) {
            if (prev_play_state != GAME_SHOWING_DEFEAT) {
                set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, PWM_duty);

                // Fill black first
                game_fill_black(fb);
                prev_play_state = GAME_SHOWING_DEFEAT;
                need_present = 1;
            }
            else {
                // Animate defeat image scaling
                // Copy current (black), then draw defeat
                memcpy(fb, (u8 *)DispCtrl_Inst.framePtr[current_displayed], DEMO_MAX_FRAME);
                game_draw_defeat_image(fb, game.defeat_scale);
                need_present = 1;
            }
        }
        else if (game.play_state == GAME_RESTARTING) {
            if (prev_play_state != GAME_RESTARTING) {
                prev_play_state = GAME_RESTARTING;

                game_fill_black(fb);
                game_draw_defeat_image(fb, 1.0f);
                need_present = 1;
            }
        }

        // ===== STEP 5: VSYNC-synchronized present (ONLY ONCE PER LOOP) =====
        if (need_present) {
            /* Flush cache for the buffer we just rendered */
            Xil_DCacheFlushRange((UINTPTR)fb, DEMO_MAX_FRAME);

            /* CRITICAL FIX: Proper order for tear-free display
             * 1. Wait for current frame to finish scanning
             * 2. Switch to new frame at frame boundary
             */
            present_frame_at_vsync(next_render);

            /* Update indices for next iteration */
            current_displayed = next_render;
            next_render = (next_render + 1) % DISPLAY_NUM_FRAMES;
        }
    }

    return 0;
}

/**
 * VDMA interrupt initialization
 */
int VDMA_Interrupt_Init(XAxiVdma *VdmaPtr, XScuGic *IntcPtr)
{
    int Status;
    u32 vdma_intr_id;

    // Set up interrupt handler for VDMA MM2S (Read) channel
    XAxiVdma_SetCallBack(VdmaPtr, XAXIVDMA_HANDLER_GENERAL,
                         VdmaReadIntrHandler, (void *)VdmaPtr, XAXIVDMA_READ);

    // Enable frame count interrupt (fires when frame completes)
    XAxiVdma_IntrEnable(VdmaPtr, XAXIVDMA_IXR_FRMCNT_MASK, XAXIVDMA_READ);

    /* Try to find the correct interrupt ID */
    #if defined(XPAR_FABRIC_AXI_VDMA_0_MM2S_INTROUT_INTR)
        vdma_intr_id = XPAR_FABRIC_AXI_VDMA_0_MM2S_INTROUT_INTR;
    #elif defined(XPAR_FABRIC_AXIVDMA_0_MM2S_INTROUT_INTR)
        vdma_intr_id = XPAR_FABRIC_AXIVDMA_0_MM2S_INTROUT_INTR;
    #elif defined(XPAR_FABRIC_AXIVDMA_0_MM2S_INTROUT_VEC_ID)
        vdma_intr_id = XPAR_FABRIC_AXIVDMA_0_MM2S_INTROUT_VEC_ID;
    #else
        #error "Cannot find VDMA MM2S interrupt ID! Check xparameters.h"
        return XST_FAILURE;
    #endif

    printf("INFO: Using VDMA interrupt ID: %d\n", vdma_intr_id);

    // Connect VDMA interrupt to GIC
    Status = XScuGic_Connect(IntcPtr, vdma_intr_id,
                             (Xil_InterruptHandler)XAxiVdma_ReadIntrHandler,
                             VdmaPtr);
    if (Status != XST_SUCCESS) {
        printf("ERROR: Failed to connect VDMA interrupt\n");
        return XST_FAILURE;
    }

    // Enable VDMA interrupt in GIC
    XScuGic_Enable(IntcPtr, vdma_intr_id);

    return XST_SUCCESS;
}

/**
 * GIC initialization
 */
int GIC_Init(u16 DeviceId, XScuGic *XScuGicInstancePtr)
{
    XScuGic_Config *IntcConfig;

    IntcConfig = XScuGic_LookupConfig(DeviceId);
    XScuGic_CfgInitialize(XScuGicInstancePtr, IntcConfig,
                          IntcConfig->CpuBaseAddress);

    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
        (Xil_ExceptionHandler)XScuGic_InterruptHandler, XScuGicInstancePtr);
    Xil_ExceptionEnable();

    return XST_SUCCESS;
}

/**
 * CPU private timer initialization
 */
int PS_timer_init(XScuTimer *Timer, u16 DeviceId, u32 timer_load)
{
    XScuTimer_Config *TMRConfigPtr;

    TMRConfigPtr = XScuTimer_LookupConfig(DeviceId);
    XScuTimer_CfgInitialize(Timer, TMRConfigPtr, TMRConfigPtr->BaseAddr);
    XScuTimer_LoadTimer(Timer, timer_load);
    XScuTimer_EnableAutoReload(Timer);
    XScuTimer_Start(Timer);

    XScuGic_Connect(&GIC_Inst, XPAR_SCUTIMER_INTR,
        (Xil_InterruptHandler)Timer_IRQ_Handler, (void *)&Timer_Inst);
    XScuGic_Enable(&GIC_Inst, XPAR_SCUTIMER_INTR);
    XScuTimer_EnableInterrupt(Timer);

    return XST_SUCCESS;
}

/**
 * Timer interrupt handler
 */
static void Timer_IRQ_Handler(void *CallBackRef)
{
    XScuTimer *TimerInstancePtr = (XScuTimer *)CallBackRef;
    XScuTimer_ClearInterruptStatus(TimerInstancePtr);
    g_tick++;
}
