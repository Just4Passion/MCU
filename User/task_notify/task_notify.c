
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"


#define notifyNOTIFIED_TASK_STACK_SIZE    configMINIMAL_STACK_SIZE
#define notifyTASK_PRIORITY               (tskIDLE_PRIORITY)

#define notifyUINT32_MAX                      ((uint32_t )0xffffffff)

#define notifySUSPENDED_TEST_TIMER_PERIOD     pdMS_TO_TICKS(50)

/****************************************************
 * 
 *                       函数声明
 * 
 *****************************************************/
static void prvNotifiedTask(void * pvParameters);
static UBaseType_t prvRand(void);
static void prvSingleTaskTests(void);
static void prvSuspendedTaskTimerTestCallback(TimerHandle_t xExpiredTimer);
static void prvNotifyingTimer(TimerHandle_t xNotUsed);

/****************************************************
 * 
 *                       变量声明
 * 
 *****************************************************/

static TaskHandle_t xTaskToNotify = NULL;
/*随机数*/
static size_t uxNextRand = 0;
/*全局错误状态*/
static BaseType_t xErrorStatus = pdPASS;

static volatile uint32_t ulNotifyCycleCount = 0;

/*发送定时器*/
static TimerHandle_t xTimer = NULL;

/**/
static uint32_t ulTimerNotificationsReceived = 0UL;
static uint32_t ulTimerNotificationsSent = 0UL;

void vStartTaskNotifyTask(void)
{
    xTaskCreate(prvNotifiedTask, "Task Notify", notifyNOTIFIED_TASK_STACK_SIZE, NULL, (notifyTASK_PRIORITY + 2), &xTaskToNotify);
    uxNextRand = (size_t) prvRand;
}



/*通知任务*/
static void prvNotifiedTask(void * pvParameters)
{
    const TickType_t xMaxPeriod = pdMS_TO_TICKS(90);
    const TickType_t xMinPeriod = pdMS_TO_TICKS(10);
    const TickType_t xDontBlock = 0;

    /*循环增加优先级*/
    const uint32_t ulCyclesToRaisePriority = 50UL;
    
    TickType_t xPeriod;

    /*既然是在任务里面运行的, 就会存在被挂起, 阻塞的可能*/
    prvSingleTaskTests();

    /*创建一个定时器发送通知*/
    xTimer = xTimerCreate("Notifier", xMaxPeriod, pdFALSE, NULL, prvNotifyingTimer);
    for(;;)
    {
        xPeriod = prvRand() % xMaxPeriod;
        if (xPeriod < xMinPeriod)
        {
            xPeriod = xMinPeriod;
        }
        xTimerChangePeriod(xTimer, xPeriod, portMAX_DELAY);
        xPeriod = prvRand() % xMaxPeriod;
        if (xPeriod < xMinPeriod)
        {
            xPeriod = xMinPeriod;
        }

        if(ulTaskNotifyTake( pdFALSE, xPeriod) != 0)
        {
            ulTimerNotificationsReceived++;
        }
        if( ulTaskNotifyTake(pdFALSE, xDontBlock) != 0)
        {
            ulTimerNotificationsReceived++;
        }
        ulTimerNotificationsReceived += ulTaskNotifyTake(pdTRUE, xPeriod);

        if((ulNotifyCycleCount % ulCyclesToRaisePriority) == 0)
        {

        }
    }
}

static void prvNotifyingTimer(TimerHandle_t xNotUsed)
{
    /*取值, 加*/
    xTaskNotifyGive(xTaskToNotify);
    taskENTER_CRITICAL();
    {
        ulTimerNotificationsSent++;
    }
    taskEXIT_CRITICAL();
}

static void prvSingleTaskTests(void)
{
    const TickType_t xTicksToWait = pdMS_TO_TICKS(100UL);
    BaseType_t xReturned;

    uint32_t ulNotifiedValue;
    uint32_t ulPreviousValue;
    uint32_t  ulLoop;
    uint32_t ulNotifyingValue;
    uint32_t ulExpectedValue;
    const uint32_t ulFirstNotifiedConst = 100001UL;
    const uint32_t ulSecondNotifiedValueConst = 5555UL;
    const uint32_t ulMaxLoops = 5UL;
    const uint32_t ulBit0 = 0x01UL, ulBit1 = 0x02UL;
    TimerHandle_t xSingleTaskTimer;

    TickType_t xTimeOnEntering;
    /********************************************************
     *                  测试等待通知超时
     ********************************************************/
    xTimeOnEntering = xTaskGetTickCount();
    /*等待通知*/
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0, &ulNotifiedValue, xTicksToWait);
    (void)xReturned;    //编译报警
    /*因为没有任务会通知它, 所以会超时*/
    if((xTaskGetTickCount() - xTimeOnEntering) < xTicksToWait)
    {
        xErrorStatus = pdFAIL;
        printf("1 - xTaskNotifyWait: Failed to receive a notification\r\n");
    }

    /********************************************************
     *                  测试通知成功
     ********************************************************/
    /*如果这个通知的对象(是任务自己)没有接收通知, 则覆盖它的通知值*/
    xReturned = xTaskNotifyAndQuery( xTaskToNotify, ulFirstNotifiedConst, eSetValueWithoutOverwrite, &ulPreviousValue );
    if (xReturned == pdPASS)
    {
        printf("2 - xTaskNotifyAndQuery: Success to send notification\r\n");
    }
    /*再次等待通知, 由于上面通知了自己, 这次应该立即返回*/
    xTimeOnEntering = xTaskGetTickCount();
    xReturned = xTaskNotifyWait( notifyUINT32_MAX, 0, &ulNotifiedValue, xTicksToWait );
    if((xTaskGetTickCount() - xTimeOnEntering) >= xTicksToWait)
    {
        xErrorStatus = pdFAIL;
        printf("3 - xTaskNotifyWait: Failed to receive a notification, should not time out\r\n");
    }

    ulNotifyCycleCount++;

    /********************************************************
     *                  测试两次通知 non-overwrite
     ********************************************************/
    /*测试非覆盖通知: 如果没有通知被接收, 就覆盖它的通知值*/
    xReturned = xTaskNotify(xTaskToNotify, ulFirstNotifiedConst, eSetValueWithoutOverwrite);
    if (xReturned == pdPASS)
    {
        printf("4 - xTaskNotify: Success to send notification\r\n");
    }
    /*再次通知, 此时使用非覆盖, 第二个值就不会覆盖了*/
    xReturned = xTaskNotify(xTaskToNotify, ulSecondNotifiedValueConst, eSetValueWithoutOverwrite);
    if (xReturned == pdFAIL)
    {
        printf("5 - xTaskNotify: Failed to send notification\r\n");
    }
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0, &ulNotifiedValue, 0 );
    if (ulNotifiedValue == ulFirstNotifiedConst)
    {
        printf("6 - xTaskNotify: Two notifications expected, only first received\r\n");
    }

    /********************************************************
     *                  测试两次通知 overwrite
     ********************************************************/
    /*测试非覆盖通知: 如果没有通知被接收, 就覆盖它的通知值*/
    xReturned = xTaskNotify(xTaskToNotify, ulFirstNotifiedConst, eSetValueWithOverwrite);
    if (xReturned == pdPASS)
    {
        printf("7 - xTaskNotify: Success to send notification\r\n");
    }
    /*再次通知, 此时使用非覆盖, 第二个值就不会覆盖了*/
    xReturned = xTaskNotify(xTaskToNotify, ulSecondNotifiedValueConst, eSetValueWithOverwrite);
    if (xReturned == pdPASS) 
    {
        printf("8 - xTaskNotify: Success to send notification\r\n");
    }
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0, &ulNotifiedValue, 0 );
    if (ulNotifiedValue == ulSecondNotifiedValueConst)
    {
        printf("9 - xTaskNotify: Two notifications expected, second overwrite\r\n");
    }

    /********************************************************
     *                  测试仅通知 eNoAction
     ********************************************************/
    xReturned = xTaskNotify(xTaskToNotify, ulFirstNotifiedConst, eNoAction);
    xReturned = xTaskNotifyWait( notifyUINT32_MAX, 0, &ulNotifiedValue, 0 );
    if (ulNotifiedValue == ulSecondNotifiedValueConst)
    {
        printf("10 - xTaskNotify: eNoAction have no action\r\n");
    }

    /********************************************************
     *                  测试通知计数 eIncrement
     ********************************************************/
    for( ulLoop = 0; ulLoop < ulMaxLoops; ulLoop++ )
    {
        xReturned = xTaskNotify(xTaskToNotify, 0, eIncrement );
    }
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0, &ulNotifiedValue, 0);
    if (ulNotifiedValue == (ulSecondNotifiedValueConst + ulMaxLoops))
    {
        printf("11 - xTaskNotify: Notification count incremented as expected\r\n");
    }

    /*没有任何通知待处理*/
    xReturned = xTaskNotifyWait(0, 0, &ulNotifiedValue, 0);
    if (xReturned == pdFAIL)
    {
        printf("12 - xTaskNotify: No notifications pending\r\n");
    }


    /********************************************************
     *                  测试通知标志位 eSetBits
     ********************************************************/
    ulNotifyingValue = 0x01;
    ulLoop = 0;
    xTaskNotifyWait(notifyUINT32_MAX, 0, &ulNotifiedValue, 0);
    do
    {
        xTaskNotify(xTaskToNotify, ulNotifyingValue, eSetBits);
        xReturned = xTaskNotifyWait(0, 0, &ulNotifiedValue, 0);
        if (xReturned == pdFAIL)
        {
            break;
        }
        ulLoop++;
        ulNotifyingValue <<= 1UL;
    }while(ulNotifiedValue != notifyUINT32_MAX);
    if(ulLoop != 32)
    {
        printf("12 - xTaskNotify: Failed to set all bits\r\n");
    }

    /********************************************************
     *           测试通知等待超时, 进入清除位和退出清除位
     ********************************************************/
    xReturned = xTaskNotifyWait(ulBit0, ulBit1, &ulNotifiedValue, xTicksToWait);
    /*超时了, 但是进去的时候就把状态为清零了; 因为超时, 出来的时候没有清零状态位*/
    printf("13 - xTaskNotifyWait: ulBit0 = 0x%x, ulNotifiedValue = 0x%x\r\n", ulBit0, ulNotifiedValue);


    /********************************************************
     *           测试通知 eNoAction
     ********************************************************/
    xTaskNotify(xTaskToNotify, notifyUINT32_MAX, eNoAction);
    xReturned = xTaskNotifyWait( 0x00UL, 0x00UL, &ulNotifiedValue, 0);
    if (xReturned == pdPASS)
    {
        printf("14 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);
    }

    /********************************************************
     *           测试通知 eNoAction, 收到通知, 返回进去的时候被清除的值, 告诉你清除成功了
     ********************************************************/
    xTaskNotify(xTaskToNotify, 0, eNoAction);
    xReturned = xTaskNotifyWait(0x00, ulBit1, &ulNotifiedValue, 0); //返出来的是它刚进去的时候被清除的值
    printf("15 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue); //退出时值被清除了, 返回0xfffffffe, 被修改成0xfffffffc

    /********************************************************
     *           测试通知 Query查到的是之前的值
     ********************************************************/
    xTaskNotifyAndQuery(xTaskToNotify, 0x00, eSetBits, &ulPreviousValue);  //应该返回0xfffffffc
    printf("16 - xTaskNotifyAndQuery: ulPreviousValue = 0x%x\r\n", ulPreviousValue);
    xTaskNotifyWait( 0x00, notifyUINT32_MAX, &ulNotifiedValue, 0);
    printf("17 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);    //应该返回0xfffffffc


    xTaskNotifyAndQuery(xTaskToNotify, 0x00, eSetBits, &ulPreviousValue);
    printf("18 - xTaskNotifyWait: ulPreviousValue = 0x%x\r\n", ulPreviousValue);    //应该返回0


    /********************************************************
     *           测试通知 eSetBits
     ********************************************************/
    ulExpectedValue = 0;
    for( ulLoop = 0x01; ulLoop < 0x80UL; ulLoop <<= 1UL)
    {
        xTaskNotifyAndQuery(xTaskToNotify, ulLoop, eSetBits, &ulPreviousValue);
        ulExpectedValue |= ulLoop;
        printf("19 - xTaskNotifyAndQuery: ulPreviousValue = 0x%x\r\n", ulPreviousValue);
    }
    /*如果没有收到消息, 就会使用notifyUINT32_MAX清空当前通知值, 然后等待接收通知
    如果已经接收到通知, ulNotifiedValue的值就是接收到的通知值*/
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0x00, &ulNotifiedValue, 0);
    printf("20 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);

    /********************************************************
     *           测试通知 eSetValueWithoutOverwrite, 没有收到通知覆盖; 已经收到通知不覆盖
     ********************************************************/
    /*已经wait了, 所以当前的状态是wait_通知*/
    xTaskNotifyAndQuery(xTaskToNotify, ulFirstNotifiedConst, eSetValueWithoutOverwrite, &ulPreviousValue);
    printf("21 - xTaskNotifyAndQuery: ulPreviousValue = 0x%x\r\n", ulPreviousValue);
    xReturned = xTaskNotifyWait(notifyUINT32_MAX, 0x00, &ulNotifiedValue, 0);
    printf("22 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);

    /********************************************************
     *           测试通知 
     *              非"接收通知"状态xTaskNotifyWait把ulNotifiedValue值清零测试
     *              xTaskNotifyStateClear清空状态测试
     ********************************************************/
    /*先通知, 让它变成通知状态, 然后without-overwrite设置值, 此时肯定设置不成功*/
    xTaskNotifyWait(notifyUINT32_MAX, 0x00, NULL, 0);
    xTaskNotifyAndQuery(xTaskToNotify, 0xff, eSetValueWithoutOverwrite, &ulPreviousValue);
    printf("23 - xTaskNotifyAndQuery: ulPreviousValue = 0x%x\r\n", ulPreviousValue);    //0x00  
    /*清空状态*/
    xTaskNotifyStateClear(xTaskToNotify);
    xTaskNotifyAndQuery(xTaskToNotify, 0xffff, eSetValueWithoutOverwrite, &ulPreviousValue);
    printf("24 - xTaskNotifyAndQuery: ulPreviousValue = 0x%x\r\n", ulPreviousValue);    //0xff
    xTaskNotifyWait(notifyUINT32_MAX, 0x00, &ulNotifiedValue, 0);
    printf("25 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);    //0xffff
    /*清空值*/
    xReturned = ulTaskNotifyValueClear(xTaskToNotify, notifyUINT32_MAX);

    /********************************************************
     *           测试通知 使用定时器把任务挂起, 执行完通知之后, 把任务恢复
     ********************************************************/
    xSingleTaskTimer = xTimerCreate("SingleNotify", 
                                    notifySUSPENDED_TEST_TIMER_PERIOD, 
                                    pdFALSE, (void *)0, prvSuspendedTaskTimerTestCallback);
    ulNotifyCycleCount++;
    xTaskNotifyWait(notifyUINT32_MAX, 0, NULL, 0);
    /*提高任务优先级*/
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
    ulNotifiedValue = 0;
    /*启动定时器*/
    xTimerStart(xSingleTaskTimer, portMAX_DELAY);
    xReturned = xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);
    if (xReturned == pdFAIL)
    {
        printf("26 - xTaskNotifyWait: no notification, time out\r\n");
    }
    ulNotifyCycleCount++;
    /*启动定时器*/
    xTimerStart(xSingleTaskTimer, portMAX_DELAY);
    xReturned = xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);
    if (xReturned == pdPASS)
    {
        printf("27 - xTaskNotifyWait: ulNotifiedValue = 0x%x\r\n", ulNotifiedValue);
    }
    /*恢复任务优先级*/
    vTaskPrioritySet(NULL, notifyTASK_PRIORITY);
    xTimerDelete(xSingleTaskTimer, portMAX_DELAY);
    ulNotifyCycleCount++;
    /*清空值*/
    xTaskNotifyWait(notifyUINT32_MAX, 0, NULL, 0);
}

static void prvSuspendedTaskTimerTestCallback(TimerHandle_t xExpiredTimer)
{
    static uint32_t ulCallCount = 0;
    if( ulCallCount == 0 )
    {
        /*挂起任务*/
        vTaskSuspend( xTaskToNotify );
        if (eTaskGetState( xTaskToNotify ) == eSuspended )
        {
            printf("Timer suspend task, no notification\r\n");
        }
        /*恢复任务*/
        vTaskResume( xTaskToNotify );
    }
    else
    {
        /*挂起任务*/
        vTaskSuspend( xTaskToNotify );
        xTaskNotify( xTaskToNotify, ulCallCount, eSetValueWithOverwrite );
        if (eTaskGetState( xTaskToNotify ) == eSuspended )
        {
            printf("Timer suspend task, send notification, but not change eSuspended\r\n");
        }
        else
        {
            printf("Timer suspend task, send notification, then change eSuspended\r\n");
        }
        /*恢复任务*/
        vTaskResume( xTaskToNotify );
    }
    ulCallCount++;
}


static UBaseType_t prvRand(void)
{
    const size_t uxMultiplier = ( size_t ) 0x015a4e35, uxIncrement = ( size_t ) 1;

    /*产生伪随机数*/
    uxNextRand = (uxMultiplier * uxNextRand) + uxIncrement;
    return((uxNextRand >> 16) & (( size_t) 0x7fff));
}


