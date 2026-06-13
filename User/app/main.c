
#include <stdio.h>
#include <string.h>
#include <math.h>

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

#include "drv_watchdog.h"

#include "drv_timer.h"

#include "drv_fsmc_lcd.h"

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

void feed_iwdg()
{
	int ret = DY_EOK;
	dy_device_t *iwdg_dev = dy_find_device("iwdg");
	if (NULL == iwdg_dev)
	{
		return;
	}
	ret = iwdg_dev->ops->control(iwdg_dev, WDG_FEED_IWDG, NULL);
	if (ret != DY_EOK)
	{
		printf("feed iwdg failed\r\n");
	}
	else
	{
		/*2s喂一次狗, 此时增加30个计数, 一个计数是65ms*/
		printf("feed iwdg successfully\r\n");
	}
}

void lcd_init_show()
{
	int ret = DY_EOK;
	dy_device_t *lcd_dev = dy_find_device("lcd");
	if (NULL == lcd_dev)
	{
		return;
	}
	/*硬件初始化已经完成*/

	/****************************************************************
	 * 						上电初始化配置
	 ****************************************************************/
	/*RST复位*/
	ret = lcd_dev->ops->control(lcd_dev, LCD_RST, NULL);
	/*寄存器配置*/
	ret = lcd_dev->ops->control(lcd_dev, LCD_REG_CONFIG, NULL);
	/*设置扫描方向*/
	uint8_t gram_scan_mode = 6;
	ret = lcd_dev->ops->control(lcd_dev, LCD_GRAM_SCAN_CONFIG, &gram_scan_mode);
	/*清屏*/
	ret = lcd_dev->ops->control(lcd_dev, LCD_CLEAR_SCREEN, NULL);
	/*开启背光灯*/
	ret = lcd_dev->ops->control(lcd_dev, LCD_OPEN_BK_LIGHT, NULL);

	/****************************************************************
	 * 						图形驱动测试
	 ****************************************************************/
	/*画直线*/
	fsmc_lcd_line_t line = {.point1.x = 0, .point1.y = 0, .point2.x = 400, .point2.y = 400, .color = LCD_PIXEL_WHITE};
	ret = lcd_dev->ops->control(lcd_dev, LCD_DRAW_LINE, &line);
	/*画矩形*/
	fsmc_lcd_rectangle_t rectangle = {.start_point.x = 0, .start_point.y = 0, .width = 200, .height = 200, .color = LCD_PIXEL_BLACK, .filled = true};
	ret = lcd_dev->ops->control(lcd_dev, LCD_DRAW_RECTANGLE, &rectangle);
	/*画圆形*/
	fsmc_lcd_circle_t circle = {.center.x = 200, .center.y = 200, .radius = 200, .color = LCD_PIXEL_BLUE, .filled = true};
	ret = lcd_dev->ops->control(lcd_dev, LCD_DRAW_CIRCLE, &circle);
	
	/****************************************************************
	 * 						字符驱动测试
	 ****************************************************************/
	/*显示字符*/
	fsmc_lcd_string_t charactor = {.position.x = 400, .position.y = 400, .color = LCD_PIXEL_YELLOW, .str = "X"};
	ret = lcd_dev->ops->control(lcd_dev, LCD_DISPLAY_EN_CHAR, &charactor);
	/*显示字符串*/
	fsmc_lcd_string_t str = {.position.x = 0, .position.y = 0, .color = LCD_PIXEL_YELLOW, .str = "Hello World"};
	ret = lcd_dev->ops->control(lcd_dev, LCD_DISPLAY_EN_STRING, &str);
}

void lcd_draw_line()
{
	int ret = DY_EOK;
	dy_device_t *lcd_dev = dy_find_device("lcd");
	if (NULL == lcd_dev)
	{
		return;
	}
	/*清屏*/

	/*绘制*/
	//fsmc_lcd_line_t line = {.point1.x = 0, .point1.y = 0, .point2.x = 400, .point2.y = 400, .color = 0xFFFF};
	//ret = lcd_dev->ops->control(lcd_dev, LCD_DRAW_LINE, &line);
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

	/*看门狗初始化*/
	drv_iwdg_init();	// 确保系统初始化时间在范围内

	/*启用基本定时器6*/
	//drv_base_timer_init();
	//drv_oc_timer_init();
	//drv_ic_timer_init();

	/*sram*/
	//drv_fsmc_sram_init();

	/*lcd*/
	drv_fsmc_lcd_init();

	/*注册所有硬件*/
	drv_led_init();
	drv_key_init();

	drv_rtc_init();
	set_rtc_time();


	RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    
    printf("APB1: %lu\n", clocks.PCLK1_Frequency);
    printf("APB2: %lu\n", clocks.PCLK2_Frequency);
	printf("systemclock = %u\r\n", SystemCoreClock);
}


/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	int ret = 0;
	timer_manager_init();
	event_engine_init();
	lcd_init_show();
}

void key_timer_callback()
{
    static uint8_t key_last_state = 0;
    uint8_t key_cur_state = 0;
    dy_device_t *key1 = dy_find_device("key1");
    if (NULL == key1)
    {
        return;
    }
    /*读取按键状态, 并记录*/
    key1->ops->read(key1, &key_cur_state, 1);
    /*说明状态未发生改变*/
    if (key_cur_state != key_last_state)
    {
        if (0 != key_cur_state)
        {
            /*之前是松开的, 现在是按下了*/
            
        }
    }
    key_last_state = key_cur_state;
}


void app_init()
{
	//uint8_t timer_id1 = timer_create(10, lcd_draw_line, true);
	//timer_start(timer_id1);
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	uint8_t timer_id7 = timer_create(2000, feed_iwdg, true);
	timer_start(timer_id7);

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


