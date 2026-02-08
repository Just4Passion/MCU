
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

void single_color_led()
{
	/*************************************
	 * 呼吸灯
	 * 		基本配置
	 * 			PWM波: 周期是100Hz(1s输出100个PWM波), 占空比0-100可调
	 * 			呼吸频率: 占空比完成一次0->100->0的调节所需的时间
	 *		实现方法
	 *			线性调节: 0->100->0. 定时任务10ms, 执行400+30次任务完成一次呼吸. 即呼吸频率是4.3s
	 *			正弦调节: 0->100->0. 定时任务10ms, 执行500次任务完成一次呼吸. 即呼吸频率是5s
	 *			符合人眼感知的伽马校正或指数曲线: 0->100->0. 定时任务10ms, 执行500次任务完成一次呼吸. 即呼吸频率是5s
	 **************************************/
	dy_device_t *timer10 = dy_find_device("timer10");
	if (NULL == timer10)
	{
		return;
	}
	#if 0		//线性 - 调整速率为4/6分的情况下存在顿挫感; 不调整亮度保持时间太长
	static uint32_t duty = 0;
	static int8_t dir = 1;

	//duty += dir * 1;
	if (duty >= 1000) { dir = -1; }
	if (duty <= 0) { dir = 1; }
	
	if (duty <= 400) {
		duty += dir * 1;
	} else{
		duty += dir * 20;
	}

	timer10->ops->control(timer10, OCTIMER_SET_PULSE, &duty);
	#endif

	#if 0		//正弦 - 感知上亮度保持时间太长
	static uint32_t counter = 0;
    static const uint32_t breath_cycle = 500; // 呼吸周期（毫秒）
    
	// 使用正弦函数生成平滑的亮度变化
    float radian = (2 * 3.14159 * counter) / breath_cycle;
    float sine_value = (sin(radian) + 1.0) / 2.0; // 0到1之间
    
    uint32_t duty = (uint32_t)(sine_value * 1000);
    counter = (counter + 1) % breath_cycle;
    
    timer10->ops->control(timer10, OCTIMER_SET_PULSE, &duty);
	#endif

	#if 1		//指数 - 效果奇好
	static uint32_t counter = 0;
    static const uint32_t breath_cycle = 500; // 呼吸周期（毫秒）

	float normalized = (float)counter / breath_cycle;
	float brightness = pow(sin(normalized * 3.14159), 2.2); // 伽马校正2.2
	
	uint32_t duty = (uint32_t)(brightness * 1000); 			//
	counter = (counter + 1) % breath_cycle;
    
    timer10->ops->control(timer10, OCTIMER_SET_PULSE, &duty);
	#endif
}

/*全彩色LED*/
void full_colors_led()
{
	static uint8_t rnd_enable_flag = 0;
	if (0 == rnd_enable_flag)
	{
		/* 使能RNG时钟 */
  		RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
		RNG_Cmd(ENABLE);
		rnd_enable_flag = 1;
	}

	uint32_t random_color = 0;
	/* 等待随机数产生完毕 */
	while(RNG_GetFlagStatus(RNG_FLAG_DRDY)== RESET);
	/*获取随机数*/       
	random_color = RNG_GetRandomNumber();
	/************************************************
	 * 原理：
	 * 		全彩是三色灯(红, 蓝, 绿)不同亮度和混合
	 * 		PWM占空比控制不同灯的亮的时间
	 * 
	 * 硬件设计:
	 * 		LED_Red -> PF6 -> TIM10_CH1
	 * 		LED_Green -> PF7 -> TIM11_CH1
	 * 		LED_Blue -> PF8 -> TIM13_CH1
	 * 
	 *************************************************/
	int ret = DY_EOK;
	dy_device_t *timer10 = dy_find_device("timer10");
	if (NULL == timer10)
	{
		return;
	}

	dy_device_t *timer11 = dy_find_device("timer11");
	if (NULL == timer11)
	{
		return;
	}

	dy_device_t *timer13 = dy_find_device("timer13");
	if (NULL == timer13)
	{
		return;
	}

	uint32_t red_pulse = 0;
	uint32_t green_pulse = 0;
	uint32_t blue_pulse = 0;

	red_pulse = (random_color >> 16) & 0x000000FF;
	green_pulse = (random_color >> 8) & 0x000000FF;
	blue_pulse = (random_color) & 0x000000FF;
	/*分别给3个定时器设置不同的占空比*/
	timer10->ops->control(timer10, OCTIMER_SET_PULSE, &red_pulse);
	timer11->ops->control(timer11, OCTIMER_SET_PULSE, &green_pulse);
	timer13->ops->control(timer13, OCTIMER_SET_PULSE, &blue_pulse);
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
	drv_oc_timer_init();

	/*注册所有硬件*/
	//drv_led_init();
	drv_key_init();

	drv_rtc_init();
	set_rtc_time();


	RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    
    printf("APB1: %lu\n", clocks.PCLK1_Frequency);
    printf("APB2: %lu\n", clocks.PCLK2_Frequency);
	printf("===============================systemclock = %u\r\n", SystemCoreClock);
}


/*系统初始化: 定时器, 事件驱动*/
void system_init()
{
	int ret = 0;
	timer_manager_init();
	event_engine_init();
}

void app_init()
{
	//uint8_t timer_id1 = timer_create(200, show_timer6_cnt, true);
	//timer_start(timer_id1);
	uint8_t timer_id2 = timer_create(10, key1_timer_callback, true);
	timer_start(timer_id2);
	uint8_t timer_id7 = timer_create(2000, feed_iwdg, true);
	timer_start(timer_id7);
	//uint8_t timer_id8 = timer_create(1000, full_colors_led, true);
	//timer_start(timer_id8);
	uint8_t timer_id9 = timer_create(10, single_color_led, true);
	timer_start(timer_id9);

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
