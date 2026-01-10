
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

#include "ff_app.h"

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
	date_time.hours = 23;
    date_time.minutes = 10;
    date_time.seconds = 28;

    /*日期*/
    date_time.weekday = 6;
    date_time.date = 5;
    date_time.month = 1;
    date_time.year = 2026;
	ret = rtc_dev->ops->control(rtc_dev, RTC_SET_DATE_TIME, (void*)&date_time);
	if (ret != DY_EOK)
	{
		printf("rtc set time error\r\n");
		return;
	}
}

uint8_t g_write_buffer[1024] = "Today is Monday, January 5, 2026";
uint8_t g_read_buffer[1024] = {0};
void fatfs_test()
{
	int ret = 0;
	FIL file;
	unsigned int len = 0;
	// 检查磁盘空间
	DWORD free_clusters, total_clusters;
	FATFS* fs;

	/*获取磁盘空间*/
	ret = f_getfree("1:", &free_clusters, &fs);
	if (ret == FR_OK) {
		DWORD free_sectors = free_clusters * fs->csize;
		DWORD free_kb = free_sectors * fs->ssize / 1024;
		
		printf("flash space now: %luKB\r\n", free_kb);
	}
	/*创建文件*/
	ret = f_open(&file, "1:fatfs_test.txt", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
	if (ret != FR_OK)
	{
		printf("f_open failed, ret = %d\r\n", ret);
		return;
	}
	/*写文件*/
	ret = f_write(&file, g_write_buffer, sizeof(g_write_buffer), &len);
	if (ret != FR_OK)
	{
		printf("f_write failed, ret = %d\r\n", ret);
		return;
	}
	/*文件定位到开头*/
	ret = f_lseek(&file, 0);
	if (ret != FR_OK)
	{
		printf("f_lseek failed, ret = %d\r\n", ret);
		return;
	}
	/*读取文件*/
	ret = f_read(&file, g_read_buffer, sizeof(g_read_buffer), &len);
	if (ret != FR_OK)
	{
		printf("f_read failed, ret = %d\r\n", ret);
		return;
	}
	/*比较写入和读取的结果*/
	if (0 == memcmp(g_write_buffer, g_read_buffer, sizeof(g_write_buffer)))
	{
		printf("=============read is same with write\r\n");
	}
	else
	{
		printf("=============read is not same with write\r\n");
	}
	ret = f_close(&file);
}

void scan_root_file()
{
	fatfs_scan_path("1:");
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

	drv_rtc_init();
	set_rtc_time();
}


/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	int ret = 0;
	timer_manager_init();
	event_engine_init();
	#if 0
	/*执行以下格式化 - 全块擦除*/
	dy_device_t *spi_flash = dy_find_device("flash_16MB");
	if (NULL == spi_flash)
	{
		printf("find device failed\r\n");
	}
	/*唤醒flash*/
	ret = spi_flash->ops->control(spi_flash, W25Q128_CHIP_ERASE, NULL);
	if (ret != DY_EOK)
	{
		printf("fat: spi_flash chip erase failed\r\n");
	}
	#endif
	ret = fatfs_mount();
	if (ret != DY_EOK)
	{
		printf("fatfs_mount failed\r\n");
	}
}

void app_init()
{
	//uint8_t timer_id1 = timer_create(1000, led_blink_per_second, true);
	//timer_start(timer_id1);
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	//uint8_t timer_id3 = timer_create(1000, spi_flash_write_read_cmp_test, true);
	//timer_start(timer_id3);
	uint8_t timer_id4 = timer_create(5000, fatfs_test, true);
	timer_start(timer_id4);
	uint8_t timer_id5 = timer_create(1000, scan_root_file, true);
	timer_start(timer_id5);

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

