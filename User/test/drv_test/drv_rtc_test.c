
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

#include "drv_spi.h"
#include "drv_spi_flash.h"

#include "drv_rtc.h"

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
 * 			RTC测试, 使用内部的LSI晶振
 * 
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
			led_dev->ops->control(led_dev, LED_BLUE_ON, NULL);
			break;
		case EVENT_BUTTON_RELEASE:
			/*红灯亮*/
			led_dev->ops->control(led_dev, LED_RED_ON, NULL);
			break;
		case EVENT_BUTTON_LONG_PRESS:
			/*绿灯亮*/
			led_dev->ops->control(led_dev, LED_GREEN_ON, NULL);
			break;
		default:
			break;
	}
}

void rtc_time_test()
{
	int ret = 0;
	rtc_date_time date_time;
	dy_device_t *rtc_dev = dy_find_device("rtc");
	if (NULL == rtc_dev)
	{
		printf("rtc_dev not found\r\n");
		return;
	}
	ret = rtc_dev->ops->control(rtc_dev, RTC_GET_DATE_TIME, (void*)&date_time);
	if (ret != DY_EOK)
	{
		printf("rtc get time error\r\n");
		return;
	}
	printf("year = %d, month = %d, day = %d, weekday = %d, hours = %d, minutes = %d, seconds = %d\r\n",
		date_time.year, date_time.month, date_time.date, date_time.weekday,
		date_time.hours, date_time.minutes, date_time.seconds);
}

static void set_rtc_time()
{
	int ret = 0;
	rtc_date_time date_time;
	dy_device_t *rtc_dev = dy_find_device("rtc");
	if (NULL == rtc_dev)
	{
		printf("rtc_dev not found\r\n");
		return;
	}

	/*时间*/
	date_time.hours = 22;
    date_time.minutes = 17;
    date_time.seconds = 31;

    /*日期*/
    date_time.weekday = 6;
    date_time.date = 3;
    date_time.month = 1;
    date_time.year = 2026;
	ret = rtc_dev->ops->control(rtc_dev, RTC_SET_DATE_TIME, (void*)&date_time);
	if (ret != DY_EOK)
	{
		printf("rtc set time error\r\n");
		return;
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
	//drv_i2c_hw_init();
	//ret = drv_eeprom_init();

	/*SPI总线, 及其挂载设备初始化*/
	//drv_spi_hw_init();
	//drv_spi_flash_init();

	/*注册所有硬件*/
	drv_led_init();
	drv_key_init();

	drv_rtc_init();
	set_rtc_time();
}

/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	timer_manager_init();
	event_engine_init();
}

void app_init()
{
	//uint8_t timer_id1 = timer_create(1000, led_blink_per_second, true);
	//timer_start(timer_id1);
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	//uint8_t timer_id3 = timer_create(1000, spi_flash_write_read_cmp_test, true);
	//timer_start(timer_id3);
	uint8_t timer_id4 = timer_create(2000, rtc_time_test, true);
	timer_start(timer_id4);

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


