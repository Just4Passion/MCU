
#include <stdio.h>
#include <string.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"
#include "simple_event_engine.h"

#include "kservice.h"

#include "drv_key.h"
#include "drv_led.h"
#include "drv_i2c.h"
#include "drv_usart.h"
#include "drv_serial.h"
#include "drv_i2c.h"

#include "drv_eeprom.h"

/*********************
 * 
 * SysTick 内核外设定时器
 * 
 *********************/
void systick_init(uint32_t ticks_persecond)
{
	SysTick_Config((SystemCoreClock / ticks_persecond));	// core_cm4.h, STATIC INLINE
}

/***************************************************************************
 * 			EEPROM存储: 256字节
 * 
 * 			测试功能实现了
 * 					向地址0-7, 写入8个字节: 1, 2, 3, 4, 5, 6, 7, 8; 读取地址0-7, 奇数亮红灯, 偶数亮绿灯
 * 					向地址12-23, 写入12个字节: 1-12; 读取地址12-23的数据, 奇数亮红灯, 偶数亮绿灯
 * 					向地址31-48, 写入18个字节: 1-18; 读取31-48的数据, 奇数亮红灯,
 **************************************************************************/

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
		led_dev->ops->control(led_dev, LED_RED_ON, NULL);
	}
	else if (2 == count)
	{
		led_dev->ops->control(led_dev, LED_GREEN_ON, NULL);
	}
	else if (3 == count)
	{
		led_dev->ops->control(led_dev, LED_BLUE_ON, NULL);

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

void eeprom_write_read_compare()
{
	dy_device_t *eeprom_dev = dy_find_device("eeprom");
	if (NULL == eeprom_dev)
	{
		return;
	}
	int32_t ret = 0;
	static uint8_t mem_addr = 0;
	mem_addr++;
	if (8 == mem_addr)
	{
		mem_addr = 0;
	}
	printf("mem_addr = %d\r\n", mem_addr);
	uint8_t data_write[256] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	uint8_t data_read[256] = {0};

	/*从0地址写入*/
	eeprom_dev->ops->control(eeprom_dev, EEPROM_SET_MEM_ADDR, &mem_addr);
	ret = eeprom_dev->ops->write(eeprom_dev, data_write, 16);

	/*从0地址读取*/
	eeprom_dev->ops->control(eeprom_dev, EEPROM_SET_MEM_ADDR, &mem_addr);
	ret = eeprom_dev->ops->read(eeprom_dev, data_read, 16);
	/*比较写入和读取的结果*/
	if (0 == memcmp(data_write, data_read, 16))
	{
		printf("data is OK\r\n");
	}
	else
	{
		printf("data is error\r\n");
		uint8_t i = 0;
		printf("read info: ");
		for (i = 0; i < 16; ++i)
		{
			printf("%d ", data_read[i]);
		}
		printf("\r\n");
	}
}

/*板子初始化*/
void board_init()
{
	int ret = 0;
	systick_init(1000);

	/*初始化*/
	dy_bus_manager_init();
	dy_device_manager_init();

	/*usart总线, 及其挂载设备初始化*/
	drv_usart_hw_init();
	drv_serial_init();

	/*I2C总线, 及其挂载设备初始化*/
	drv_i2c_hw_init();
	ret = drv_eeprom_init();

	/*注册所有硬件*/
	drv_led_init();
	drv_key_init();
}

/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	timer_manager_init();
	event_engine_init();
}

void app_init()
{
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	uint8_t timer_id3 = timer_create(1000, eeprom_write_read_compare, true);
	timer_start(timer_id3);

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
	while(1)
	{
		/*超时定时器处理*/
		timer_manager_expired_timer_handler();
		/*事件处理*/
		event_process();
	}
	return 0;
}
