
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
	/*获取I2C总线的数据*/
    dy_bus_t *bus = dy_bus_find("I2C1");
    if (NULL == bus)
    {
        return DY_ERROR;
    }
	int ret = 0;
	uint32_t i2c_timeout = 5000;
	dy_stm32_i2c_bus_config_t *cfg = (dy_stm32_i2c_bus_config_t *)(bus->priv_data);
	dy_i2c_bus_t *i2c_bus = (dy_i2c_bus_t*)(bus);
	

	dy_i2c_bus_ctl_cmd_detect_dev_t dev = {.dev_addr = 0xA0, .timeout_ms = 10};
    ret = i2c_bus->ops.i2c_bus_control(bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev);

	/*开始写入*/
	uint8_t data_buf[2] = {0};
    dy_i2c_msg i2c_msg = {0};
	i2c_msg.addr = 0xA0;
    i2c_msg.flags = 0;
    i2c_msg.buf = data_buf;
    i2c_msg.len = 2;

	uint8_t write_buf[256] = {0};
	uint32_t i = 0;
	for (i = 0; i < 8; ++i)
    {
		/*检查EEPROM是否准备好了: 最长10ms*/
		ret = i2c_bus->ops.i2c_bus_control(bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev);
		if (0 != ret)
		{
			return;
		}
        /*准备好了则写入*/
        data_buf[0] = 128 + i;
        data_buf[1] = i + 12;
		write_buf[i] = i;
        if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
        {
            return;
        }
    }
	printf("write is over\r\n");
	ret = i2c_bus->ops.i2c_bus_control(bus, DY_I2C_CTRL_CMD_DETECT_DEV, (void*)&dev);
	if (0 != ret)
	{
		return;
	}
	/*开始读取: 先写内存地址, 再读取数据*/
	uint8_t read_buf[256] = {0};
	i2c_msg.addr = 0xA0;
    i2c_msg.flags = 0;
    i2c_msg.buf = data_buf;
    i2c_msg.len = 1;
	data_buf[0] = 128;	// 地址
	if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
	{
		return;
	}
	printf("write mem addr ok\r\n");
	i2c_msg.addr = 0xA0;
    i2c_msg.flags = 1;
    i2c_msg.buf = read_buf;
    i2c_msg.len = 8;
	if (1 != i2c_bus->ops.master_xfer((dy_bus_t *)i2c_bus, &i2c_msg, 1))
	{
		printf("read failed\r\n");
		return;
	}
	printf("read is ok\r\n");

	if (0 == memcmp(write_buf, read_buf, 8))
	{
		printf("write read is same\r\n");
	}
	else
	{
		printf("write read is not same\r\n");
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


