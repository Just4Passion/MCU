
#include "FreeRTOS.h"
#include "gd32f4xx.h"

/*
TIM2 的作用：

1. 作为定时器中断源，产生频率为 20000 Hz 的定时中断（每 50 微秒一次）
2. 配置为向上计数模式，预分频器为 0，周期根据系统时钟计算
3. 启用更新中断并配置为最高优先级，用于触发系统定时中断

TIM3 的作用：

1. 作为高分辨率时间测量的自由运行计数器
2. 设置为最大周期 (0xFFFF)，不产生中断
3. 在 TIM2 中断服务程序中捕获当前计数值，用于计算相邻中断之间的时间差

工作流程：

1. TIM2 定期产生中断
2. 在 TIM2 中断服务程序中读取 TIM3 当前计数值
3. 计算相邻中断的时间差
4. 与预期时间差比较，计算中断处理抖动
5. 维护最大抖动值

该测试用于测量 FreeRTOS 中断处理的抖动性能，通过比较实际中断间隔与期望间隔来评估系统的实时性能

使用无符号整数计算, 减法会自然处理溢出情况
*/

#ifdef TIMER_JITTER_TEST
//这个应该叫做建立时间测试
#define timerINTERRUPT_FREQUENCY		( ( unsigned short ) 20000 )
#define timerSETTLE_TIME			5
void vSetupTimerTest()
{
    /*启用time2
    启用time3
    */
    unsigned long frequency = 0;
    timer_parameter_struct timer_initpara;
    rcu_periph_clock_enable(RCU_TIMER2);
    rcu_periph_clock_enable(RCU_TIMER3);
    
    timer_deinit(TIMER2);
    timer_deinit(TIMER3);

    timer_struct_para_init(&timer_initpara);
    frequency = configCPU_CLOCK_HZ / timerINTERRUPT_FREQUENCY;  //2000分之一秒
    timer_initpara.prescaler 		= 0;
    timer_initpara.alignedmode		= TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection	= TIMER_COUNTER_UP;
    timer_initpara.period			= (unsigned short)(frequency & 0xffffUL);
    timer_initpara.clockdivision 	= TIMER_CKDIV_DIV1;
    timer_init(TIMER2, &timer_initpara);

    /*timer3配置为最大周期*/
    timer_initpara.period			= (unsigned short)0xffffUL;
    timer_init(TIMER3, &timer_initpara);

    /*使能TIMER2中断*/
    nvic_irq_enable(TIMER2_IRQn, 0, 0);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);

    /*启用*/
    timer_enable(TIMER2);
    timer_enable(TIMER3);
}

volatile unsigned short max_jitter = 0;
void vTimer2IntHandler(void)
{
    /*每次发生中断的时候, 记录TIMER3的当前计数值*/
    static unsigned short last_count = 0, settle_count = 0, max_difference = 0;\
    unsigned short cur_count, difference;

    cur_count = timer_counter(TIMER3);

    if (settle_count >= timerSETTLE_TIME)
    {
        difference = (cur_count - last_count); /*当前与上一次的差值*/
        if (difference > max_difference)    /*如果差值大于记录的最大值, 更新最大值*/
        {
            max_difference = difference;
            max_jitter = max_difference - timerINTERRUPT_FREQUENCY; //计算最大抖动值
        }
    }
    else
    {
        settle_count++;
    }
    last_count = cur_count;
    /*清除中断标志位*/
    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);
}

#endif




