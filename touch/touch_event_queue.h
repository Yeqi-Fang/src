/* ============================================================ */
/*                Touch Event Queue Header                      */
/*   Lock-free ring buffer for touch event handling            */
/* ============================================================ */

#ifndef TOUCH_EVENT_QUEUE_H
#define TOUCH_EVENT_QUEUE_H

#include "xil_types.h"

/* ============================================================ */
/*                      Type Definitions                        */
/* ============================================================ */

/**
 * Touch event structure
 */
typedef struct {
    u16 x;          // Touch X coordinate
    u16 y;          // Touch Y coordinate
    u8  is_down;    // 1 = press/contact, 0 = release
} TouchEvent;

/* ============================================================ */
/*                      Configuration                           */
/* ============================================================ */

#define TQ_SIZE 16  // Must be power of 2

/* ============================================================ */
/*              External Declarations (for touch.c)             */
/* ============================================================ */

// 队列变量（在 main.c 中定义）
extern volatile TouchEvent tq[TQ_SIZE];
extern volatile u8 tq_w;
extern volatile u8 tq_r;

// 队列函数声明
void tq_push(u16 x, u16 y, u8 is_down);
int tq_pop(TouchEvent *e);

#endif // TOUCH_EVENT_QUEUE_H
