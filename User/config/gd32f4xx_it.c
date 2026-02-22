
#include <stdint.h>
#include <rthw.h>
#include <rtthread.h>

#include "gd32f4xx_it.h"

void hard_fault_handler_c(unsigned int * args)
{
	/*
	发生错误了, 此时寄存器信息会被压入栈中
	和芯片的架构有关, 它支持多少寄存器, 这些寄存器的值分别表示什么
	*/
	
	
}

/*SysTick中断服务函数*/
void SysTick_Handler( void )
{
	/* enter interrupt */
    rt_interrupt_enter();

    rt_tick_increase();

    /* leave interrupt */
    rt_interrupt_leave();
}

/*USART0中断服务函数: 调试串口*/
void USART0_IRQHandler(void)
{
	debug_entry_USART0_IRQHandler();
}


