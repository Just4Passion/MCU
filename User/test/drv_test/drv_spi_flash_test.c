
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
 * 			SPI Flash W25Q128: 16MB
 * 
 * 			测试功能实现了
 * 					向指定地址写入超过一个扇区的数据, 然后再从该字节读取写入的数据
 * 					将两个数据进行比较, 一致, 则执行通过
 * 
 * STM32F407ZG: 192(128 + 64)KB		512KB
 * RO size: 11.72KB - Code + RO Data
 * RW size: 15.06KB
 * ROM size: 12.00KB (Code + RO Data(初始化了, 且只读) + RW Data(初始化了, 但是可修改, ROM需要保存初始化值))
 * Stack size: 0x2000 = 8KB, 
 * Max Stack Usage = 4034 + Unknown(编译器内嵌函数)
 * 	drv_flash_W25Q128_write ⇒ drv_flash_W25Q128_single_sector_write ⇒ drv_flash_W25Q128_PageWrite ⇒ drv_flash_W25Q128_WriteEnable ⇒ drv_flash_W25Q128_send_data ⇒ __2printf
 * 	扇区的"读"->"修改"->"写", 需要申请一个扇区(4KB)的内存空间, 我把这个空间放在栈中
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

/*使用全局变量, 使用栈空间的话, 会导致栈溢出*/
uint8_t g_data_write[6 * 1024] = {0};
uint8_t g_data_read[6 * 1024] = {0};
void spi_flash_write_read_cmp_test()
{
	/*获取SPI总线的数据*/
	dy_device_t *spi_flash = dy_find_device("flash_16MB");
	if (NULL == spi_flash)
	{
		printf("spi_flash not found\r\n");
		return;
	}

	int32_t ret = 0;
	/*写入内存地址*/
	static uint32_t mem_addr = 0;

	for (int i = 0; i < 6 * 1024; ++i)
	{
		g_data_write[i] = i;
	}

	spi_flash->ops->control(spi_flash, W25Q128_SET_MEM_ADDR, (void*)&mem_addr);
	ret = spi_flash->ops->write(spi_flash, g_data_write, sizeof(g_data_write));
	if (ret != sizeof(g_data_write))
	{
		printf("spi flash write failed\r\n");
		return;
	}
	/*从0地址读取*/
	spi_flash->ops->control(spi_flash, W25Q128_SET_MEM_ADDR, &mem_addr);
	ret = spi_flash->ops->read(spi_flash, g_data_read, sizeof(g_data_read));
	if (ret != sizeof(g_data_read))
	{
		printf("spi flash read failed\r\n");
		return;
	}
	/*比较写入和读取的结果*/
	if (0 == memcmp(g_data_write, g_data_read, 6 * 1024))
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
			printf("%d ", g_data_read[i]);
		}
		printf("\r\n");
	}
	mem_addr += 255;
	if (mem_addr > ((4 * 1024) + 5))
	{
		mem_addr = 0;
	}
}

void spi_flash_control_test()
{
	/*获取SPI总线的数据*/
	dy_device_t *spi_flash = dy_find_device("flash_16MB");
	if (NULL == spi_flash)
	{
		printf("spi_flash not found\r\n");
		return;
	}
	int32_t ret = 0;
	uint8_t device_id[3] = {0};

	ret = spi_flash->ops->control(spi_flash, W25Q128_WAKE_UP, NULL);
	if (ret != DY_EOK)
	{
		printf("read chip id failed\r\n");
		return;
	}
	printf("wakeup chip successful\r\n");

	ret = spi_flash->ops->control(spi_flash, W25Q128_READ_CHIP_ID, (void*)device_id);
	if (ret != DY_EOK)
	{
		printf("read chip id failed\r\n");
		return;
	}
	printf("device id: %02x %02x %02x\r\n", device_id[0], device_id[1], device_id[2]);	//返回结果为EF 40 18, 即W25Q128
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
	drv_spi_flash_init();

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
	//uint8_t timer_id1 = timer_create(1000, led_blink_per_second, true);
	//timer_start(timer_id1);
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	//uint8_t timer_id3 = timer_create(1000, spi_flash_write_read_cmp_test, true);
	//timer_start(timer_id3);
	uint8_t timer_id4 = timer_create(1000, spi_flash_control_test, true);
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
