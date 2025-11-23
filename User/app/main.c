

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"
#include "simple_event_engine.h"

#include "kservice.h"

#include "drv_key.h"


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
 * 按键去抖
 * 	每隔10ms检测一次, 如果两次之间的值一致, 说明是有效值
 * 	如果两次检测都是按下, 说明是按下了, 记录按下的状态(状态被处理后, 清除), 或者发送按下事件
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

void app_init()
{
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	/*订阅事件*/
	event_subscribe(EVENT_BUTTON_PRESS, key_event_handler);
	event_subscribe(EVENT_BUTTON_RELEASE, key_event_handler);
	event_subscribe(EVENT_BUTTON_LONG_PRESS, key_event_handler);
}

int main()
{
	board_init();
	system_init();
	app_init();

	uint8_t data = 0;
	uint8_t last_data = data;
	dy_device_t *dev = dy_find_device("key1");
	dy_device_t *led_dev = dy_find_device("led");

	while(1)
	{
		/*超时定时器处理*/
		timer_manager_expired_timer_handler();
		/*事件处理*/
		event_process();

	}
	return 0;
}


