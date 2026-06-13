

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"

#include "kservice.h"


/***************************************************************************
 * 
 * 			测试功能实现了LED闪烁效果: 红色, 绿色, 蓝色, 轮流亮1s
 * 
 **************************************************************************/


/*********************
 * 
 * SysTick 内核外设定时器
 * 
 *********************/
void systick_init(uint32_t ticks_persecond)
{
	SysTick_Config((SystemCoreClock / ticks_persecond));	// core_cm4.h, STATIC INLINE
}

/*************************
 * 
 * 灯闪烁: 红, 绿, 蓝
 * 
 ************************/

void led_blink_per_second()
{
	static uint8_t count = 0;
	dy_device_t *led_dev = dy_find_device("led");
	if (NULL == led_dev)
	{
		return;
	}

	count++;
	if (1 == count)
	{
		led_dev->ops->control(led_dev, 1, NULL);
	}
	else if (2 == count)
	{
		led_dev->ops->control(led_dev, 2, NULL);
	}
	else if (3 == count)
	{
		led_dev->ops->control(led_dev, 3, NULL);

		count = 0;
	}
}

static void dev_init_callback(dy_device_t *dev, void *arg)
{
    dev->ops->init(dev);
}

void board_init()
{
	systick_init(1000);

	/*注册所有总线*/

	/*总线完成初始化*/

	/*注册所有硬件*/
	drv_led_init();
	/*所有的硬件完成初始化*/
	dy_device_foreach(dev_init_callback, NULL);
}

void system_init()
{
	systick_init(1000);
	timer_manager_init();
}

int main()
{
	board_init();
	system_init();

	uint8_t timer_id1 = timer_create(1000, led_blink_per_second, true);
	timer_start(timer_id1);

	while(1)
	{

	}
	return 0;
}






