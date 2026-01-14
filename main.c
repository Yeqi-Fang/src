/* ------------------------------------------------------------ */
/*   PVZ Game - Main Program (Touch REALLY Fixed)              */
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

// Frame buffers
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];

// Global tick counter (incremented by Timer ISR)
volatile u32 g_tick = 0;

// Game state
GameState game;

// PWM duty cycle
float PWM_duty;

/* ============================================================ */
/*         Touch Event Queue Implementation (定义在这里)        */
/* ============================================================ */

// 队列变量定义（只在 main.c 中定义一次）
volatile TouchEvent tq[TQ_SIZE];
volatile u8 tq_w = 0;
volatile u8 tq_r = 0;

/**
 * Push touch event to queue
 * Called from Touch ISR (in touch.c)
 */
void tq_push(u16 x, u16 y, u8 is_down)
{
    u8 next = (tq_w + 1) & (TQ_SIZE - 1);
    if (next == tq_r) return;  // Queue full

    tq[tq_w].x = x;
    tq[tq_w].y = y;
    tq[tq_w].is_down = is_down;

    // Memory barrier
    __asm__ volatile("dmb sy" ::: "memory");

    tq_w = next;
}

/**
 * Pop touch event from queue
 * Called from main loop
 */
int tq_pop(TouchEvent *e)
{
    if (tq_r == tq_w) return 0;  // Queue empty

    *e = tq[tq_r];

    // Memory barrier
    __asm__ volatile("dmb sy" ::: "memory");

    tq_r = (tq_r + 1) & (TQ_SIZE - 1);
    return 1;
}

/* ============================================================ */

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
    printf("  Plants vs Zombies - Touch REALLY Fixed\n");
    printf("  Resolution: 800x480\n");
    printf("  Touch: Single Queue Architecture\n");
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

    printf("INFO: Touch queue initialized (single instance)\n");

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

    // Initialize game
    game_init(&game);

    // Get framebuffer pointer (single buffer)
    u8 *fb = (u8 *)DispCtrl_Inst.framePtr[0];

    // Initial full draw
    memcpy(fb, gImage_background1_hd, DEMO_MAX_FRAME);
    game_draw_full(&game, fb);
    Xil_DCacheFlushRange((unsigned int)fb, DEMO_MAX_FRAME);

    printf("\n========================================\n");
    printf("Game started successfully!\n");
    printf("Touch should work now!\n");
    printf("========================================\n\n");

    // Touch state machine
    int has_down = 0;
    u16 down_x = 0, down_y = 0;

    // Tick accumulator
    u32 tick_accum = 0;

    // Debug counters
    u32 push_count = 0, pop_count = 0;

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

            if (game_update_animation(&game)) {
                flags |= F_ANIM;
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
            pop_count++;

            // Debug: print every 10 events
            if (pop_count % 10 == 0) {
                printf("[Main] Popped %u events (w=%u, r=%u)\n",
                       pop_count, tq_w, tq_r);
            }

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

        // ===== STEP 4: Render =====
        if (flags & F_FULL) {
            memcpy(fb, gImage_background1_hd, DEMO_MAX_FRAME);
            game_draw_full(&game, fb);
            game_draw_suns(&game, fb);
            game_draw_peas(&game, fb);
            game_draw_zombies(&game, fb);
            Xil_DCacheFlushRange((unsigned int)fb, DEMO_MAX_FRAME);
        }
        else if (flags) {
            if (flags & F_ANIM)   game_draw_animation(&game, fb);
            if (flags & F_SUN)    game_draw_suns(&game, fb);
            if (flags & F_PEA)    game_draw_peas(&game, fb);
            if (flags & F_ZOMBIE) game_draw_zombies(&game, fb);
            Xil_DCacheFlushRange((unsigned int)fb, DEMO_MAX_FRAME);
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
 * Timer interrupt handler
 */
static void Timer_IRQ_Handler(void *CallBackRef)
{
    XScuTimer *TimerInstancePtr = (XScuTimer *)CallBackRef;
    XScuTimer_ClearInterruptStatus(TimerInstancePtr);
    g_tick++;
}
