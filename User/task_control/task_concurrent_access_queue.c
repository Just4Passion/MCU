


#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"


/******************************************************************
 *                              宏定义
 ****************************************************************/
#define intqHIGHER_PRIORITY          (configMAX_PRIORITIES - 2)
#define intqLOWER_PRIORITY               (tskIDLE_PRIORITY)

#define intqHIGH_PRIORITY_TASK1          ((UBaseType_t)1)
#define intqHIGH_PRIORITY_TASK2          ((UBaseType_t)2)
#define intqLOW_PRIORITY_TASK            ((UBaseType_t)3)
#define intqFIRST_INTERRUPT              ((UBaseType_t)4)
#define intqSECOND_INTERRUPT             ((UBaseType_t)5)
#define intqQUEUE_LENGTH                 ((UBaseType_t)10)

#define intqNUM_VALUES_TO_LOG            (200)
#define intqMIN_ACCEPTABLE_TASK_COUNT    (5)
#define intqVALUE_OVERRUN                (50)

#define intqONE_TICK_DELAY               (1)
#define intqSHORT_DELAY                  (140)


/******************************************************************
 *                              宏操作
 ****************************************************************/
#define timerNORMALLY_EMPTY_TX()                                                            \
    if (xQueueIsQueueFullFromISR(xNormallyEmptyQueue) != pdTRUE)                            \
    {                                                                                       \
        UBaseType_t uxSavedInterruptStatus;                                                 \
        uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();                             \
        {                                                                                   \
            uxValueForNormallyEmptyQueue++;                                                 \
            if (xQueueSendFromISR(xNormallyEmptyQueue,                                      \
                    (void * )&uxValueForNormallyEmptyQueue, &xHigherPriorityTaskWoken) != pdPASS)    \
            {                                                                               \
                uxValueForNormallyEmptyQueue--;                                             \
            }                                                                               \
        }                                                                                   \
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);                                 \
    }                                                                                       \

#define timerNORMALLY_FULL_TX()                                                                                                         \
    if(xQueueIsQueueFullFromISR(xNormallyFullQueue) != pdTRUE)                                                                          \
    {                                                                                                                                   \
        UBaseType_t uxSavedInterruptStatus;                                                                                             \
        uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();                                                                         \
        {                                                                                                                               \
            uxValueForNormallyFullQueue++;                                                                                              \
            if(xQueueSendFromISR(xNormallyFullQueue, (void *)&uxValueForNormallyFullQueue, &xHigherPriorityTaskWoken) != pdPASS)        \
            {                                                                                                                           \
                uxValueForNormallyFullQueue--;                                                                                          \
            }                                                                                                                           \
        }                                                                                                                               \
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);                                                                             \
    }

#define timerNORMALLY_EMPTY_RX()                                                                       \
    if(xQueueReceiveFromISR(xNormallyEmptyQueue, &uxRxedValue, &xHigherPriorityTaskWoken) != pdPASS)   \
    {                                                                                                   \
        prvQueueAccessLogError(__LINE__);                                                               \
    }                                                                                                   \
    else                                                                                                \
    {                                                                                                   \
        prvRecordValue_NormallyEmpty(uxRxedValue, intqSECOND_INTERRUPT);                                \
    }

#define timerNORMALLY_FULL_RX()                                                                         \
    if(xQueueReceiveFromISR(xNormallyFullQueue, &uxRxedValue, &xHigherPriorityTaskWoken) == pdPASS)     \
    {                                                                                                   \
        prvRecordValue_NormallyFull(uxRxedValue, intqSECOND_INTERRUPT);                                 \
    }      



/******************************************************************
 *                              变量声明
 ****************************************************************/
/*任务并发访问队列: 任务, 中断等*/

/*任务句柄*/
TaskHandle_t xHighPriorityNormallyEmptyTask1;
TaskHandle_t xHighPriorityNormallyEmptyTask2; 
TaskHandle_t xHighPriorityNormallyFullTask1;
TaskHandle_t xHighPriorityNormallyFullTask2;

/*队列*/
static QueueHandle_t xNormallyEmptyQueue;
static QueueHandle_t xNormallyFullQueue;

static volatile UBaseType_t uxValueForNormallyEmptyQueue = 0;
static volatile UBaseType_t uxValueForNormallyFullQueue = 0;

/*记录接收到的数据*/
static uint8_t ucNormallyEmptyReceivedValues[intqNUM_VALUES_TO_LOG] = {0};
static uint8_t ucNormallyFullReceivedValues[intqNUM_VALUES_TO_LOG] = {0};

/*错误标志*/
static volatile UBaseType_t xErrorLine = ( UBaseType_t ) 0;
static BaseType_t xErrorStatus = pdPASS;

/******************************************************************
 *                              函数声明
 ****************************************************************/

 static void prvQueueAccessLogError(UBaseType_t uxLine);
 static void prvRecordValue_NormallyFull(UBaseType_t uxValue, UBaseType_t uxSource);
 static void prvRecordValue_NormallyEmpty(UBaseType_t uxValue, UBaseType_t uxSource);
 static void prvHigherPriorityNormallyEmptyTask(void * pvParameters);
 static void prvLowerPriorityNormallyEmptyTask(void * pvParameters);
 static void vInitialiseTimerForIntQueueTest();

/*******************************************************************************
 * 
 * 两个高优先级任务: 不断地从队列中取数据
 * 一个低优先级任务:
 *          尝试从队列中取数据, 取不到就提升自己的优先级到最高, 然后往队列中发送数据
 *          当队列为空的情况, 高优先级任务被阻塞, 低优先级任务有机会执行
 *  
 * 
 *******************************************************************************/
void vStartInterruptQueueTasks(void)
{
    xTaskCreate(prvHigherPriorityNormallyEmptyTask, "H1QRx", 
        configMINIMAL_STACK_SIZE, (void*)intqHIGH_PRIORITY_TASK1, 
        intqHIGHER_PRIORITY, &xHighPriorityNormallyEmptyTask1);
    xTaskCreate(prvHigherPriorityNormallyEmptyTask, "H2QRx", 
        configMINIMAL_STACK_SIZE, (void*)intqHIGH_PRIORITY_TASK2, 
        intqHIGHER_PRIORITY, &xHighPriorityNormallyEmptyTask2);
    xTaskCreate(prvLowerPriorityNormallyEmptyTask, "L1QRx", 
        configMINIMAL_STACK_SIZE, NULL, 
        intqLOWER_PRIORITY, NULL);
    #if 0
    xTaskCreate(NULL, "H1QTx", 
        configMINIMAL_STACK_SIZE, (void*)intqHIGH_PRIORITY_TASK1, 
        intqHIGHER_PRIORITY, &xHighPriorityNormallyFullTask1);
    xTaskCreate(NULL, "H2QTx", 
        configMINIMAL_STACK_SIZE, (void*)intqHIGH_PRIORITY_TASK2, 
        intqHIGHER_PRIORITY, &xHighPriorityNormallyFullTask2);
    xTaskCreate( NULL, "L2QRx", 
        configMINIMAL_STACK_SIZE, NULL, intqLOWER_PRIORITY, NULL );
    #endif
    /*创建两个队列*/
    xNormallyEmptyQueue = xQueueCreate(intqQUEUE_LENGTH, (UBaseType_t)sizeof(UBaseType_t));
    xNormallyFullQueue = xQueueCreate(intqQUEUE_LENGTH, (UBaseType_t)sizeof(UBaseType_t));

    /*加入到注册列表中, 方便调试*/
    vQueueAddToRegistry(xNormallyFullQueue, "NormallyFull");
    vQueueAddToRegistry(xNormallyEmptyQueue, "NormallyEmpty");
}


/*

*/
static void prvHigherPriorityNormallyEmptyTask(void * pvParameters)
{
    UBaseType_t uxRxed;
    UBaseType_t ux;
    UBaseType_t uxTask1, uxTask2;
    UBaseType_t uxInterrupts; 
    UBaseType_t uxErrorCount1 = 0, uxErrorCount2 = 0;


    if ((UBaseType_t)pvParameters == intqHIGH_PRIORITY_TASK1)
    {
        /*启动定时器任务*/
    }
    for (;;)
    {
        /*中断向队列发送数据, 任务从队列接收数据, 得看中断发送了什么数据*/
        if (xQueueReceive(xNormallyEmptyQueue, &uxRxed, intqSHORT_DELAY) != pdPASS)
        {
            prvQueueAccessLogError(__LINE__);
        }
        else
        {
            /*在散列表中记录接收的数据的任务参数*/
            prvRecordValue_NormallyEmpty(uxRxed, (UBaseType_t)pvParameters);
        }

        /*强制触发任务切换: 如果队列中只有自己是就绪, 同时自己的优先级最高, 还是会调用到自己
        所以这种方式是确保: 1. 高优先级被调度  2. 同级别的其他任务被调度
        */
        taskYIELD();
        
        if ((UBaseType_t)pvParameters == intqHIGH_PRIORITY_TASK1)
        {
            if (uxValueForNormallyEmptyQueue > (intqNUM_VALUES_TO_LOG + intqVALUE_OVERRUN))
            {
                /*挂起Task2, 只有任务1有统一的权限*/
                vTaskSuspend(xHighPriorityNormallyEmptyTask2);

                uxTask1 = 0;
                uxTask2 = 0;
                uxInterrupts = 0;

                /*验证 TASK1、TASK2 和中断的贡献值数量*/
                for (ux = 0; ux < intqNUM_VALUES_TO_LOG; ux++)
                {
                    /*遍历记录的数据, 看看Task1和Task2分别从队列中接收到了多少次数据*/
                    if (ucNormallyEmptyReceivedValues[ux] == 0)
                    {
                        /*等于0, 说明这个值还没被接收到过*/
                        prvQueueAccessLogError(__LINE__);
                    }
                    else
                    {
                        /*任务1接收的数据 */
                        if (ucNormallyEmptyReceivedValues[ux] == intqHIGH_PRIORITY_TASK1)
                        {
                            uxTask1++;
                        }
                        /*任务2接收的数据*/
                        else if (ucNormallyEmptyReceivedValues[ux] == intqHIGH_PRIORITY_TASK2)
                        {
                            uxTask2++;
                        }
                        /*中断发送的数据*/
                        else if (ucNormallyEmptyReceivedValues[ux] == intqSECOND_INTERRUPT)
                        {
                            uxInterrupts++;
                        }
                    }
                }
                
                /*最少要接收5个: 为什么是5个, 满是200个*/
                if (uxTask1 < intqMIN_ACCEPTABLE_TASK_COUNT)
                {
                    printf("HighTask1 receive messages just %d\n", uxTask1);
                }
                if (uxTask2 < intqMIN_ACCEPTABLE_TASK_COUNT)
                {
                    printf("HighTask2 receive messages just %d\n", uxTask2);
                }
                if (uxInterrupts == 0)
                {
                    printf("Interrupt receive messages just %d\n", uxInterrupts);
                }

                memset(ucNormallyEmptyReceivedValues, 0x00, sizeof(ucNormallyEmptyReceivedValues));
                portENTER_CRITICAL();
                {
                    uxValueForNormallyEmptyQueue = 0;
                }
                portEXIT_CRITICAL();

                /*把自己挂起来*/
                vTaskSuspend(NULL);
                /*唤醒任务2*/
                vTaskResume(xHighPriorityNormallyEmptyTask2);
            }

        }

    }
}

/*
从队列中接收数据, 接收不到就提高自己的优先级, 然后向队列中发送数据
*/
static void prvLowerPriorityNormallyEmptyTask(void * pvParameters)
{
    UBaseType_t uxValue, uxRxed;

    for (;;)
    {
        /*高优先级任务被挂起, 这个时候低优先级有机会去执行*/
        if (xQueueReceive(xNormallyEmptyQueue, &uxRxed, intqONE_TICK_DELAY) != errQUEUE_EMPTY)
        {
            prvRecordValue_NormallyEmpty(uxRxed, intqLOW_PRIORITY_TASK);

            if (eTaskGetState(xHighPriorityNormallyEmptyTask1) != eSuspended)
            {
                prvQueueAccessLogError(__LINE__);
            }
            /*唤醒高优先级任务1: 低优先级唤醒高优先, 是谁阻塞了高优先级任务*/
            vTaskResume(xHighPriorityNormallyEmptyTask1);
        }
        else
        {
            /*在接收不到数据的时候, 提高自己的优先级到最高*/
            vTaskPrioritySet(NULL, intqHIGHER_PRIORITY + 1);

            portENTER_CRITICAL();
            {
                /*不断地提升自己的优先级, 提升一次就加1*/
                uxValueForNormallyEmptyQueue++;
                uxValue = uxValueForNormallyEmptyQueue;
            }
            portEXIT_CRITICAL();

            /*然后把数据发送到队列中: 收到队列中数据的高优先级任务也只会记录合法的数据*/
            if( xQueueSend(xNormallyEmptyQueue, &uxValue, portMAX_DELAY) != pdPASS)
            {
                prvQueueAccessLogError(__LINE__);
            }
            /*降低自己的优先级*/
            vTaskPrioritySet(NULL, intqLOWER_PRIORITY);
        }
    }
}


static void prvRecordValue_NormallyEmpty(UBaseType_t uxValue, UBaseType_t uxSource)
{
    if( uxValue < intqNUM_VALUES_TO_LOG )
    {
        /*避免重复记录*/
        if( ucNormallyEmptyReceivedValues[uxValue] != 0x00 )
        {
            prvQueueAccessLogError( __LINE__ );
        }
        /*记录接收到的数据*/
        ucNormallyEmptyReceivedValues[uxValue] = ( uint8_t ) uxSource;
    }
}

static void prvRecordValue_NormallyFull(UBaseType_t uxValue,
                                         UBaseType_t uxSource)
{
    if(uxValue < intqNUM_VALUES_TO_LOG)
    {
        if(ucNormallyFullReceivedValues[ uxValue ] != 0x00)
        {
            prvQueueAccessLogError(__LINE__);
        }

        ucNormallyFullReceivedValues[uxValue] = (uint8_t)uxSource;
    }
}


static void prvQueueAccessLogError(UBaseType_t uxLine)
{
    xErrorLine = uxLine;
    xErrorStatus = pdFAIL;
}


BaseType_t xFirstTimerHandler()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    UBaseType_t uxRxedValue;
    static UBaseType_t uxNextOperation = 0;

    uxNextOperation++;

    if (uxNextOperation & (UBaseType_t)0x01)
    {
        /*向Empty队列发送数据*/
        timerNORMALLY_EMPTY_TX();
        timerNORMALLY_EMPTY_TX();
        timerNORMALLY_EMPTY_TX();
    }
    else
    {
        /*从Full队列接收数据*/
        timerNORMALLY_FULL_RX();
        timerNORMALLY_FULL_RX();
        timerNORMALLY_FULL_RX();
    }
    return xHigherPriorityTaskWoken;
}

BaseType_t xSecondTimerHandler( void )
{
    UBaseType_t uxRxedValue;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static UBaseType_t uxNextOperation = 0;

    uxNextOperation++;

    if( uxNextOperation & ( UBaseType_t ) 0x01 )
    {
        /*先发送, 再收: 定时器任务的优先级是最高的, 但是定时器任务里面有一个等待队列, 等待超时的定时器*/
        timerNORMALLY_EMPTY_TX();
        timerNORMALLY_EMPTY_TX();
        
        timerNORMALLY_EMPTY_RX();
        timerNORMALLY_EMPTY_RX();
    }
    else
    {
        timerNORMALLY_FULL_RX();
        timerNORMALLY_FULL_TX();
        timerNORMALLY_FULL_TX();
        timerNORMALLY_FULL_TX();
    }

    return xHigherPriorityTaskWoken;
}



static void vInitialiseTimerForIntQueueTest()
{
    TimerHandle_t xFirstTimer;
    TimerHandle_t xSendTimer;
    xFirstTimer = xTimerCreate("FirstTimer", 
                            300, pdTRUE, (void *)1, 
                            xFirstTimerHandler);
    if (xFirstTimer == NULL)
    {
        printf("Failed to create First timer\n");
    }
    xSendTimer = xTimerCreate("SendTimer", 
                            200, pdTRUE, (void *)2,
                            xSecondTimerHandler);
    if (xSendTimer == NULL)
    {
        printf("Failed to create Send timer\n");
    }
    if (xTimerStart(xFirstTimer, portMAX_DELAY) != pdPASS)
    {
        printf("Failed to start First timer\n");
    }
    if (xTimerStart(xSendTimer, portMAX_DELAY) != pdPASS)
    {
        printf("Failed to start Send timer\n");
    }
}


