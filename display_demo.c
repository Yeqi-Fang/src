///* ------------------------------------------------------------ */
///*       PVZ Game - HD 480p Version (800×480)                  */
///* ------------------------------------------------------------ */
//#include "display_demo.h"
//#include "display_ctrl/display_ctrl.h"
//#include "i2c/PS_i2c.h"
//#include <stdio.h>
//#include "xil_types.h"
//#include "xil_cache.h"
//#include "xparameters.h"
//#include "ugui.h"
//#include "xscutimer.h"
//#include "xscugic.h"
//#include "xil_exception.h"
//#include "ax_pwm.h"
//#include "background1_hd.h"  // ⭐ 使用800×480的背景图片
//
//// 参数定义 - 兼容不同的xparameters命名
//#ifdef XPAR_AXI_DYNCLK_0_BASEADDR
//    #define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_BASEADDR
//#endif
//
//#ifdef XPAR_AXI_VDMA_0_DEVICE_ID
//    #define VGA_VDMA_ID XPAR_AXI_VDMA_0_DEVICE_ID
//#elif defined(XPAR_AXIVDMA_0_DEVICE_ID)
//    #define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
//#endif
//
//#ifdef XPAR_V_TC_0_DEVICE_ID
//    #define DISP_VTC_ID XPAR_V_TC_0_DEVICE_ID
//#elif defined(XPAR_VTC_0_DEVICE_ID)
//    #define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
//#endif
//
//// 实例声明
//DisplayCtrl DispCtrl_Inst;
//XAxiVdma VDMA_Inst;
//XIicPs I2C0_Inst;
//XScuGic GIC_Inst;
//XScuTimer Timer_Inst;
//
//// 帧缓存 - 800×480×3 = 2,764,800 字节
//u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];
//
//// GUI
//UG_GUI gui;
//float PWM_duty;
//
//// 函数声明
//void FrameBuf_pixel_set(UG_S16 x, UG_S16 y, UG_COLOR c);
//void draw_background_hd(void);
//void draw_test_pattern_hd(void);
//int PS_timer_init(XScuTimer *Timer, u16 DeviceId, u32 timer_load);
//int GIC_Init(u16 DeviceId, XScuGic *XScuGicInstancePtr);
//static void Timer_IRQ_Handler(void *CallBackRef);
//
//int main2(void)
//{
//    printf("\nSTRIDE=%d (should be 3840)\n\n", DEMO_STRIDE);
//
//    XAxiVdma_Config *vdmaConfig;
//    u8 *pFrames[DISPLAY_NUM_FRAMES];
//    int i;
//
//    printf("\n");
//    printf("========================================\n");
//    printf("  PVZ Game - HD Version\n");
//    printf("  Resolution: 800×480 (HD 480p)\n");
//    printf("========================================\n\n");
//
//    // 初始化帧缓存指针
//    for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
//        pFrames[i] = frameBuf[i];
//
//    printf("配置信息:\n");
//    printf("  SCREEN_WIDTH  = %d\n", SCREEN_WIDTH);
//    printf("  SCREEN_HEIGHT = %d\n", SCREEN_HEIGHT);
//    printf("  DEMO_STRIDE   = %d 字节\n", DEMO_STRIDE);
//    printf("  DEMO_MAX_FRAME= %d 字节 (%.1f MB)\n",
//           DEMO_MAX_FRAME, DEMO_MAX_FRAME / 1024.0 / 1024.0);
//    printf("\n");
//
//    // 设置PWM，控制LCD背光
//    PWM_duty = 0.7;
//    #ifdef XPAR_AX_PWM_0_S00_AXI_BASEADDR
//        set_pwm_frequency(XPAR_AX_PWM_0_S00_AXI_BASEADDR, 100000000, 200);
//        set_pwm_duty(XPAR_AX_PWM_0_S00_AXI_BASEADDR, PWM_duty);
//        printf("✓ PWM背光已设置 (%.0f%%)\n", PWM_duty * 100);
//    #endif
//
//    // GIC初始化
//    #ifdef XPAR_SCUGIC_0_DEVICE_ID
//        GIC_Init(XPAR_SCUGIC_0_DEVICE_ID, &GIC_Inst);
//        printf("✓ GIC初始化完成\n");
//    #endif
//
//    // I2C接口初始化
//    #ifdef XPAR_XIICPS_0_DEVICE_ID
//        i2c_init(&I2C0_Inst, XPAR_XIICPS_0_DEVICE_ID, 100000);
//        printf("✓ I2C初始化完成\n");
//    #endif
//
//    // CPU私有定时器初始化
//    #ifdef XPAR_XSCUTIMER_0_DEVICE_ID
//        PS_timer_init(&Timer_Inst, XPAR_XSCUTIMER_0_DEVICE_ID,
//                      (u32)(XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 200 - 1));
//        printf("✓ Timer初始化完成\n");
//    #endif
//
//    // VDMA实例初始化
//    vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
//    XAxiVdma_CfgInitialize(&VDMA_Inst, vdmaConfig, vdmaConfig->BaseAddress);
//    printf("✓ VDMA初始化完成\n");
//
//    // 初始化VMDA、VTC、DynClk等模块
//    DisplayInitialize(&DispCtrl_Inst, &VDMA_Inst, DISP_VTC_ID,
//                      DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
//    DisplayStart(&DispCtrl_Inst);
//    printf("✓ Display Controller启动完成\n");
//
//    // uGUI初始化 - 使用800×480
//    UG_Init(&gui, FrameBuf_pixel_set, 800, 480);
//    printf("✓ uGUI初始化完成 (800×480)\n\n");
//
//    // 先显示测试图案（5秒）
//    printf("显示测试图案（彩色条纹）...\n");
//    draw_test_pattern_hd();
//    Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame],
//                         DEMO_MAX_FRAME);
//
//    // 简单延时
//    for (volatile int d = 0; d < 50000000; d++);
//
//    // 绘制背景图片
//    printf("\n正在绘制草坪背景 (800×480)...\n");
//    draw_background_hd();
//
//    Xil_DCacheFlushRange((unsigned int)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame],
//                         DEMO_MAX_FRAME);
//
//    printf("✓ 背景绘制完成！\n\n");
//    printf("========================================\n");
//    printf("PVZ游戏 - 第一步完成\n");
//    printf("屏幕应该显示完整的草坪背景了！\n");
//    printf("========================================\n\n");
//
//    // 主循环
//    while(1);
//    return 0;
//}
//
///**
// * 绘制HD背景图片 (800×480)
// */
//void draw_background_hd(void)
//{
//    int x, y;
//    u32 pixel_idx = 0;
//    u8 *fb = (u8 *)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame];
//    u32 fb_idx = 0;
//
//    // 遍历每个像素
//    for (y = 0; y < 480; y++) {
//        for (x = 0; x < 800; x++) {
//            // 从图片数据读取BGR
//            fb[fb_idx++] = gImage_background1_hd[pixel_idx++];  // Blue
//            fb[fb_idx++] = gImage_background1_hd[pixel_idx++];  // Green
//            fb[fb_idx++] = gImage_background1_hd[pixel_idx++];  // Red
//        }
//
//        // 每100行打印进度
//        if ((y + 1) % 100 == 0) {
//            printf("  进度: %d/480 (%.1f%%)\n", y + 1, (y + 1) * 100.0 / 480);
//        }
//    }
//}
//
///**
// * 绘制测试图案 - HD版本
// */
//void draw_test_pattern_hd(void)
//{
//    int x, y;
//    UG_COLOR colors[8] = {
//        0xFFFFFF,  // 白
//        0xFFFF00,  // 黄
//        0x00FFFF,  // 青
//        0x00FF00,  // 绿
//        0xFF00FF,  // 品红
//        0xFF0000,  // 红
//        0x0000FF,  // 蓝
//        0x000000   // 黑
//    };
//
//    int bar_width = 800 / 8;  // 每条160像素宽
//
//    for (y = 0; y < 480; y++) {
//        for (x = 0; x < 800; x++) {
//            int color_idx = x / bar_width;
//            if (color_idx >= 8) color_idx = 7;
//
//            FrameBuf_pixel_set(x, y, colors[color_idx]);
//        }
//    }
//
//    printf("  测试图案已绘制 (8色条纹)\n");
//    printf("  如果能看到完整的8条彩色条纹，说明STRIDE配置正确\n");
//}
//
///**
// * 像素绘制函数 - 支持800×480
// */
//void FrameBuf_pixel_set(UG_S16 x, UG_S16 y, UG_COLOR c)
//{
//    u32 iPixelAddr;
//    u8 *cpFrames;
//
//    // 边界检查
//    if (x < 0 || x >= 800 || y < 0 || y >= 480)
//        return;
//
//    cpFrames = (u8 *)DispCtrl_Inst.framePtr[DispCtrl_Inst.curFrame];
//
//    // 地址计算：800像素/行 × 3字节/像素 = 3840字节/行
//    iPixelAddr = (y * 800 + x) * 3;
//
//    cpFrames[iPixelAddr]     = c;        // Blue
//    cpFrames[iPixelAddr + 1] = c >> 8;   // Green
//    cpFrames[iPixelAddr + 2] = c >> 16;  // Red
//}
//
///**
// * GIC初始化
// */
//int GIC_Init(u16 DeviceId, XScuGic *XScuGicInstancePtr)
//{
//    XScuGic_Config *IntcConfig;
//
//    IntcConfig = XScuGic_LookupConfig(DeviceId);
//    XScuGic_CfgInitialize(XScuGicInstancePtr, IntcConfig,
//                          IntcConfig->CpuBaseAddress);
//
//    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
//        (Xil_ExceptionHandler)XScuGic_InterruptHandler, XScuGicInstancePtr);
//    Xil_ExceptionEnable();
//
//    return XST_SUCCESS;
//}
//
///**
// * CPU私有定时器初始化
// */
//int PS_timer_init(XScuTimer *Timer, u16 DeviceId, u32 timer_load)
//{
//    XScuTimer_Config *TMRConfigPtr;
//
//    TMRConfigPtr = XScuTimer_LookupConfig(DeviceId);
//    XScuTimer_CfgInitialize(Timer, TMRConfigPtr, TMRConfigPtr->BaseAddr);
//    XScuTimer_LoadTimer(Timer, timer_load);
//    XScuTimer_EnableAutoReload(Timer);
//    XScuTimer_Start(Timer);
//
//    #ifdef XPAR_SCUTIMER_INTR
//        XScuGic_Connect(&GIC_Inst, XPAR_SCUTIMER_INTR,
//            (Xil_InterruptHandler)Timer_IRQ_Handler, (void *)&Timer_Inst);
//        XScuGic_Enable(&GIC_Inst, XPAR_SCUTIMER_INTR);
//        XScuTimer_EnableInterrupt(Timer);
//    #endif
//
//    return XST_SUCCESS;
//}
//
///**
// * CPU私有定时器中断处理函数
// */
//static void Timer_IRQ_Handler(void *CallBackRef)
//{
//    XScuTimer *TimerInstancePtr = (XScuTimer *)CallBackRef;
//    XScuTimer_ClearInterruptStatus(TimerInstancePtr);
//    XScuTimer_DisableInterrupt(TimerInstancePtr);
//
//    // TODO: 游戏逻辑更新
//
//    XScuTimer_EnableInterrupt(TimerInstancePtr);
//}
//
