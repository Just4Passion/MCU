
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"

/*********************
 * 
 * SysTick定时器回调
 * 
 *********************/
void SysTick_Handler()
{
	//timer_manager_tick_handler();

	timer_manager_tick_decrement_handler();
}


