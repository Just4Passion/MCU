
#include <stdio.h>
#include "gd32f30x.h" 

#include "system_mng.h"

#define SYSTEM_PRIORITY_BITS 		4U   /* 系统优先级位数 */

void system_mini_init()
{
	/* 配置时钟频率, 配置flash等待周期 */
	SystemInit();

	/* 使用AFIO时钟: 用于GPIO复用. EXTI中断 */
	rcu_periph_clock_enable(RCU_AF);

	/* 配置中断向量地址: 相对于0x08000000的偏移 */
	nvic_vector_table_set(NVIC_VECTTAB_FLASH, 0x10000);

	/* 配置中断优先级分级数量 */
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
}

void system_info_print()
{
	extern int* __initial_sp[];
	extern int* __heap_base[];
	extern int* __heap_limit[];
	extern int* __Vectors[];
	extern int* __Vectors_End[];
	extern int* __Vectors_Size[];
	printf("\r\n============================System Info============================\r\n");
	/************************************************
	 * 
	 * 				堆栈信息, 向量表
	 * 
	 ***********************************************/
	printf("Stack & Heap & Vector:\r\n");
	printf("__initial_sp = 0x%08x\r\n", __initial_sp);
	printf("__heap_base = 0x%08x\r\n", __heap_base);
	printf("__heap_limit = 0x%08x\r\n", __heap_limit);
	printf("__Vectors = 0x%08x\r\n", __Vectors);
	printf("__Vectors_End = 0x%08x\r\n", __Vectors_End);
	printf("__Vectors_Size = 0x%08x\r\n", __Vectors_Size);
	printf("\r\n");
	/************************************************
	 * 
	 * 				系统启动源信息
	 * 
	 ***********************************************/
	printf("System Reset Source:\r\n");
	if (SET == rcu_flag_get(RCU_FLAG_EPRST))
	{
		printf("reset from external PIN\r\n");
	}
	else if (SET == rcu_flag_get(RCU_FLAG_PORRST))
	{
		printf("reset from power reset\r\n");
	}
	else if (SET == rcu_flag_get(RCU_FLAG_SWRST))
	{
		printf("reset from software reset\r\n");
	}
	else if (SET == rcu_flag_get(RCU_FLAG_FWDGTRST))
	{
		printf("reset from free watchdog\r\n");
	}
	else if (SET == rcu_flag_get(RCU_FLAG_WWDGTRST))
	{
		printf("reset from window watchdog\r\n");
	}
	else if (SET == rcu_flag_get(RCU_FLAG_LPRST))
	{
		printf("reset from low power reset\r\n");
	}
	else
	{
		printf("reset from power on\r\n");
	}
	printf("\r\n");
}

/*********************************************************************
 * 
 * 								SysTick中断
 * 
 **********************************************************************/
void systick_config()
{
	/* 时基配置为1ms中断一次 */
	if (SysTick_Config(SystemCoreClock / 1000U))
	{
		while (1)
		{
			/* 配置看门狗 */
		}
	}
	NVIC_SetPriority(SysTick_IRQn, 0x01U);
}

void systick_int_enable()
{
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
}

void systick_int_disable()
{
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}


/*********************************************************************
 * 
 * 								SVC异常
 * 
 **********************************************************************/
void svc_config()
{
	NVIC_SetPriority(SVCall_IRQn, 0x0EU);
}

void svc_trigger()
{
	__asm volatile("SVC #1"); // 触发服务号 1
}

/************************************
 * 关于异常禁用问题: 可以通过异常屏蔽寄存器来控制
 * 也可以通过优先级屏蔽来实现
 * 缺点:
 * 		无法做到精准控制, 会影响到其他中断
 *********************************/
void svc_disable()
{
	SCB->SHCSR &= ~SCB_SHCSR_SVCALLPENDED_Msk;
	//NVIC_DisableIRQ(SVCall_IRQn);
}

/*********************************************************************
 * 
 * 								PendSV异常
 * 
 **********************************************************************/
void pend_sv_config()
{
	NVIC_SetPriority(PendSV_IRQn, 0x0FU);
}

void pend_sv_trigger()
{
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void pend_disable()
{
	SCB->SHCSR &= ~SCB_SHCSR_PENDSVACT_Msk;
	//NVIC_DisableIRQ(PendSV_IRQn); //没有作用, 无法起到禁用. 属于内核内部的异常, 不受到NVIC控制
}