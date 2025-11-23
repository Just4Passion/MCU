

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"
#include "simple_event_engine.h"

#include "kservice.h"

#include "drv_key.h"


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
 *  按键事件处理
 * 
 ************************/
void key_event_handler(event_t *event)
{
	dy_device_t *led_dev = dy_find_device("led");
	if (NULL == led_dev)
	{
		return;
	}
	switch (event->type)
	{
		case EVENT_BUTTON_PRESS:
			/*蓝灯亮*/
			led_dev->ops->control(led_dev, 3, NULL);
			break;
		case EVENT_BUTTON_RELEASE:
			/*红灯亮*/
			led_dev->ops->control(led_dev, 1, NULL);
			break;
		case EVENT_BUTTON_LONG_PRESS:
			/*绿灯亮*/
			led_dev->ops->control(led_dev, 2, NULL);
			break;
		default:
			break;
	}
}

/*设备初始化回调*/
static void dev_init_callback(dy_device_t *dev, void *arg)
{
    dev->ops->init(dev);
}

/*板子初始化*/
void board_init()
{
	systick_init(1000);

	/*注册所有总线*/

	/*总线完成初始化*/

	/*注册所有硬件*/
	drv_led_register();
	drv_key_register();
	/*所有的硬件完成初始化*/
	dy_device_foreach(dev_init_callback, NULL);
}

/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	systick_init(1000);
	timer_manager_init();
	event_engine_init();
}

/*应用初始化*/
void app_init()
{
    /*创建按键检测定时任务*/
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);

	/*订阅按键事件*/
	event_subscribe(EVENT_BUTTON_PRESS, key_event_handler);
	event_subscribe(EVENT_BUTTON_RELEASE, key_event_handler);
	event_subscribe(EVENT_BUTTON_LONG_PRESS, key_event_handler);
}

int main()
{
	board_init();
	system_init();
	app_init();

	while(1)
	{
		/*超时定时器处理*/
		timer_manager_expired_timer_handler();
		/*事件处理*/
		event_process();

	}
	return 0;
}


