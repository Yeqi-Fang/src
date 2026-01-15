#ifndef DISPLAY_DEMO_H_
#define DISPLAY_DEMO_H_

#include "xil_types.h"

#define SCREEN_WIDTH            800    // 修改这里！
#define SCREEN_HEIGHT           480     // 修改这里！

// 帧缓存参数
#define DEMO_MAX_FRAME          (800 * 480 * 3)    // = 2,764,800 字节
#define DEMO_STRIDE             (800 * 3)           // = 3,840 字节

#define DISPLAY_NUM_FRAMES      2

#endif
