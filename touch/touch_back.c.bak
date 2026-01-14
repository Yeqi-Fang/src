#include "touch.h"
#include "xparameters.h"
#include "../i2c/PS_i2c.h"

// Parameter definitions
#define IIC_DEVICE_ID	XPAR_XIICPS_0_DEVICE_ID
#define TOUCH_ADDRESS 	0x38	/* 0xA0 as an 8 bit number. */
//u8 WriteBuffer[20];	/* Read buffer for reading a page. */
XIicPs *pIicInstance;

//触摸屏中断服务子程序
void Touch_Intr_Handler(void *InstancePtr)
{
	//设置如下全局变量为1，表明有触摸动作。
	//进一步的触摸动作处理，在CPU私有定时器中断子程序中进行。
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
	return XST_SUCCESS;
}




