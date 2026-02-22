
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
#if (INCLUDE_xTaskGetSchedulerState == 1 ) 
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { 
#endif
        xPortSysTickHandler();
#if (INCLUDE_xTaskGetSchedulerState == 1 ) 
    }
#endif
}

/*USART0中断服务函数: 调试串口*/
void USART0_IRQHandler(void)
{
	serial_usart_interrupt_handle();
}


