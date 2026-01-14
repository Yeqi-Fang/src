#include "touch.h"
#include "xparameters.h"
#include "../i2c/PS_i2c.h"
#include "../touch_event_queue.h"  // 使用 extern 声明

// Parameter definitions
#define IIC_DEVICE_ID	XPAR_XIICPS_0_DEVICE_ID
#define TOUCH_ADDRESS 	0x38
XIicPs *pIicInstance;

// 调试计数器
static u32 touch_isr_count = 0;

//触摸屏中断服务子程序
void Touch_Intr_Handler(void *InstancePtr)
{
	u8 ReadBuffer[50];
	
	touch_isr_count++;

	// 立即读取触摸数据
	if (touch_i2c_read_bytes(ReadBuffer, 0, 20) == XST_SUCCESS) {
		// 解析触摸状态
		u8 state = (ReadBuffer[3] & 0xC0);
		
		// 只处理有效的按下/松开事件
		if (state == 0x00 || state == 0x40) {
			// 提取坐标
			u16 x = ((ReadBuffer[3] & 0x3F) << 8) | ReadBuffer[4];
			u16 y = ((ReadBuffer[5] & 0x3F) << 8) | ReadBuffer[6];
			
			// 入队事件（调用main.c中定义的函数）
			u8 is_down = (state == 0x00) ? 1 : 0;
			tq_push(x, y, is_down);

			// 调试打印（每10次打印一次）
			if (touch_isr_count % 10 == 0) {
				printf("[ISR] Pushed %u events, state=0x%02X, x=%u, y=%u, down=%u (w=%u, r=%u)\n",
				       touch_isr_count, state, x, y, is_down, tq_w, tq_r);
			}
		}
	}
	
	// 保持兼容性
	touch_sig = 1;
}

//触摸屏触摸信息读取函数
int touch_i2c_read_bytes(u8 *BufferPtr, u8 address, u16 ByteCount)
{
	u8 buf[2];
	buf[0] = address;
	if(i2c_wrtie_bytes(pIicInstance,TOUCH_ADDRESS,buf,1) != XST_SUCCESS)
		return XST_FAILURE;
	if(i2c_read_bytes(pIicInstance,TOUCH_ADDRESS,BufferPtr,ByteCount) != XST_SUCCESS)
			return XST_FAILURE;
	return XST_SUCCESS;
}

//触摸屏中断初始化函数
int touch_interrupt_init (XIicPs *pIicIns,XScuGic *XScuGicInstancePtr,u32 Int_Id)
{
	pIicInstance=pIicIns;
	//关联中断服务子程序
	XScuGic_Connect(XScuGicInstancePtr,Int_Id,(Xil_InterruptHandler)Touch_Intr_Handler,NULL);
	//使能GIC的IRQ_F2P(2)中断
	XScuGic_Enable(XScuGicInstancePtr, Int_Id);

	printf("[Touch] Interrupt handler registered (ISR will use extern queue)\n");

	return XST_SUCCESS;
}
