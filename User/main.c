
#include <stdio.h>
#include "gd32f30x.h" 
#include "gd32f30x_libopt.h"

#include "drv_usart.h"
#include "system_mng.h"

void my_delay(uint32_t count)
{
	while(count--);
}

int main(void)
{
	system_mini_init();

	/*系统时基: 配置周期, 启用优先级*/
	//systick_config();

	/*配置PendSV最低优先级: 如何验证 - 通过SysTick中断来验证.*/
	//pend_sv_config();

	/*串口调试*/
	debug_init();

	/* 初始化任务系统 */
	//task_start();
	//timer_pwm_led();
	//LED_initialize();
	//timer_pwm_led_init();
	main_os();

	while(1)
	{
		debug_deal();
        my_delay(100);
	}
	return 0;
}
