/* ------------------------------------------------------------ */
/*       PVZ Game - Main Program                                */
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

// Parameter redefinition
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID

// Instance declaration
DisplayCtrl DispCtrl_Inst;
XAxiVdma VDMA_Inst;
XIicPs I2C0_Inst;
XScuGic GIC_Inst;
XScuTimer Timer_Inst;

// Frame buffer
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];

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
    printf("  Plants vs Zombies - PVZ Game\n");
    printf("  Resolution: 800x480\n");
    printf("========================================\n\n");

    // Initialize frame buffer pointers
    for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
        pFrames[i] = frameBuf[i];

    // Set PWM, control LCD backlight
    PWM_duty = 0.7;
    set_pwm_frequency(XPAR_AX_PWM_0_S00_AXI_BASEADDR, 100000000, 200);
    set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, PWM_duty);

    // GIC initialization
    GIC_Init(XPAR_XSCUTIMER_0_DEVICE_ID, &GIC_Inst);

    // I2C interface initialization
    i2c_init(&I2C0_Inst, XPAR_XIICPS_0_DEVICE_ID, 100000);

    // Touch screen interrupt initialization
    touch_interrupt_init(&I2C0_Inst, &GIC_Inst, 63);

    // CPU private timer initialization (10ms period)
    PS_timer_init(&Timer_Inst, XPAR_XSCUTIMER_0_DEVICE_ID,
                  (u32)(XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 200 - 1));

    // VDMA instance initialization
    vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
    XAxiVdma_CfgInitialize(&VDMA_Inst, vdmaConfig, vdmaConfig->BaseAddress);

    // Initialize VMDA, VTC, DynClk modules
    DisplayInitialize(&DispCtrl_Inst, &VDMA_Inst, DISP_VTC_ID,
                      DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
    DisplayStart(&DispCtrl_Inst);

    printf("Display system initialized\n");

    // Initialize game
    game_init(&game);

    // Draw initial interface
    game_draw(&game, (u8 *)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame]);
    Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame],
                         DEMO_MAX_FRAME);

    printf("\n========================================\n");
    printf("Game started successfully!\n");
    printf("- Click plant card to select\n");
    printf("- Click lawn grid to plant\n");
    printf("========================================\n\n");

    // Main loop
    while (1);
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
 * CPU private timer interrupt handler
 */
static void Timer_IRQ_Handler(void *CallBackRef)
{
    u8 ReadBuffer[50];
    int current_x1, current_y1;
    static u8 touch_released = 1;  // Prevent repeated trigger
    static u8 need_redraw = 0;     // Flag to control redrawing

    XScuTimer *TimerInstancePtr = (XScuTimer *)CallBackRef;
    XScuTimer_ClearInterruptStatus(TimerInstancePtr);
    XScuTimer_DisableInterrupt(TimerInstancePtr);

    // Update animation (returns 1 if frame changed)
    if (game_update_animation(&game)) {
        need_redraw = 1;
    }

    // Check touch event
    if (touch_sig == 1) {
        touch_i2c_read_bytes(ReadBuffer, 0, 20);
        touch_sig = 0;

        if ((ReadBuffer[3] & 0xc0) == 0x00) {
            // Touch screen pressed
            current_x1 = ((ReadBuffer[3] & 0x3f) << 8) + ReadBuffer[4];
            current_y1 = ((ReadBuffer[5] & 0x3f) << 8) + ReadBuffer[6];

            if (touch_released) {
                // Handle game logic
                game_handle_touch(&game, current_x1, current_y1);

                // Mark need redraw
                need_redraw = 1;

                touch_released = 0;
            }
        }
        else if ((ReadBuffer[3] & 0xc0) == 0x40) {
            // Touch screen released
            touch_released = 1;
        }
    }

    // Only redraw when necessary
    if (need_redraw) {
        game_draw(&game, (u8 *)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame]);

        // Refresh display
        Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame],
                             DEMO_MAX_FRAME);

        need_redraw = 0;
    }

    XScuTimer_EnableInterrupt(TimerInstancePtr);
}
