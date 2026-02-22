

#ifndef FREERTOS_INTERRUPT_PRIORITY_H
#define FREERTOS_INTERRUPT_PRIORITY_H
#include "FreeRTOSConfig.h"
#include "gd32f4xx_misc.h"
/*全局执行一次nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);*/
#define CURRENT_NVIC_GROUP NVIC_PRIGROUP_PRE4_SUB0

/*系统中断优先级: 系统调度相关的中断, PendSV, Systick等*/
#define KERNEL_INTERRUPT_PRIORITY configLIBRARY_LOWEST_INTERRUPT_PRIORITY
/*系统中断最大优先级: 这种情况, 比这个优先级高的中断优先处理, 让用户中断高于系统的中断
但是比这个优先级低的中断, 不再具备抢占CPU的能力, 进入中断后会屏蔽其他中断, 建立一个临界区*/
#define MAX_SYSCALL_INTERRUPT_PRIORITY configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

/*使用优先级分组: Pre4, Sub0, 共有16个优先级, 抢占优先级4位, 范围是0-15; 子优先级固定0*/
/*USART0用于调试, 中断优先级配置成最低*/
#define USART0_FUNC_CLI             15



#define 


#endif








