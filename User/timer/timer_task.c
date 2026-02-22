

#include <stdio.h>
#include <string.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "me_rtos_cli.h"

/*************************************************************
    定时器
        创建, 启动, 停止, 重置

    自动重载定时器
    单次定时器
    从ISR中调用的定时器功能

FreeRTOS定时器的实现机制
    定时器服务任务： FreeRTOS 创建一个专门的定时器任务（通常优先级很高），负责处理所有定时器的到期事件。
    定时器队列： 所有定时器操作都通过一个队列传递给定时器服务任务，确保线程安全。
    时间基准： 定时器的过期时间计算基于 `xTaskGetTickCount()` 返回的系统滴答计数，这个计数由 SysTick 中断提供
    定时器列表管理： 定时器按照过期时间排序存储在链表中，定时器服务任务按顺序检查和处理到期的定时器。
**********************************************************/

#define tmrdemoDONT_BLOCK                    ((TickType_t )0)
#define tmrdemoONE_SHOT_TIMER_PERIOD         ( xBasePeriod * ( TickType_t ) 3 )

/*定时器回调函数*/
static void prvAutoReloadTimerCallback(TimerHandle_t pxExpiredTimer);
/*创建一堆自动重载定时器*/
static void prvTest1_CreateTimersWithoutSchedulerRunning(void);

static void prvISRAutoReloadTimerCallback(TimerHandle_t pxExpiredTimer);
static void prvISROneShotTimerCallback(TimerHandle_t pxExpiredTimer);

/*自动重载: 定时器*/
static TimerHandle_t xAutoReloadTimers[configTIMER_QUEUE_LENGTH + 1] = { 0 };
/*自动重载定时器的计数器*/
static uint8_t ucAutoReloadTimerCounters[configTIMER_QUEUE_LENGTH + 1] = { 0 };

static uint8_t ucIsStopNeededInTimerZeroCallback = (uint8_t) pdFALSE;
/*基本周期, 其他定时器都是基于基础周期计算*/
static TickType_t xBasePeriod = 0;

/*可用于中断的自动重载定时器*/
static TimerHandle_t xISRAutoReloadTimer = NULL;
static uint8_t ucISRAutoReloadTimerCounter = ( uint8_t ) 0;

/*一次定时器*/
static TimerHandle_t xISROneShotTimer = NULL;
static uint8_t ucISROneShotTimerCounter = ( uint8_t ) 0;

/*单次定时器*/
static TimerHandle_t xOneShotTimer = NULL;
static uint8_t ucOneShotTimerCounter = ( uint8_t ) 0;

void vStartTimerDemoTask(TickType_t xBasePeriodIn)
{
    xBasePeriod = xBasePeriodIn;
    prvTest1_CreateTimersWithoutSchedulerRunning();
}

/*
*/
/**
 * @brief 在调度器未运行的情况下创建并启动多个自动重载定时器。
 * 该函数用于测试目的，循环创建 `configTIMER_QUEUE_LENGTH` 个定时器，每个定时器具有递增的周期（基于 `xBasePeriod` 和当前索引），
 * 并尝试启动它们。由于调度器未运行，定时器启动操作可能会失败（依赖调度器处理定时器队列消息）。
 * @note
 * - 定时器周期计算公式：`(xTimer + 1) * xBasePeriod`，其中 `xTimer` 为循环索引（0 到 `configTIMER_QUEUE_LENGTH - 1`）。
 * - 定时器为自动重载类型（`pdTRUE`），使用回调函数 `prvAutoReloadTimerCallback`。
 * - 创建的定时器句柄存储在全局数组 `xAutoReloadTimers` 中。
 * - 如果创建或启动定时器失败，会通过 `printf` 输出错误信息。
 * 
 * @warning 此函数假设调度器处于未运行状态，调用前需确保环境符合，否则行为未定义。
 */
static void prvTest1_CreateTimersWithoutSchedulerRunning(void)
{
    TickType_t xTimer;
    for( xTimer = 0; xTimer < (configTIMER_QUEUE_LENGTH - 2); xTimer++)
    {
        /*创建一个定时器并插入到链表中*/
        xAutoReloadTimers[xTimer] = xTimerCreate( "FR Timer", 
            ((xTimer + ( TickType_t )1) * xBasePeriod ),    //定时器周期
            pdTRUE,                 //是否自动重载
            (void*)xTimer,          //定时器标识
            prvAutoReloadTimerCallback );
        if (xAutoReloadTimers[xTimer] == NULL)
        {
            printf("Failed to create timer %d\r\n", xTimer);
        }
        else
        {
            /*启动定时器 - 启动定时器会发送一个任务消息到定时器操作队列中*/
            if (xTimerStart(xAutoReloadTimers[xTimer], portMAX_DELAY) != pdPASS)
            {
                printf("Failed to start timer %d\r\n", xTimer);
            }
        }
    }

    /*创建一个定时器, 它的回调函数可以在中断*/
    xISRAutoReloadTimer = xTimerCreate("ISR Timer",
                                        0xFFFF,
                                        pdTRUE,
                                        (void*)NULL,
                                        prvISRAutoReloadTimerCallback);
    if (xTimerStart(xISRAutoReloadTimer, portMAX_DELAY) != pdPASS)
    {
        printf("ISR Timer timer start failed\r\n");
    }
    /*不使能自动重载*/
    xISROneShotTimer = xTimerCreate("ISR OS",
                                    0xFFFF,                             
                                    pdFALSE,   
                                    (void *)NULL,
                                    prvISROneShotTimerCallback);
    if (xTimerStart(xISROneShotTimer, portMAX_DELAY) != pdPASS)
    {
        printf("ISR OS timer start failed\r\n");
    }
}

/**
 * @brief 定时器自动重载回调函数。
 * 该函数在定时器到期时被调用，用于处理定时器事件。它会更新对应定时器的计数器，
 * 并打印到期次数。对于定时器0，可根据标志位决定是否停止定时器。
 * @param pxExpiredTimer 指向到期定时器的句柄，类型为 TimerHandle_t。
 * @note
 * - 函数首先获取定时器的ID，并检查其有效性（ID需小于等于 configTIMER_QUEUE_LENGTH + 1）。
 * - 对于有效的ID，增加 ucAutoReloadTimerCounters 数组中对应索引的计数器值，并打印信息。
 * - 如果定时器ID为0且全局变量 ucIsStopNeededInTimerZeroCallback 设置为 pdTRUE，
 *   则停止该定时器并将 ucIsStopNeededInTimerZeroCallback 重置为 pdFALSE。
 * - 此函数为静态函数，仅限内部使用。
 */
static void prvAutoReloadTimerCallback(TimerHandle_t pxExpiredTimer)
{
    size_t uxTimerID;
    uxTimerID = (size_t)pvTimerGetTimerID(pxExpiredTimer);

    if(uxTimerID <= (configTIMER_QUEUE_LENGTH + 1))
    {
        (ucAutoReloadTimerCounters[ uxTimerID ])++;
        //printf("Timer %d expired %d times\r\n", uxTimerID, ucAutoReloadTimerCounters[uxTimerID]);

        /*如果ucIsStopNeededInTimerZeroCallback置为1, 停止定时器0*/
        if( (uxTimerID == (size_t) 0) && (ucIsStopNeededInTimerZeroCallback == (uint8_t) pdTRUE))
        {
            xTimerStop( pxExpiredTimer, tmrdemoDONT_BLOCK);
            ucIsStopNeededInTimerZeroCallback = (uint8_t)pdFALSE;
        }
    }
}

static void prvISRAutoReloadTimerCallback(TimerHandle_t pxExpiredTimer)
{
    ( void ) pxExpiredTimer;

    ucISRAutoReloadTimerCounter++;
}

static void prvISROneShotTimerCallback(TimerHandle_t pxExpiredTimer)
{
    ( void ) pxExpiredTimer;

    ucISROneShotTimerCounter++;
}


/********************************************************************


                            调试函数注册


*******************************************************************/
BaseType_t timerstatus_handle(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
    BaseType_t xReturn = pdFALSE;
    /*查看所有timers的信息*/
    const char * const pcHeader = "Timer   Name    Period   Status   #\r\n************************************************\r\n";
    strcpy(pcWriteBuffer, pcHeader);
    vTimerListInfo(pcWriteBuffer + strlen(pcHeader), xWriteBufferLen - strlen(pcHeader));
    return xReturn;
}
CMD_REGISTER_BASE(timerstatus, timerstatus: displays all timers information);








