#include <stdio.h>


#include "gd32f30x.h"
#include "gd32f30x_it.h"
#include "core_cm4.h"

void hard_fault_handler_c(unsigned int * args)
{
	/*
	发生错误了, 此时寄存器信息会被压入栈中
	和芯片的架构有关, 它支持多少寄存器, 这些寄存器的值分别表示什么
	*/
	
	
}

/*SysTick中断服务函数*/
//void SysTick_Handler( void )
//{
//	
//}
unsigned int g_cur_sleep_tick = 0;
void sleep_ms(unsigned int ms)
{
	g_cur_sleep_tick = ms;
    while(g_cur_sleep_tick != 0);
}

void systick_handler_c(unsigned int *arg)
{
	/*
	SysTick_Handler PROC
                EXPORT  SysTick_Handler             [WEAK]
	    IMPORT  systick_handler_c
                TST LR, #4 
                ITE EQ 
                MRSEQ R0, MSP 
                MRSNE R0, PSP 
                B systick_handler_c
				ENDP
	*/

	if (g_cur_sleep_tick > 0)
	{
		g_cur_sleep_tick--;
	}

	/* 调用任务调度器 */
	task_scheduler();
}



/***********************************************************************************
 * 
 * 								PendSV异常实现任务切换
 * 
 ************************************************************************************/
void PendSV_Handler_Test()
{
	printf("\r\nPendSV_Handler\r\n");
	/*实现任务的切换*/
	/*初次进入执行任务A, 再次进入执行任务B*/
	/*这里全部使用MSP, 看看能否实现*/

	/*每个任务都有自己的栈空间*/

	/*进来, 第一件事是保存上一个函数的堆栈到它本身的栈空间, 然后把栈顶设置上一个函数的栈顶*/
	#if 0
	asm("mrs r0, psr");
    asm("msr psr_c, #0x1");
    asm("tst lr, #0x4");
    asm("ite eq");
    asm("mrs r0, psr");

	/*如果在这里直接调用, 最终并没有退出PendSV*/
	queue[cur_task_id]->task_entry(NULL);
	#endif
}


/***********************************************************************************
 * 
 * 								SVC异常实现任务切换
 * 
 ************************************************************************************/

void SVC_Handler_C(unsigned int *svc_args)
{
	uint8_t svc_number, svc_code1;
	uint32_t stacked_r0, stacked_r1, stacked_r2, stacked_r3;
	uint32_t stacked_r12, stacked_lr, stacked_ra, stacked_xpsr;

	svc_code1 = ((char*)svc_args[6])[-1];	//0xDF, SVC指令在CortexM4中的编码
	svc_number = ((char*)svc_args[6])[-2];	//首先它是PC指针, Thumb指令是2字节, 减去2, 对应SVC指令的参数, 减去1就是SVC指令0xDF
	printf("\r\nSVC_Handler_C, SVC number is 0x%02x, 0x%02x\r\n", svc_number, svc_code1);

	stacked_r0 = svc_args[0];	//进来的第一个值是栈顶的值
	stacked_r1 = svc_args[1];
	stacked_r2 = svc_args[2];
	stacked_r3 = svc_args[3];


	stacked_r12 = svc_args[4];
	stacked_lr = svc_args[5];
	stacked_ra = svc_args[6];
	stacked_xpsr = svc_args[7];	//xPSR: 这个寄存器的作用是什么

	printf("\r\nR0[0x%08x], R1[0x%08x], R2[0x%08x], R3[0x%08x], R12[0x%08x], LR[0x%08x], RE[0x%08x], xPSR[0x%08x]\r\n",
		stacked_r0, stacked_r1, stacked_r2, stacked_r3, 
		stacked_r12, stacked_lr, stacked_ra, stacked_xpsr);

	switch(svc_number)
	{
		case 0x01:
			svc_args[0] = stacked_r0 + stacked_r1;// 加法
			break;
		case 0x02:
			svc_args[0] = stacked_r0 - stacked_r1;// 减法
			break;
		case 0x03:
			svc_args[0] = stacked_r0 + 1;		  // 自加
			break;
		case 0x04:
			break;
		default:
			break;
	}

	return;
}

__asm void SVC_Handler(void)
{
	TST LR, #4
	ITE EQ
	MRSEQ R0, MSP
	MRSNE R0, PSP
	
	B __cpp(SVC_Handler_C)
	ALIGN 4
}

/*USART0中断服务函数: 调试串口*/
void USART0_IRQHandler(void)
{
	debug_entry_USART0_IRQHandler();
}
