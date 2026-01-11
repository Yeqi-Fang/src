#include "xil_types.h"
#include "xscugic.h"
#include "xiicps.h"
#include "xparameters.h"
#include "xil_exception.h"
#ifndef TOUCH_H_
#define TOUCH_H_

int touch_interrupt_init (XIicPs *pIicIns,XScuGic *XScuGicInstancePtr,u32 Int_Id);
int touch_i2c_read_bytes(u8 *BufferPtr, u8 address, u16 ByteCount);

u8 touch_sig;

#endif /* TOUCH_H_ */
