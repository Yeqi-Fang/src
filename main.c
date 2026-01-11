/* ------------------------------------------------------------ */
/*       PVZ Game - Main Program (Optimized Version)           */
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

// Frame buffers
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];

// Redraw control flags
volatile u8 global_need_full_redraw = 0;      // Set when planting or UI changes
volatile u8 global_need_animation_redraw = 0; // Set when only animation updates

// Game state
GameState game;

// PWM duty cycle
float PWM_duty;

// Function declarations
int PS_timer_init(XScuTimer *Timer, u16 DeviceId, u32 timer_load);
int GIC_Init(u16 DeviceId, XScuGic *XScuGicInstancePtr);
static void Timer_IRQ_Handler(void *CallBackRef);

int main(void)
{
    XAxiVdma_Config *vdmaConfig;
    u8 *pFrames[DISPLAY_NUM_FRAMES];
    int i;

    printf("\n========================================\n");
    printf("  Plants vs Zombies - Optimized Version\n");
    printf("  Resolution: 800x480\n");
    printf("========================================\n\n");

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

    // CPU private timer initialization (10ms period, 100Hz)
    PS_timer_init(&Timer_Inst, XPAR_XSCUTIMER_0_DEVICE_ID,
                  (u32)(XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 200 - 1));

    // VDMA instance initialization
    vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
    XAxiVdma_CfgInitialize(&VDMA_Inst, vdmaConfig, vdmaConfig->BaseAddress);

    // Initialize VDMA, VTC, DynClk modules
    DisplayInitialize(&DispCtrl_Inst, &VDMA_Inst, DISP_VTC_ID,
                      DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
    DisplayStart(&DispCtrl_Inst);

    printf("Display system initialized\n");
    printf("Double buffering: %s\n", DISPLAY_NUM_FRAMES >= 2 ? "ENABLED" : "DISABLED");

    // Initialize game
    game_init(&game);

    // Initial full draw to all buffers (CRITICAL for triple buffering)
    for (i = 0; i < DISPLAY_NUM_FRAMES; i++) {
        // Copy complete background
        memcpy(DispCtrl_Inst.framePtr[i], gImage_background1_hd, DEMO_MAX_FRAME);

        // Draw initial game state (UI + plants)
        game_draw_full(&game, (u8 *)DispCtrl_Inst.framePtr[i]);

        // Flush cache
        Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[i], DEMO_MAX_FRAME);
    }

    printf("\n========================================\n");
    printf("Game started successfully!\n");
    printf("- Click plant card to select\n");
    printf("- Click lawn grid to plant\n");
    printf("OPTIMIZED: Only plants redraw on animation\n");
    printf("========================================\n\n");

    // Main loop
    while (1) {
        // Check if full redraw is needed (planting, UI changes)
        if (global_need_full_redraw) {
            global_need_full_redraw = 0;
            global_need_animation_redraw = 0; // Clear animation flag too

            // CRITICAL FIX: Update ALL buffers when UI changes
            // With triple buffering, if we only update one buffer, when we rotate
            // back to old buffers they will have stale UI (old sun count, wrong card selection)
            for (i = 0; i < DISPLAY_NUM_FRAMES; i++) {
                memcpy(DispCtrl_Inst.framePtr[i], gImage_background1_hd, DEMO_MAX_FRAME);
                game_draw_full(&game, (u8 *)DispCtrl_Inst.framePtr[i]);
                Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[i], DEMO_MAX_FRAME);
            }

            // Switch to next frame for display
            u32 draw_frame = (DispCtrl_Inst.curFrame + 1) % DISPLAY_NUM_FRAMES;
            XAxiVdma_StartParking(&VDMA_Inst, draw_frame, XAXIVDMA_READ);
            DispCtrl_Inst.curFrame = draw_frame;
        }
        // Check if only animation redraw is needed
        else if (global_need_animation_redraw) {
            global_need_animation_redraw = 0;

            // Calculate next write frame
            u32 draw_frame = (DispCtrl_Inst.curFrame + 1) % DISPLAY_NUM_FRAMES;

            // OPTIMIZED: Only redraw plant cells (no background copy, no UI redraw)
            // This is much faster - only updates 5-10 small areas instead of full screen
            game_draw_animation(&game, (u8 *)DispCtrl_Inst.framePtr[draw_frame]);

            // Flush cache and switch display
            Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[draw_frame], DEMO_MAX_FRAME);
            XAxiVdma_StartParking(&VDMA_Inst, draw_frame, XAXIVDMA_READ);
            DispCtrl_Inst.curFrame = draw_frame;
        }
    }

    return 0;
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
 * Optimized timer interrupt handler
 * Updates game logic and sets appropriate redraw flags
 */
static void Timer_IRQ_Handler(void *CallBackRef)
{
    u8 ReadBuffer[50];
    int current_x1, current_y1;
    static u8 touch_released = 1;
    int prev_sun, prev_card;

    XScuTimer *TimerInstancePtr = (XScuTimer *)CallBackRef;
    XScuTimer_ClearInterruptStatus(TimerInstancePtr);

    // Update animation logic first
    if (game_update_animation(&game)) {
        // Animation frame changed - set flag for optimized redraw
        global_need_animation_redraw = 1;
    }

    // Handle touch input
    if (touch_sig == 1) {
        // Store UI state BEFORE handling touch
        prev_sun = game.sun_count;
        prev_card = game.selected_card;

        touch_i2c_read_bytes(ReadBuffer, 0, 20);
        touch_sig = 0;

        if ((ReadBuffer[3] & 0xc0) == 0x00) {
            // Touch pressed
            current_x1 = ((ReadBuffer[3] & 0x3f) << 8) + ReadBuffer[4];
            current_y1 = ((ReadBuffer[5] & 0x3f) << 8) + ReadBuffer[6];

            if (touch_released) {
                // Handle game logic (card selection, planting)
                game_handle_touch(&game, current_x1, current_y1);

                // Check if UI state changed AFTER handling touch
                if (game.sun_count != prev_sun || game.selected_card != prev_card) {
                    // UI changed - need full redraw of all buffers
                    global_need_full_redraw = 1;
                    // Full redraw takes priority, clear animation flag
                    global_need_animation_redraw = 0;
                }

                touch_released = 0;
            }
        }
        else if ((ReadBuffer[3] & 0xc0) == 0x40) {
            // Touch released
            touch_released = 1;
        }
    }
}
