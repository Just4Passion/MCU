/*
 * FreeRTOS V202411.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

 #ifndef FREERTOS_CONFIG_H
 #define FREERTOS_CONFIG_H
 
 #include <stdio.h>
 #include "gd32f4xx.h"
 
 
 /*-----------------------------------------------------------
  * Application specific definitions.
  *
  * These definitions should be adjusted for your particular hardware and
  * application requirements.
  *
  * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
  * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE. 
  *
  * See http://www.freertos.org/a00110.html
  *----------------------------------------------------------*/
 
 #if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 extern uint32_t SystemCoreClock;
 #endif
  
 
 #define configUSE_PREEMPTION		    1           //抢占式
 #define configUSE_IDLE_HOOK			0
 #define configUSE_TICK_HOOK			0
 #define configCPU_CLOCK_HZ			(SystemCoreClock)   //( ( unsigned long ) 72000000 )	
 #define configTICK_RATE_HZ			( ( TickType_t ) 1000 )
 #define configMAX_PRIORITIES		( 32 )                                   //可使用的最大优先级数
 #define configMINIMAL_STACK_SIZE	( ( unsigned short ) 128 )              //空闲任务使用的栈大小, 也就是最小栈大小
 #define configTOTAL_HEAP_SIZE		( ( size_t ) ( 16 * 1024 ) )
 #define configMAX_TASK_NAME_LEN		( 16 )                                  //任务名称的最大长度
 //#define configUSE_TRACE_FACILITY	0
 #define configUSE_16_BIT_TICKS		    0            //系统节拍计数器变量类型, 1表示16位无符号整形, 0表示32位无符号整形
 #define configIDLE_SHOULD_YIELD		1           //空闲任务让出CPU的权限, 默认使能   
 
 /* Co-routine definitions. */
 #define configUSE_CO_ROUTINES 		0
 #define configMAX_CO_ROUTINE_PRIORITIES ( 2 )
 
 /* Set the following definitions to 1 to include the API function, or zero
 to exclude the API function. */
 
 #define INCLUDE_vTaskPrioritySet		1
 #define INCLUDE_uxTaskPriorityGet		1
 #define INCLUDE_vTaskDelete			1
 #define INCLUDE_vTaskCleanUpResources	0
 #define INCLUDE_vTaskSuspend			1
 #define INCLUDE_vTaskDelayUntil		1
 #define INCLUDE_vTaskDelay				1
 
 /* This is the raw value as per the Cortex-M3 NVIC.  Values can be 255
 (lowest) to 0 (1?) (highest). */
 //#define configKERNEL_INTERRUPT_PRIORITY 		255
 /* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
 See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
 //#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	191 /* equivalent to 0xb0, or priority 11. */
 
 
 /* This is the value being used as per the ST library which permits 16
 priority values, 0 to 15.  This must correspond to the
 configKERNEL_INTERRUPT_PRIORITY setting.  Here 15 corresponds to the lowest
 NVIC value of 255. */
 #define configLIBRARY_KERNEL_INTERRUPT_PRIORITY	15
 
 #define vAssertCalled(char,int) printf("Error:%s,%d\r\n",char,int)
 #define configASSERT( x ) if( ( x ) == 0 ) { vAssertCalled(__FILE__, __LINE__); }       //打印断言失败的位置和行号
 //#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ; ; ) { } }     //这里是进入一个死循环
 
 /*****************************************************************
  * 
  *                          编译指令介绍
  * 
  *****************************************************************/
 #if 1
 /*基础配置选项*/
 #define configUSE_TIME_SLICING                          1               //使能时间片轮转调度器, 默认使能
 #define configUSE_PORT_OPTIMISED_TASK_SELECTION         1               //任务选择端口优化, 需要特定硬件支持
 #define configUSE_TICKLESS_IDLE                         1               //使能低功耗tickless模式, 默认关闭
 #define configUSE_QUEUE_SETS                            1               //使能队列集, 默认关闭
 #define configUSE_TASK_NOTIFICATIONS                    1               //开启任务通知功能, 默认开启
 #define configUSE_MUTEXES                               1               //使用互斥信号量
 #define configUSE_RECURSIVE_MUTEXES                     1               //使用递归互斥信号量
 #define configUSE_COUNTING_SEMAPHORES                   1               //为 1 时使用计数信号量
 #define configQUEUE_REGISTRY_SIZE                       10              //设置可以注册的信号量和消息队列个数
 #define configUSE_APPLICATION_TASK_TAG                  0
 
 
 /*内存相关*/
 #define configSUPPORT_DYNAMIC_ALLOCATION                1       //使能动态内存分配
 #define configSUPPORT_STATIC_ALLOCATION                 0       //使能静态内存分配
 //#define configTOTAL_HEAP_SIZE                           ((size_t)(36*1024)) //系统总堆大小
 
 
 /*钩子函数*/
 #define configUSE_IDLE_HOOK                             0       //空闲钩子
 #define configUSE_TICK_HOOK                             0       //时间片钩子
 #define configUSE_MALLOC_FAILED_HOOK                    0       //内存分配失败钩子
 #define configCHECK_FOR_STACK_OVERFLOW                  0       //堆栈溢出检测钩子
 
 /*运行事件和任务状态收集*/
 #define configGENERATE_RUN_TIME_STATS                   1       //启用时间统计
 #define configUSE_TRACE_FACILITY                        1       //启用可视化追踪
 #define configUSE_STATS_FORMATTING_FUNCTIONS            1       //格式化输出函数调用信息
 
 
 /*与协程有关的配置*/
 //#define configUSE_CO_ROUTINES                           0       //启用协程支持
 //#define configMAX_CO_ROUTINE_PRIORITIES                 0       //协程最大优先级
 
 /*定时器*/
 #define configUSE_TIMERS                                1       //启用软件定时器
 #define configTIMER_TASK_PRIORITY                       (configMAX_PRIORITIES-1)    //定时器任务优先级
 #define configTIMER_QUEUE_LENGTH                        10      //定时器队列长度
 #define configTIMER_TASK_STACK_DEPTH                    (configMINIMAL_STACK_SIZE*2)    //定时器任务栈大小
 
 
 /*可选函数配置选项*/
 #define INCLUDE_xTaskGetSchedulerState                  1 
 #define INCLUDE_vTaskPrioritySet                        1
 #define INCLUDE_uxTaskPriorityGet                       1 
 #define INCLUDE_vTaskDelete                             1
 //#define INCLUDE_vTaskCleanUpResources                 1
 #define INCLUDE_vTaskSuspend                            1
 #define INCLUDE_vTaskDelayUntil                         1
 #define INCLUDE_vTaskDelay                              1
 #define INCLUDE_eTaskGetState                           1
 #define INCLUDE_xTimerPendFunctionCall                  1
 
 
 
 /*中断相关*/
 #ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS                                 __NVIC_PRIO_BITS        //stm32fxxx.h
 #else
 #define configPRIO_BITS                                 4
 #endif
 #define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15          //中断最低优先级
 #define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    11           //系统可管理的中断最高优先级, basepri 寄存器, 让 FreeRTOS 屏蔽优先级数值大于这个宏定义的中断
 #define configKERNEL_INTERRUPT_PRIORITY ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
 
 #define configMAX_SYSCALL_INTERRUPT_PRIORITY ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )


/*****************************************************************
 * 
 *                          调试接口编译宏定义
 * 
 *****************************************************************/
#define configINCLUDE_QUERY_HEAP_COMMAND 1

/*定时器相关函数*/
void vSetupTimerForRunTimeStats(void);              //用于启动定时器
uint32_t ulGetRunTimeCounterValue(void);            //用于获取定时器计数
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  vSetupTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE() ulGetRunTimeCounterValue()

 
 /*中断函数*/
 #define xPortPendSVHandler                              PendSV_Handler                //PendSV处理函数
 #define vPortSVCHandler                                 SVC_Handler                   //
 
 #endif
 
 
 /*****************************************************************
  * 
  *                          port接口声明
  * 
  *****************************************************************/
 
 void xPortSysTickHandler( void );
 
 
 #endif /* FREERTOS_CONFIG_H */
 
 

