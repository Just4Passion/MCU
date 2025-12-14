
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "drv_time.h"

#include "timer_manager.h"

/*********************
 * 
 * SysTick定时器回调
 * 
 *********************/
void SysTick_Handler()
{
	//timer_manager_tick_handler();
	drv_time_sys_tick_increment();
	timer_manager_tick_decrement_handler();
}

// 实际的HardFault中断服务例程
void HardFault_Handler(void) {
    
	GPIO_InitTypeDef stGPIOInit;

    /*启用时钟*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    stGPIOInit.GPIO_Mode = GPIO_Mode_OUT;       //输出
	stGPIOInit.GPIO_OType = GPIO_OType_PP;      //推挽
	stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
	stGPIOInit.GPIO_Speed = GPIO_Speed_2MHz;
    /*红灯*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOF, &stGPIOInit);
	GPIO_ResetBits(GPIOF, GPIO_Pin_6);
	while(1);
}

// 实际的HardFault中断服务例程
void BusFault_Handler(void) {
    GPIO_InitTypeDef stGPIOInit;

    /*启用时钟*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    stGPIOInit.GPIO_Mode = GPIO_Mode_OUT;       //输出
	stGPIOInit.GPIO_OType = GPIO_OType_PP;      //推挽
	stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
	stGPIOInit.GPIO_Speed = GPIO_Speed_2MHz;
    /*红灯*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOF, &stGPIOInit);
	GPIO_ResetBits(GPIOF, GPIO_Pin_6);
	while(1);
}

// 实际的HardFault中断服务例程
void UsageFault_Handler(void) {
    GPIO_InitTypeDef stGPIOInit;

    /*启用时钟*/
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

    stGPIOInit.GPIO_Mode = GPIO_Mode_OUT;       //输出
	stGPIOInit.GPIO_OType = GPIO_OType_PP;      //推挽
	stGPIOInit.GPIO_PuPd = GPIO_PuPd_UP;
	stGPIOInit.GPIO_Speed = GPIO_Speed_2MHz;
    /*红灯*/
    stGPIOInit.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOF, &stGPIOInit);
	GPIO_ResetBits(GPIOF, GPIO_Pin_6);
	while(1);
}