

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "timer_manager.h"


/*********************
 * 
 * led控制接口
 * 	红: PF6
 * 	绿: PF7
 * 	蓝: PF8
 * 
 *********************/
static void led_init()
{
	GPIO_InitTypeDef stIOInit;
	/*启用时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOF, ENABLE);

	stIOInit.GPIO_Mode = GPIO_Mode_OUT;
	stIOInit.GPIO_OType = GPIO_OType_PP;
	stIOInit.GPIO_Speed = GPIO_Speed_2MHz;
	stIOInit.GPIO_PuPd = GPIO_PuPd_UP;

	/*红色*/
	stIOInit.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOF, &stIOInit);
	GPIO_SetBits(GPIOF, GPIO_Pin_6);

	/*绿色*/
	stIOInit.GPIO_Pin = GPIO_Pin_7;
	GPIO_Init(GPIOF, &stIOInit);
	GPIO_SetBits(GPIOF, GPIO_Pin_7);

	/*蓝色*/
	stIOInit.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOF, &stIOInit);
	GPIO_SetBits(GPIOF, GPIO_Pin_8);
}

/*********************
 * 
 * led控制接口
 * 	红: PF6
 * 	绿: PF7
 * 	蓝: PF8
 * 
 *********************/
void red_green_blue_blink()
{
	static uint8_t count = 0;
	count++;
	if (1 == count)
	{
		GPIO_ResetBits(GPIOF, GPIO_Pin_6);
		GPIO_SetBits(GPIOF, GPIO_Pin_7);
		GPIO_SetBits(GPIOF, GPIO_Pin_8);
	}
	else if (2 == count)
	{
		GPIO_SetBits(GPIOF, GPIO_Pin_6);
		GPIO_ResetBits(GPIOF, GPIO_Pin_7);
		GPIO_SetBits(GPIOF, GPIO_Pin_8);
	}
	else if (3 == count)
	{
		GPIO_SetBits(GPIOF, GPIO_Pin_6);
		GPIO_SetBits(GPIOF, GPIO_Pin_7);
		GPIO_ResetBits(GPIOF, GPIO_Pin_8);

		count = 0;
	}
}


/*********************
 * 
 * SysTick定时器回调
 * 
 *********************/
static void SysTick_Handler()
{
	timer_manager_tick_handler();
}

/*********************
 * 
 * SysTick 内核外设定时器
 * 
 *********************/
static void systick_init(uint32_t ticks_persecond)
{
	SysTick_Config((SystemCoreClock / ticks_persecond));	// core_cm4.h, STATIC INLINE
}



/*************************
 * 
 * 按键去抖
 * 
 ************************/



int main()
{
	systick_init(1000);
	timer_manager_init();
	led_init();

	uint8_t timer_id1 = timer_create(1000, red_green_blue_blink, true);
	timer_start(timer_id1);
	while(1)
	{

	}
	return 0;
}


