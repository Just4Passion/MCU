

#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/************************************************************
 *                     Demo演示了优先级继承
 * 当低优先级阻塞高优先级任务时, 低优先级会继承高优先级任务的优先级
 * 知道低优先级释放资源后, 继承的优先级会恢复
 * 高优先级得到资源后, 优先执行
 ************************************************************/

#define intsemMAX_COUNT                         3
#define intsemMASTER_PRIORITY                   (tskIDLE_PRIORITY)
#define intsemSLAVE_PRIORITY                    (tskIDLE_PRIORITY + 1)

#define intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS    (100)
#define intsemNO_BLOCK                          0

/************************************************************
 *                      函数声明
 ************************************************************/
static void vInterruptMutexSlaveTask(void * pvParameters);
static void vInterruptMutexMasterTask(void * pvParameters);
static void prvTakeAndGiveInTheSameOrder(void);


/************************************************************
 *                      变量定义
 ************************************************************/
static volatile BaseType_t xErrorDetected = pdFALSE;

static volatile uint32_t ulMasterLoops = 0, ulCountingSemaphoreLoops = 0;

/*从任务的句柄*/
static TaskHandle_t xSlaveHandle;
/*两个互斥量; 一个计数信号量(资源计数)*/
static SemaphoreHandle_t xISRMutex = NULL;
static SemaphoreHandle_t xISRCountingSemaphore = NULL;
static SemaphoreHandle_t xMasterSlaveMutex = NULL;

const TickType_t xInterruptGivePeriod = pdMS_TO_TICKS(intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS);

static BaseType_t xOkToGiveMutex = pdFALSE, xOkToGiveCountingSemaphore = pdFALSE;

void vStartInterruptSemaphoreTasks(void)
{
    /************************************************
     * 创建两个互斥量, 一个主, 一个从
     * 主任务从终端获取互斥量, 然后分享互斥量给从任务
     ***********************************************/
    xISRMutex = xSemaphoreCreateMutex();
    xMasterSlaveMutex = xSemaphoreCreateMutex();
    xISRCountingSemaphore = xSemaphoreCreateCounting(intsemMAX_COUNT, 0);

    /*创建任务*/
    /*从任务优先级比主任务高, 当主任务唤醒从任务后, 切换到从任务运行*/
    xTaskCreate(vInterruptMutexSlaveTask, "Int Master", configMINIMAL_STACK_SIZE, NULL, intsemSLAVE_PRIORITY, &xSlaveHandle);
    xTaskCreate(vInterruptMutexSlaveTask, "Int Slave", configMINIMAL_STACK_SIZE, NULL, intsemMASTER_PRIORITY, NULL);
    xTaskCreate(NULL, "Int Count", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
}

static void vInterruptMutexMasterTask(void * pvParameters)
{
    for (;;)
    {
        prvTakeAndGiveInTheSameOrder();
        ulMasterLoops++;
        vTaskDelay(intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS);
        /*反转: 先释放阻塞从任务的信号量, 然后再释放ISR信号量*/

        ulMasterLoops++;
        vTaskDelay(intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS);
    }
}

static void prvTakeAndGiveInTheSameOrder(void)
{
    /*上锁*/
    if( xSemaphoreTake(xMasterSlaveMutex, intsemNO_BLOCK) != pdPASS)
    {

    }
    /*唤醒从任务 - 唤醒后切换到从任务运行*/
    vTaskResume(xSlaveHandle);
    /*切换后, 在回来时, 集成了从任务的优先级*/
    printf("Lock xMasterSlaveMutex, master task prioriy is %d\r\n", uxTaskPriorityGet(NULL));
    /*获取xISRMutex - 成功*/
    xOkToGiveMutex = pdTRUE;
    if(xSemaphoreTake(xISRMutex, (xInterruptGivePeriod * 2)) != pdPASS)
    {
        printf("Master failed to take xISRMutex, first time\n");
    }
    xOkToGiveMutex = pdFALSE;
    /*再获取xISRMutex - 失败, 这里会被阻塞, 让其他任务运行*/
    if(xSemaphoreTake(xISRMutex, (xInterruptGivePeriod * 2)) != pdFAIL)
    {
        printf("Master failed to take xISRMutex, second time\n");
    }
    printf("No free xMasterSlaveMutex, master task prioriy is %d\r\n", uxTaskPriorityGet(NULL));

    if (xSemaphoreGive(xISRMutex) != pdPASS)
    {
        printf("Master failed to give xISRMutex\n");
    }

    if(xSemaphoreGive( xMasterSlaveMutex ) != pdPASS)
    {
        printf("Master failed to give xMasterSlaveMutex\n");
    }
    printf("Free xMasterSlaveMutex, master task prioriy is %d\r\n", uxTaskPriorityGet(NULL));
    /*重置队列 - 清空所有数据*/
    xQueueReset(xISRMutex);
}

static void vInterruptMutexSlaveTask(void * pvParameters)
{
    for(;;)
    {
        /*挂起自己, 等待主任务唤醒*/
        vTaskSuspend(NULL);

        /*获取互斥量 - 可以理解成上锁, 这里超时时间是很长的, 最大值, 当任务释放后, 从任务就自动获取了*/
        if( xSemaphoreTake(xMasterSlaveMutex, portMAX_DELAY) != pdPASS)
        {
            xErrorDetected = __LINE__;
            printf("Slave failed to take mutex\n");
        }

        /*释放互斥量 - 可以理解成解锁*/
        if( xSemaphoreGive(xMasterSlaveMutex) != pdPASS )
        {
            xErrorDetected = __LINE__;
            printf("Slave failed to give mutex\n");
        }
    }
}


static void vInterruptCountingSemaphoreTask(void * pvParameters)
{
    BaseType_t xCount;
    const TickType_t xDelay = pdMS_TO_TICKS(intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS) * (intsemMAX_COUNT + 1);

    for(;;)
    {
        if (uxQueueMessagesWaiting((QueueHandle_t)xISRCountingSemaphore) != 0)
        {

        }
        /*延迟, 让其他任务运行*/
        xOkToGiveCountingSemaphore = pdTRUE;
        vTaskDelay(xDelay);
        xOkToGiveCountingSemaphore = pdFALSE;
        /*中断任务会释放信号量, 这里获取信号量*/
        if (uxQueueMessagesWaiting((QueueHandle_t)xISRCountingSemaphore) != intsemMAX_COUNT)
        {

        }
        /*当中断释放了所有计数信号量, 队列就满了*/
        if( uxQueueSpacesAvailable((QueueHandle_t)xISRCountingSemaphore) != 0)
        {

        }
        ulCountingSemaphoreLoops++;
        /*取出全部信号量*/
        while( xSemaphoreTake(xISRCountingSemaphore, 0) == pdPASS )
        {
            xCount++;
        }
        if (xCount != intsemMAX_COUNT)
        {

        }

        /*提高优先级, 让任务能够在中断给了信号量时立即运行*/
        vTaskPrioritySet( NULL, configMAX_PRIORITIES - 1);
        xOkToGiveCountingSemaphore = pdTRUE;
        xSemaphoreTake(xISRCountingSemaphore, portMAX_DELAY);
        xSemaphoreTake(xISRCountingSemaphore, portMAX_DELAY);
        xOkToGiveCountingSemaphore = pdFALSE;
        vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
        ulCountingSemaphoreLoops++;
    }

}

void vInterruptSemaphorePeriodTest(void)
{
    static TickType_t xLastGiveTime = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    TickType_t xTimeNow;

    xTimeNow = xTaskGetTickCountFromISR();

    if ((xTimeNow - xLastGiveTime) >= pdMS_TO_TICKS(intsemINTERRUPT_MUTEX_GIVE_PERIOD_MS))
    {
        if(xOkToGiveMutex != pdFALSE)
        {
            xSemaphoreGiveFromISR(xISRMutex, NULL);
            xSemaphoreGiveFromISR(xISRMutex, &xHigherPriorityTaskWoken);    //失败
        }
        if( xOkToGiveCountingSemaphore != pdFALSE )
        {
            xSemaphoreGiveFromISR(xISRCountingSemaphore, &xHigherPriorityTaskWoken);
        }
        xLastGiveTime = xTimeNow;
    }
}

