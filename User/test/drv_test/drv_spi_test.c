
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

void spi_flash_write_read()
{
	/**********************************************
	 * SPI总线初始化成功了, 现在使用SPI总线来操作SPI Flash：W25Q128 Flash芯片
	 * 
	 * 1、硬件设计
	 * 	(1)CS: 片选. 接入PG6
	 * 	(2)CLK: 时钟. 接入PB3
	 * 	(3)DIO: 输入. 接入PB5
	 * 	(4)DO: 输出. 接入PB4
	 * 	(5)WP: 写保护. 低电平时禁止写入. 直接接入电源, 不使用写保护
	 *  (6)HOLD: 暂停通信. 低电平时, 通讯暂停. 数据输出引脚输出高阻抗, 时钟和数据输入引脚无效
	 * ********************************************/

	/*获取SPI总线的数据*/
    dy_bus_t *bus = dy_bus_find("SPI1");
    if (NULL == bus)
    {
        return DY_ERROR;
    }
	int ret = 0;
	/****************************
	 * 读取芯片型号
	 * 	命令0x9F
	 * 	返回值: M7-M0 ID15-ID8 ID7-ID0
	 ****************************/
	uint8_t read_chip_model = 0x9F;
	uint8_t model_buf[3] = {0};
	ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_LOW, NULL); //拉低片选
	if (ret != DY_EOK)
	{
		printf("===============low failed\r\n");
		return;
	}
	ret = bus->ops->send(bus, (void*)&read_chip_model, 1);
	if (ret != 1)
	{
		printf("===============send failed\r\n");
		return;
	}
	ret = bus->ops->recv(bus, (void*)model_buf, 3);
	if (ret != 3)
	{
		printf("===============rcv failed\r\n");
		return;
	}
	ret = bus->ops->control(bus, DY_SPI_CTRL_CMD_CS_HIGH, NULL); //拉高片选
	if (ret != DY_EOK)
	{
		printf("===============high failed\r\n");
		return;
	}
	printf("success: %02x %02x%02x\r\n", model_buf[0], model_buf[1], model_buf[2]);	//返回结果为EF 4018, 即W25Q128
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
	drv_spi_hw_init();

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
	uint8_t timer_id3 = timer_create(1000, spi_flash_write_read, true);
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
